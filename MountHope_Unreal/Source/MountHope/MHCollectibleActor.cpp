#include "MHCollectibleActor.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MHCollectibleSubsystem.h"
#include "MHGameModeBase.h"
#include "MHGameStateSubsystem.h"

AMHCollectibleActor::AMHCollectibleActor()
{
    PrimaryActorTick.bCanEverTick = false;

    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    RootComponent = InteractionSphere;
    InteractionSphere->SetSphereRadius(InteractionRadius);
    InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    DisplayMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayMesh"));
    DisplayMesh->SetupAttachment(RootComponent);
    DisplayMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AMHCollectibleActor::BeginPlay()
{
    Super::BeginPlay();

    InteractionSphere->SetSphereRadius(InteractionRadius);
    RefreshVisibilityForCollectedState();
}

FText AMHCollectibleActor::GetInteractionPrompt_Implementation() const
{
    UGameInstance* GameInstance = GetGameInstance();
    if (GameInstance)
    {
        if (UMHCollectibleSubsystem* Collectibles = GameInstance->GetSubsystem<UMHCollectibleSubsystem>())
        {
            FMHCollectibleRecord Record;
            if (Collectibles->FindCollectible(ItemId, Record) && !Record.DisplayName.IsEmpty())
            {
                return FText::FromString(FString::Printf(TEXT("Pick up %s"), *Record.DisplayName));
            }
        }
    }

    return FText::FromString(TEXT("Pick up item"));
}

void AMHCollectibleActor::Interact_Implementation(APawn* InstigatorPawn)
{
    if (!InstigatorPawn || ItemId.IsNone())
    {
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance)
    {
        return;
    }

    UMHCollectibleSubsystem* Collectibles = GameInstance->GetSubsystem<UMHCollectibleSubsystem>();
    if (!Collectibles || Collectibles->IsCollected(ItemId))
    {
        return;
    }

    FMHCollectibleRecord Record;
    if (!Collectibles->FindCollectible(ItemId, Record))
    {
        return;
    }

    if (!Collectibles->CollectItem(ItemId))
    {
        return;
    }

    if (UMHGameStateSubsystem* GameState = GameInstance->GetSubsystem<UMHGameStateSubsystem>())
    {
        if (Record.CashReward != 0)
        {
            GameState->AddCash(Record.CashReward);
        }

        TArray<FName> CompletedSets;
        const int32 SetBonus = Collectibles->ConsumePendingSetBonus(CompletedSets);
        if (SetBonus > 0)
        {
            GameState->AddCash(SetBonus);
            UE_LOG(LogTemp, Log, TEXT("MountHope: Collectible set bonus $%d (%d set(s))"), SetBonus, CompletedSets.Num());
        }
    }
    else
    {
        TArray<FName> DiscardedSets;
        Collectibles->ConsumePendingSetBonus(DiscardedSets);
    }

    if (AMHGameModeBase* GameMode = Cast<AMHGameModeBase>(UGameplayStatics::GetGameMode(this)))
    {
        for (const FMHCollectibleSet& Set : Collectibles->CollectibleSets)
        {
            if (!Set.MemberIds.Contains(ItemId))
            {
                continue;
            }

            int32 Found = 0;
            int32 Total = 0;
            if (Collectibles->GetSetProgress(Set.Id, Found, Total))
            {
                GameMode->NotifyCollectibleProgress(ItemId, Set.Id, Found);
            }
        }

        GameMode->NotifyCollectibleProgress(ItemId, NAME_None, 1);
    }

    UGameplayStatics::PlaySound2D(this, PickupSound);
    RefreshVisibilityForCollectedState();
}

void AMHCollectibleActor::RefreshVisibilityForCollectedState()
{
    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance)
    {
        return;
    }

    UMHCollectibleSubsystem* Collectibles = GameInstance->GetSubsystem<UMHCollectibleSubsystem>();
    const bool bAlreadyCollected = Collectibles && Collectibles->IsCollected(ItemId);

    SetActorHiddenInGame(bAlreadyCollected);
    SetActorEnableCollision(!bAlreadyCollected);
}
