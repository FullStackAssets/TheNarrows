#include "MHPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "MHGameModeBase.h"
#include "MHDialogueSubsystem.h"
#include "MHInteractable.h"
#include "MHPedestrianCharacter.h"
#include "MHPoliceUnitPawn.h"
#include "MHRadioSubsystem.h"
#include "MHVehiclePawn.h"
#include "MHWantedSubsystem.h"

AMHPlayerCharacter::AMHPlayerCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 450.0f;
    CameraBoom->bUsePawnControlRotation = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;

    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
    GetCharacterMovement()->BrakingDecelerationWalking = 1800.0f;
}

void AMHPlayerCharacter::BeginPlay()
{
    Super::BeginPlay();
    AddDefaultMappingContext();
}

void AMHPlayerCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    TickStamina(DeltaSeconds);
}

void AMHPlayerCharacter::TickStamina(float DeltaSeconds)
{
    if (bInVehicle)
    {
        return;
    }

    if (bSprintRequested && Stamina > 0.0f)
    {
        Stamina = FMath::Max(0.0f, Stamina - SprintStaminaDrainPerSecond * DeltaSeconds);
        if (Stamina <= 0.0f)
        {
            SetSprinting(false);
        }
    }
    else if (Stamina < MaxStamina)
    {
        // Regen whenever not actively draining, not just when the sprint key is released — if
        // stamina hits 0 while sprint is still held, movement is already forced to walk speed
        // above, so waiting for bSprintRequested to go false too would strand stamina at 0 until
        // the player releases and re-presses the key.
        Stamina = FMath::Min(MaxStamina, Stamina + StaminaRegenPerSecond * DeltaSeconds);
    }
}

void AMHPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        BindEnhancedInput(EnhancedInput);
    }

    BindLegacyInput(PlayerInputComponent);

    if (PlayerInputComponent)
    {
        PlayerInputComponent->BindAction(TEXT("DialogueChoice1"), IE_Pressed, this, &AMHPlayerCharacter::InputDialogueChoice1);
        PlayerInputComponent->BindAction(TEXT("DialogueChoice2"), IE_Pressed, this, &AMHPlayerCharacter::InputDialogueChoice2);
    }
}

void AMHPlayerCharacter::AddDefaultMappingContext()
{
    if (!DefaultMappingContext)
    {
        return;
    }

    if (APlayerController* PC = Cast<APlayerController>(Controller))
    {
        if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
                LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            {
                InputSubsystem->AddMappingContext(DefaultMappingContext, 0);
            }
        }
    }
}

void AMHPlayerCharacter::BindEnhancedInput(UInputComponent* PlayerInputComponent)
{
    UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (!EnhancedInput)
    {
        return;
    }

    if (IA_Move)
    {
        EnhancedInput->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AMHPlayerCharacter::InputMove);
    }
    if (IA_Look)
    {
        EnhancedInput->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AMHPlayerCharacter::InputLook);
    }
    if (IA_Sprint)
    {
        EnhancedInput->BindAction(IA_Sprint, ETriggerEvent::Started, this, &AMHPlayerCharacter::InputSprintStart);
        EnhancedInput->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &AMHPlayerCharacter::InputSprintStop);
        EnhancedInput->BindAction(IA_Sprint, ETriggerEvent::Canceled, this, &AMHPlayerCharacter::InputSprintStop);
    }
    if (IA_Interact)
    {
        EnhancedInput->BindAction(IA_Interact, ETriggerEvent::Started, this, &AMHPlayerCharacter::InputInteract);
    }
    if (IA_EnterExitVehicle)
    {
        EnhancedInput->BindAction(
            IA_EnterExitVehicle,
            ETriggerEvent::Started,
            this,
            &AMHPlayerCharacter::InputEnterExitVehicle);
    }
    if (IA_RadioNextStation)
    {
        EnhancedInput->BindAction(
            IA_RadioNextStation,
            ETriggerEvent::Started,
            this,
            &AMHPlayerCharacter::InputRadioNextStation);
    }
    if (IA_Fire)
    {
        EnhancedInput->BindAction(IA_Fire, ETriggerEvent::Started, this, &AMHPlayerCharacter::InputFire);
    }
    if (IA_DialogueChoice1)
    {
        EnhancedInput->BindAction(IA_DialogueChoice1, ETriggerEvent::Started, this, &AMHPlayerCharacter::InputDialogueChoice1Action);
    }
    if (IA_DialogueChoice2)
    {
        EnhancedInput->BindAction(IA_DialogueChoice2, ETriggerEvent::Started, this, &AMHPlayerCharacter::InputDialogueChoice2Action);
    }
}

