#!/usr/bin/env python3
"""Headless static checks for the QUAHOG_Unreal C++ module.

This file checks only ``QUAHOG_Unreal/Source/MountHope``. It does not read or
modify ``MountHope_Unreal``. A pass is a text-structure result: delimiter
balance, ``.generated.h`` placement, ``GENERATED_BODY``, own-header includes,
Build.cs module names, and ``UFUNCTION`` on dynamic-delegate targets.

WHAT THIS IS
------------
A sandbox-safe structural gate. It runs without Unreal Engine, without
UnrealBuildTool, and without a C++ compiler. It is not wired to
``.github/workflows/unreal-ci.yml`` (that workflow belongs to the other tree).

WHAT THIS IS *NOT*
------------------
This is **not** a compiler and **not** a substitute for building the module in
UE 5.8. UnrealHeaderTool code-generation (``UCLASS``/``GENERATED_BODY``/
``UPROPERTY`` -> ``*.generated.h``) and real type checking only happen in a
genuine engine build. A hand-written ``UnrealEngine`` shim was deliberately
*not* built: to compile at all it would have to ``#define`` UHT's machinery
away, at which point it would pass code that UBT rejects — false confidence,
which is worse than no gate. So this catches a specific, high-value subset of
*structural* errors that this compiler-less workflow keeps almost-hitting; it
says nothing about deeper type/semantic correctness. Always do a real editor
compile before shipping.

CHECKS (each is conservative — it skips rather than guesses when unsure, so a
green result never has false positives, and a red result is a genuine problem):

1. Delimiter balance — ``{}``, ``()``, ``[]`` balance per file, after stripping
   comments and string/char literals. (Necessary, not sufficient, but a cheap
   catch for the truncation/copy-paste errors this workflow is prone to.)
2. ``.generated.h`` include is present and is the *last* include in every
   reflection header, and its basename matches the file (a real UHT rule; a
   copy-pasted wrong ``X.generated.h`` is a hard build error).
3. Every ``UCLASS``/``USTRUCT``/``UINTERFACE`` has at least the expected number
   of ``GENERATED_BODY()`` macros.
4. Every ``X.cpp`` includes its own ``X.h`` (Unreal convention / IWYU).
5. Module-dependency hygiene — if the source uses a symbol/header that lives in
   a module that is *not* transitively guaranteed (EnhancedInput, GameplayTags,
   AIModule, NavigationSystem, UMG, Json, ChaosVehicles), that module must be
   listed in ``MountHope.Build.cs``.
6. Dynamic-delegate binding — every ``AddDynamic``/``RemoveDynamic`` target
   method must be declared ``UFUNCTION()`` (binding a non-UFUNCTION to a dynamic
   multicast delegate is a compile error).
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = PROJECT_ROOT / "Source" / "MountHope"
BUILD_CS = PROJECT_ROOT / "Source" / "MountHope" / "MountHope.Build.cs"


class Problem:
    def __init__(self, path: Path, message: str) -> None:
        self.path = path
        self.message = message

    def __str__(self) -> str:
        rel = self.path.relative_to(PROJECT_ROOT)
        return f"{rel}: {self.message}"


def strip_literals_and_comments(text: str) -> str:
    """Return ``text`` with // and /* */ comments and string/char literals
    replaced by spaces, so brace/paren counting isn't fooled by ``TEXT("(")``
    or ``'{'``. Newlines are preserved so nothing shifts across lines."""
    out = []
    i = 0
    n = len(text)
    state = "code"  # code | line_comment | block_comment | string | char
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if state == "code":
            if c == "/" and nxt == "/":
                state = "line_comment"
                out.append("  ")
                i += 2
                continue
            if c == "/" and nxt == "*":
                state = "block_comment"
                out.append("  ")
                i += 2
                continue
            if c == '"':
                state = "string"
                out.append(" ")
                i += 1
                continue
            if c == "'":
                state = "char"
                out.append(" ")
                i += 1
                continue
            out.append(c)
            i += 1
        elif state == "line_comment":
            if c == "\n":
                state = "code"
                out.append("\n")
            else:
                out.append(" ")
            i += 1
        elif state == "block_comment":
            if c == "*" and nxt == "/":
                state = "code"
                out.append("  ")
                i += 2
            else:
                out.append("\n" if c == "\n" else " ")
                i += 1
        elif state in ("string", "char"):
            closing = '"' if state == "string" else "'"
            if c == "\\":  # escape: skip next char
                out.append("  ")
                i += 2
                continue
            if c == closing:
                state = "code"
                out.append(" ")
                i += 1
                continue
            out.append("\n" if c == "\n" else " ")
            i += 1
    return "".join(out)


def source_files() -> list[Path]:
    return sorted(SOURCE_ROOT.rglob("*.h")) + sorted(SOURCE_ROOT.rglob("*.cpp"))


def check_delimiter_balance(path: Path, stripped: str) -> list[Problem]:
    problems = []
    for open_c, close_c, name in (("{", "}", "braces"), ("(", ")", "parens"), ("[", "]", "brackets")):
        o = stripped.count(open_c)
        c = stripped.count(close_c)
        if o != c:
            problems.append(Problem(path, f"unbalanced {name}: {o} '{open_c}' vs {c} '{close_c}'"))
    return problems


def includes_in_order(text: str) -> list[str]:
    return re.findall(r'#include\s+"([^"]+)"', text)


def check_generated_header(path: Path, text: str) -> list[Problem]:
    if path.suffix != ".h":
        return []
    includes = includes_in_order(text)
    gen_includes = [inc for inc in includes if inc.endswith(".generated.h")]
    has_reflection = bool(re.search(r"\b(UCLASS|USTRUCT|UENUM|UINTERFACE)\s*\(", text))

    if not gen_includes:
        if has_reflection:
            return [Problem(path, "declares a reflected type but has no .generated.h include")]
        return []

    problems = []
    if includes[-1] != gen_includes[-1]:
        problems.append(Problem(path, f"'{gen_includes[-1]}' must be the LAST include (found '{includes[-1]}' last)"))
    expected = f"{path.stem}.generated.h"
    if gen_includes[-1] != expected:
        problems.append(Problem(path, f"generated include '{gen_includes[-1]}' does not match file (expected '{expected}')"))
    return problems


def check_generated_body(path: Path, stripped: str) -> list[Problem]:
    if path.suffix != ".h":
        return []
    n_class = len(re.findall(r"\bUCLASS\s*\(", stripped))
    n_struct = len(re.findall(r"\bUSTRUCT\s*\(", stripped))
    n_iface = len(re.findall(r"\bUINTERFACE\s*\(", stripped))
    n_body = len(re.findall(r"\bGENERATED_BODY\s*\(", stripped)) + len(
        re.findall(r"\bGENERATED_UINTERFACE_BODY\s*\(", stripped)
    )
    expected_min = n_class + n_struct + n_iface
    if n_body < expected_min:
        return [Problem(path, f"expected at least {expected_min} GENERATED_BODY() (UCLASS/USTRUCT/UINTERFACE), found {n_body}")]
    return []


def check_cpp_includes_own_header(path: Path, text: str) -> list[Problem]:
    if path.suffix != ".cpp":
        return []
    own_header = f"{path.stem}.h"
    # Only enforce when a sibling header actually exists somewhere in the module.
    header_exists = any(p.name == own_header for p in SOURCE_ROOT.rglob("*.h"))
    if not header_exists:
        return []
    includes = includes_in_order(text)
    if own_header not in includes:
        return [Problem(path, f"does not #include its own header \"{own_header}\"")]
    return []


# symbol/header token -> module that must appear in Build.cs. Only modules that
# are NOT transitively guaranteed by Core/CoreUObject/Engine are listed, to keep
# the check free of false positives.
MODULE_TOKENS = {
    "EnhancedInput": ["EnhancedInputComponent.h", "EnhancedInputSubsystems.h", "UEnhancedInputComponent", "UEnhancedInputLocalPlayerSubsystem"],
    "GameplayTags": ["GameplayTagContainer.h", "FGameplayTag"],
    "AIModule": ["AIController.h", "AAIController"],
    "NavigationSystem": ["NavigationSystem.h", "UNavigationSystemV1", "FNavLocation"],
    "UMG": ["Blueprint/UserWidget.h", "UUserWidget", "Components/TextBlock.h", "UWidgetTree"],
    "Json": ["Dom/JsonObject.h", "Serialization/JsonSerializer.h", "FJsonObject", "FJsonSerializer", "TJsonReaderFactory"],
    "ChaosVehicles": ["WheeledVehiclePawn.h", "AWheeledVehiclePawn", "ChaosWheeledVehicleMovementComponent"],
}


def check_build_dependencies(all_stripped: dict[Path, str]) -> list[Problem]:
    if not BUILD_CS.exists():
        return [Problem(BUILD_CS, "MountHope.Build.cs is missing")]
    build_text = BUILD_CS.read_text(encoding="utf-8")
    problems = []
    for module, tokens in MODULE_TOKENS.items():
        used_in = None
        for path, stripped in all_stripped.items():
            if any(tok in stripped for tok in tokens):
                used_in = path
                break
        if used_in is None:
            continue
        if not re.search(rf'"{re.escape(module)}"', build_text):
            problems.append(Problem(used_in, f"uses {module} API but \"{module}\" is not a module dependency in MountHope.Build.cs"))
    return problems


def check_dynamic_delegate_ufunctions(all_text: dict[Path, str], all_stripped: dict[Path, str]) -> list[Problem]:
    # Collect every method declared UFUNCTION() across all headers: a method name
    # is "reflected" if a UFUNCTION( appears within the 3 lines above its decl.
    reflected: set[str] = set()
    method_decl = re.compile(r"\b([A-Za-z_]\w*)\s*\(")
    for path, stripped in all_stripped.items():
        if path.suffix != ".h":
            continue
        lines = stripped.splitlines()
        for idx, line in enumerate(lines):
            if "UFUNCTION" not in line:
                continue
            # A UFUNCTION() macro applies to exactly the next function declaration.
            # Walk forward (past blank lines / specifiers) to the first line that
            # contains a call-signature '(', take that method name, and stop — so a
            # neighbouring UFUNCTION's window can't bleed onto an unrelated method.
            for look in range(idx + 1, min(idx + 6, len(lines))):
                if "(" not in lines[look]:
                    continue
                m = method_decl.search(lines[look])
                if m:
                    reflected.add(m.group(1))
                break

    bind_re = re.compile(r"(?:AddDynamic|RemoveDynamic)\s*\(\s*[^,]+,\s*&\s*[A-Za-z_]\w*\s*::\s*([A-Za-z_]\w*)\s*\)")
    all_method_names: set[str] = set()
    for stripped in all_stripped.values():
        for m in method_decl.finditer(stripped):
            all_method_names.add(m.group(1))

    problems = []
    seen: set[tuple[str, str]] = set()
    for path, stripped in all_stripped.items():
        for m in bind_re.finditer(stripped):
            method = m.group(1)
            key = (str(path), method)
            if key in seen:
                continue
            seen.add(key)
            # Only fail when we can positively see the method is declared somewhere
            # (so we're sure it's a real method) but never as a UFUNCTION.
            if method in all_method_names and method not in reflected:
                problems.append(Problem(path, f"AddDynamic/RemoveDynamic target '{method}' is not declared UFUNCTION()"))
    return problems


def main() -> int:
    if not SOURCE_ROOT.is_dir():
        print(f"ERROR: source dir not found: {SOURCE_ROOT}", file=sys.stderr)
        return 1

    files = source_files()
    all_text = {p: p.read_text(encoding="utf-8") for p in files}
    all_stripped = {p: strip_literals_and_comments(t) for p, t in all_text.items()}

    problems: list[Problem] = []
    for path in files:
        problems += check_delimiter_balance(path, all_stripped[path])
        problems += check_generated_header(path, all_text[path])
        problems += check_generated_body(path, all_stripped[path])
        problems += check_cpp_includes_own_header(path, all_text[path])
    problems += check_build_dependencies(all_stripped)
    problems += check_dynamic_delegate_ufunctions(all_text, all_stripped)

    if problems:
        print(f"QUAHOG_Unreal C++ static check FAILED ({len(problems)} problem(s)):", file=sys.stderr)
        for p in problems:
            print(f"  - {p}", file=sys.stderr)
        return 1

    print(f"QUAHOG_Unreal C++ static check passed ({len(files)} files).")
    print("NOTE: structural gate only. Unreal Editor and UnrealBuildTool were not run.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
