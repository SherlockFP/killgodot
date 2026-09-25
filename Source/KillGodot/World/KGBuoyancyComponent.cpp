#include "World/KGBuoyancyComponent.h"
#include "Components/PrimitiveComponent.h"
#include "World/KGWaves.h"

UKGBuoyancyComponent::UKGBuoyancyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UKGBuoyancyComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                         FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UPrimitiveComponent* Body = GetOwner() ? Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()) : nullptr;
	if (!Body || !Body->IsSimulatingPhysics())
	{
		return;
	}
	const float Time = GetWorld()->GetTimeSeconds();
	const FBoxSphereBounds B = Body->Bounds;
	const FVector C = B.Origin;
	const FVector E = B.BoxExtent;
	// Four bottom corners + centre, pulled in a little so thin objects still sample inside themselves.
	const FVector Samples[] = {
		C + FVector(E.X * 0.7f, E.Y * 0.7f, -E.Z * 0.95f), C + FVector(-E.X * 0.7f, E.Y * 0.7f, -E.Z * 0.95f),
		C + FVector(E.X * 0.7f, -E.Y * 0.7f, -E.Z * 0.95f), C + FVector(-E.X * 0.7f, -E.Y * 0.7f, -E.Z * 0.95f),
		C + FVector(0.0f, 0.0f, -E.Z * 0.5f)};
	const float Mass = Body->GetMass();
	const float PerPoint = Mass * 980.0f * Buoyancy / UE_ARRAY_COUNT(Samples);
	int32 Wet = 0;
	for (const FVector& P : Samples)
	{
		const float Depth = FKGWaves::HeightAt(P.X, P.Y, Time) - P.Z;
		if (Depth > 0.0f)
		{
			Body->AddForceAtLocation(FVector(0.0f, 0.0f, PerPoint * FMath::Min(Depth / FullDepth, 1.5f)), P);
			++Wet;
		}
	}
	if (Wet > 0)
	{
		const float Frac = float(Wet) / UE_ARRAY_COUNT(Samples);
		Body->AddForce(-Body->GetPhysicsLinearVelocity() * Mass * LinearDrag * Frac);
		Body->AddTorqueInRadians(-Body->GetPhysicsAngularVelocityInRadians() * Mass * 100.0f * AngularDrag * Frac);
	}
}