void AMHPlayerCharacter::BindLegacyInput(UInputComponent* PlayerInputComponent)
{
    if (DefaultMappingContext && IA_Move)
    {
        return;
    }

    PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &AMHPlayerCharacter::MoveForward);
    PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &AMHPlayerCharacter::MoveRight);
    PlayerInputComponent->BindAxis(TEXT("Turn"), this, &AMHPlayerCharacter::Turn);
    PlayerInputComponent->BindAxis(TEXT("LookUp"), this, &AMHPlayerCharacter::LookUp);
    PlayerInputComponent->BindAction(TEXT("Interact"), IE_Pressed, this, &AMHPlayerCharacter::TryInteract);
    PlayerInputComponent->BindAction(
        TEXT("EnterExitVehicle"),
        IE_Pressed,
        this,
        &AMHPlayerCharacter::RequestEnterExitVehicle);
    PlayerInputComponent->BindAction(
        TEXT("RadioNextStation"),
        IE_Pressed,
        this,
        &AMHPlayerCharacter::RequestRadioNextStation);
    PlayerInputComponent->BindAction(TEXT("Fire"), IE_Pressed, this, &AMHPlayerCharacter::FirePistol);
    PlayerInputComponent->BindAction(TEXT("Sprint"), IE_Pressed, this, &AMHPlayerCharacter::LegacySprintStart);
    PlayerInputComponent->BindAction(TEXT("Sprint"), IE_Released, this, &AMHPlayerCharacter::LegacySprintStop);
}

void AMHPlayerCharacter::RequestRadioNextStation()
{
    if (!bInVehicle)
    {
        return;
    }

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UMHRadioSubsystem* RadioSubsystem = GameInstance->GetSubsystem<UMHRadioSubsystem>())
        {
            RadioSubsystem->NextStation();
        }
    }
}

void AMHPlayerCharacter::PickUpPistol(int32 AmmoAmount)
{
    bHasPistol = true;
    PistolAmmo = FMath::Max(0, PistolAmmo + AmmoAmount);
}

void AMHPlayerCharacter::FirePistol()
{
    if (!bHasPistol || bInVehicle || PistolAmmo <= 0 || !FollowCamera || !GetWorld())
    {
        return;
    }

    --PistolAmmo;
    UGameplayStatics::PlaySound2D(this, GunfireSound);

    const FVector TraceStart = FollowCamera->GetComponentLocation();
    const FVector TraceEnd = TraceStart + FollowCamera->GetComponentRotation().Vector() * PistolRange;

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);

    FHitResult Hit;
    if (!GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
    {
        return;
    }

    AActor* HitActor = Hit.GetActor();
    if (!HitActor)
    {
        return;
    }

    UMHWantedSubsystem* WantedSubsystem = GetWorld()->GetSubsystem<UMHWantedSubsystem>();

    if (AMHVehiclePawn* HitVehicle = Cast<AMHVehiclePawn>(HitActor))
    {
        HitVehicle->ApplyVehicleDamage(PistolVehicleDamage);
        if (WantedSubsystem)
        {
            WantedSubsystem->ReportCrime(EMHCrimeType::PropertyDamage, PistolCrimeSeverity / 2);
        }
    }
    else if (AMHPoliceUnitPawn* HitCop = Cast<AMHPoliceUnitPawn>(HitActor))
    {
        HitCop->ApplyDamage(PistolCopDamage);
        if (HitCop->IsDead())
        {
            HitCop->Destroy();
        }
        if (WantedSubsystem)
        {
            WantedSubsystem->ReportCrime(EMHCrimeType::Assault, PistolCrimeSeverity);
        }
    }
    else if (AMHPedestrianCharacter* HitPedestrian = Cast<AMHPedestrianCharacter>(HitActor))
    {
        HitPedestrian->Destroy();
        if (WantedSubsystem)
        {
            WantedSubsystem->ReportCrime(EMHCrimeType::Assault, PistolCrimeSeverity);
        }
    }
}

