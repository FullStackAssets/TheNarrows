#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MHPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class AMHVehiclePawn;
class UInputAction;
class UInputMappingContext;
class USoundBase;
struct FInputActionValue;

UCLASS()
class MOUNTHOPE_API AMHPlayerCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AMHPlayerCharacter();

    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Interaction")
    void TryInteract();

    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Dialogue")
    void RequestDialogueChoice(int32 ChoiceIndex);

    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Vehicle")
    void RequestEnterExitVehicle();

    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Vehicle")
    bool EnterVehicle(AMHVehiclePawn* VehiclePawn);

    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Vehicle")
    bool ExitVehicle();

    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Movement")
    void SetSprinting(bool bEnableSprint);

    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Radio")
    void RequestRadioNextStation();

    UFUNCTION(BlueprintPure, Category = "Mount Hope|Movement")
    float GetStaminaPercent() const { return MaxStamina > 0.0f ? Stamina / MaxStamina : 0.0f; }

    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Combat")
    void PickUpPistol(int32 AmmoAmount);

    UFUNCTION(BlueprintCallable, Category = "Mount Hope|Combat")
    void FirePistol();

    UFUNCTION(BlueprintPure, Category = "Mount Hope|Combat")
    bool HasPistol() const { return bHasPistol; }

    UFUNCTION(BlueprintPure, Category = "Mount Hope|Combat")
    int32 GetPistolAmmo() const { return PistolAmmo; }

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mount Hope|Camera")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mount Hope|Camera")
    TObjectPtr<UCameraComponent> FollowCamera;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mount Hope|Movement")
    float WalkSpeed = 450.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mount Hope|Movement")
    float SprintSpeed = 700.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mount Hope|Interaction")
    float InteractionRadiusMeters = 2.75f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mount Hope|Vehicle")
    float VehicleInteractRange = 350.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mount Hope|Movement")
    float MaxStamina = 100.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Mount Hope|Movement")
    float Stamina = 100.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mount Hope|Movement")
    float SprintStaminaDrainPerSecond = 28.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mount Hope|Movement")
    float StaminaRegenPerSecond = 18.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Input")
    TObjectPtr<UInputAction> IA_Move = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Input")
    TObjectPtr<UInputAction> IA_Look = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Input")
    TObjectPtr<UInputAction> IA_Sprint = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Input")
    TObjectPtr<UInputAction> IA_Interact = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Input")
    TObjectPtr<UInputAction> IA_EnterExitVehicle = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Input")
    TObjectPtr<UInputAction> IA_RadioNextStation = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Input")
    TObjectPtr<UInputAction> IA_Fire = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Input")
    TObjectPtr<UInputAction> IA_DialogueChoice1 = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Input")
    TObjectPtr<UInputAction> IA_DialogueChoice2 = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Mount Hope|Combat")
    bool bHasPistol = false;

    UPROPERTY(BlueprintReadOnly, Category = "Mount Hope|Combat")
    int32 PistolAmmo = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mount Hope|Combat")
    float PistolRange = 15000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mount Hope|Combat")
    float PistolVehicleDamage = 25.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mount Hope|Combat")
    float PistolCopDamage = 20.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mount Hope|Combat")
    int32 PistolCrimeSeverity = 40;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mount Hope|Audio")
    TObjectPtr<USoundBase> GunfireSound;

    UPROPERTY(BlueprintReadOnly, Category = "Mount Hope|Vehicle")
    bool bInVehicle = false;

    UPROPERTY(BlueprintReadOnly, Category = "Mount Hope|Vehicle")
    TObjectPtr<AMHVehiclePawn> CurrentVehicle = nullptr;

private:
    void BindLegacyInput(UInputComponent* PlayerInputComponent);
    void BindEnhancedInput(UInputComponent* PlayerInputComponent);
    void AddDefaultMappingContext();

    void InputMove(const FInputActionValue& Value);
    void InputLook(const FInputActionValue& Value);
    void InputSprintStart(const FInputActionValue& Value);
    void InputSprintStop(const FInputActionValue& Value);
    void LegacySprintStart();
    void LegacySprintStop();
    void SetSprintRequested(bool bRequested);
    void InputInteract(const FInputActionValue& Value);
    void InputDialogueChoice1();
    void InputDialogueChoice2();
    void InputDialogueChoice1Action(const FInputActionValue& Value);
    void InputDialogueChoice2Action(const FInputActionValue& Value);
    void InputEnterExitVehicle(const FInputActionValue& Value);
    void InputRadioNextStation(const FInputActionValue& Value);
    void InputFire(const FInputActionValue& Value);

    void MoveForward(float Value);
    void MoveRight(float Value);
    void Turn(float Value);
    void LookUp(float Value);

    void TryInteractWithWorld();
    AMHVehiclePawn* FindNearestVehicle() const;
    void TickStamina(float DeltaSeconds);

    bool bSprintRequested = false;
};
