#include "Online/KGSnapshotComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

UKGSnapshotComponent::UKGSnapshotComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UKGSnapshotComponent::OnRegister()
{
	Super::OnRegister();
	if (!PersistentId.IsValid())
	{
		PersistentId = FGuid::NewGuid();
	}
}

void UKGSnapshotComponent::WriteRecord(FKGActorRecord& OutRecord) const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	OutRecord.Id = PersistentId;
	OutRecord.Class = FSoftClassPath(Owner->GetClass());
	OutRecord.Transform = Owner->GetActorTransform();

	if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
	{
		if (Root->IsSimulatingPhysics())
		{
			OutRecord.LinearVelocity = Root->GetPhysicsLinearVelocity();
			OutRecord.AngularVelocityDeg = Root->GetPhysicsAngularVelocityInDegrees();
			OutRecord.bSleeping = !Root->IsAnyRigidBodyAwake();
		}
	}

	OutRecord.Blob.Reset();
	FMemoryWriter Writer(OutRecord.Blob, true);
	FObjectAndNameAsStringProxyArchive Ar(Writer, true);
	Ar.ArIsSaveGame = true;
	Owner->Serialize(Ar);
}

void UKGSnapshotComponent::ReadRecord(const FKGActorRecord& Record)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	// Actor::Serialize does not cover component properties, so the id is restored explicitly.
	PersistentId = Record.Id;
	Owner->SetActorTransform(Record.Transform, false, nullptr, ETeleportType::TeleportPhysics);

	if (Record.Blob.Num() > 0)
	{
		FMemoryReader Reader(Record.Blob, true);
		FObjectAndNameAsStringProxyArchive Ar(Reader, true);
		Ar.ArIsSaveGame = true;
		Owner->Serialize(Ar);
	}

	if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
	{
		if (Root->IsSimulatingPhysics())
		{
			Root->SetPhysicsLinearVelocity(Record.LinearVelocity);
			Root->SetPhysicsAngularVelocityInDegrees(Record.AngularVelocityDeg);
			if (Record.bSleeping)
			{
				Root->PutAllRigidBodiesToSleep();
			}
		}
	}
}