void AMHPlayerCharacter::SetSprinting(bool bEnableSprint)
{
    if (bInVehicle)
    {
        return;
    }

    GetCharacterMovement()->MaxWalkSpeed = bEnableSprint ? SprintSpeed : WalkSpeed;
}

void AMHPlayerCharacter::RequestDialogueChoice(int32 ChoiceIndex)
{
    if (bInVehicle)
    {
        return;
    }

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UMHDialogueSubsystem* DialogueSubsystem = GameInstance->GetSubsystem<UMHDialogueSubsystem>())
        {
            DialogueSubsystem->SelectChoice(ChoiceIndex, false);
        }
    }
}

void AMHPlayerCharacter::InputDialogueChoice1()
{
    RequestDialogueChoice(0);
}

void AMHPlayerCharacter::InputDialogueChoice2()
{
    RequestDialogueChoice(1);
}

void AMHPlayerCharacter::InputDialogueChoice1Action(const FInputActionValue& Value)
{
    (void)Value;
    RequestDialogueChoice(0);
}

void AMHPlayerCharacter::InputDialogueChoice2Action(const FInputActionValue& Value)
{
    (void)Value;
    RequestDialogueChoice(1);
}

void AMHPlayerCharacter::TryInteract()
{
    if (bInVehicle)
    {
        ExitVehicle();
        return;
    }

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UMHDialogueSubsystem* DialogueSubsystem = GameInstance->GetSubsystem<UMHDialogueSubsystem>())
        {
            if (DialogueSubsystem->IsConversationActive())
            {
                DialogueSubsystem->AdvanceConversation(false);
                return;
            }
        }
    }

    if (AMHVehiclePawn* NearbyVehicle = FindNearestVehicle())
    {
        EnterVehicle(NearbyVehicle);
        return;
    }

    TryInteractWithWorld();
}

void AMHPlayerCharacter::RequestEnterExitVehicle()
{
    if (bInVehicle)
    {
        ExitVehicle();
        return;
    }

    if (AMHVehiclePawn* NearbyVehicle = FindNearestVehicle())
    {
        EnterVehicle(NearbyVehicle);
    }
}

bool AMHPlayerCharacter::EnterVehicle(AMHVehiclePawn* VehiclePawn)
{
    if (bInVehicle || !VehiclePawn || !Controller)
    {
        return false;
    }

    if (!VehiclePawn->CanDriverEnter(this))
    {
        return false;
    }

    CurrentVehicle = VehiclePawn;
    bInVehicle = true;

    SetActorEnableCollision(false);
    SetActorHiddenInGame(true);
    Controller->Possess(VehiclePawn);

    if (AMHGameModeBase* GameMode = GetWorld() ? Cast<AMHGameModeBase>(GetWorld()->GetAuthGameMode()) : nullptr)
    {
        GameMode->TryCompleteVehicleObjective(true);
    }

    return true;
}

bool AMHPlayerCharacter::ExitVehicle()
{
    if (!bInVehicle || !CurrentVehicle || !Controller)
    {
        return false;
    }

    const FVector ExitLocation = CurrentVehicle->GetSuggestedExitLocation();
    SetActorLocation(ExitLocation);
    SetActorEnableCollision(true);
    SetActorHiddenInGame(false);
    Controller->Possess(this);

    bInVehicle = false;
    CurrentVehicle = nullptr;
    return true;
}

void AMHPlayerCharacter::InputMove(const FInputActionValue& Value)
{
    if (bInVehicle)
    {
        return;
    }

    const FVector2D Axis = Value.Get<FVector2D>();
    if (Axis.IsNearlyZero())
    {
        return;
    }

    if (Controller)
    {
        const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
        AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Axis.Y);
        AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Axis.X);
    }
}

void AMHPlayerCharacter::InputLook(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    if (!Axis.IsNearlyZero())
    {
        AddControllerYawInput(Axis.X);
        AddControllerPitchInput(Axis.Y);
    }
}

void AMHPlayerCharacter::InputSprintStart(const FInputActionValue& Value)
{
    SetSprintRequested(true);
}

void AMHPlayerCharacter::InputSprintStop(const FInputActionValue& Value)
{
    SetSprintRequested(false);
}

void AMHPlayerCharacter::LegacySprintStart()
{
    SetSprintRequested(true);
}

void AMHPlayerCharacter::LegacySprintStop()
{
    SetSprintRequested(false);
}

void AMHPlayerCharacter::SetSprintRequested(bool bRequested)
{
    bSprintRequested = bRequested;
    SetSprinting(bRequested && Stamina > 0.0f);
}

void AMHPlayerCharacter::InputInteract(const FInputActionValue& Value)
{
    TryInteract();
}

void AMHPlayerCharacter::InputEnterExitVehicle(const FInputActionValue& Value)
{
    RequestEnterExitVehicle();
}

void AMHPlayerCharacter::InputRadioNextStation(const FInputActionValue& Value)
{
    RequestRadioNextStation();
}

void AMHPlayerCharacter::InputFire(const FInputActionValue& Value)
{
    FirePistol();
}

void AMHPlayerCharacter::TryInteractWithWorld()
{
    TArray<FOverlapResult> Overlaps;
    const float RadiusUnrealUnits = InteractionRadiusMeters * 100.0f;
    const FCollisionShape InteractionShape = FCollisionShape::MakeSphere(RadiusUnrealUnits);
    FCollisionObjectQueryParams ObjectTypes;
    ObjectTypes.AddObjectTypesToQuery(ECC_Pawn);
    ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);

    if (!GetWorld()->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity, ObjectTypes, InteractionShape))
    {
        return;
    }

    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* Candidate = Overlap.GetActor();
        if (Candidate != nullptr && Candidate != this && Candidate->GetClass()->ImplementsInterface(UMHInteractable::StaticClass()))
        {
            IMHInteractable::Execute_Interact(Candidate, this);
            return;
        }
    }
}

void AMHPlayerCharacter::MoveForward(float Value)
{
    if (Controller == nullptr || FMath::IsNearlyZero(Value) || bInVehicle)
    {
        return;
    }

    const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
    AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Value);
}

void AMHPlayerCharacter::MoveRight(float Value)
{
    if (Controller == nullptr || FMath::IsNearlyZero(Value) || bInVehicle)
    {
        return;
    }

    const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
    AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Value);
}

void AMHPlayerCharacter::Turn(float Value)
{
    AddControllerYawInput(Value);
}

void AMHPlayerCharacter::LookUp(float Value)
{
    AddControllerPitchInput(Value);
}

AMHVehiclePawn* AMHPlayerCharacter::FindNearestVehicle() const
{
    if (!GetWorld())
    {
        return nullptr;
    }

    AMHVehiclePawn* BestVehicle = nullptr;
    float BestDistSq = VehicleInteractRange * VehicleInteractRange;
    const FVector MyLoc = GetActorLocation();

    for (TActorIterator<AMHVehiclePawn> It(GetWorld()); It; ++It)
    {
        AMHVehiclePawn* Candidate = *It;
        if (!IsValid(Candidate))
        {
            continue;
        }

        const float DistSq = FVector::DistSquared(MyLoc, Candidate->GetActorLocation());
        if (DistSq <= BestDistSq)
        {
            BestDistSq = DistSq;
            BestVehicle = Candidate;
        }
    }

    return BestVehicle;
}
