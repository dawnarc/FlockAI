// Fill out your copyright notice in the Description page of Project Settings.

#include "FlockAI.h"
#include "Agent.h"
#include "Stimulus.h"


AAgent::AAgent()
{
	PrimaryActorTick.bCanEverTick = true;

	// Initializing default values
	BaseMovementSpeed = 200.0f;
	MaxMovementSpeed = 300.0f;
	AlignmentWeight = 1.0f;
	CohesionWeight = 0.5f;
	SeparationWeight = 0.5f;
	VisionRadius = 400.0f;

	NegativeStimuliMaxFactor = 0.f;
	PositiveStimuliMaxFactor = 0.f;
	AgentPhysicalRadius = 45.0;

	// Create the mesh component
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShipMesh"));
	RootComponent = MeshComponent;
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	// Create vision sphere
	VisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("VisionSphere"));
	VisionSphere->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	VisionSphere->SetSphereRadius(VisionRadius);
}

void AAgent::BeginPlay()
{
	// Initialize move vector
	Super::BeginPlay();
	NewMoveVector = GetActorRotation().Vector().GetSafeNormal();
}

void AAgent::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	CurrentMoveVector = NewMoveVector;

	CalculateNewMoveVector();

	const FVector NewDirection = (NewMoveVector * BaseMovementSpeed * DeltaSeconds).GetClampedToMaxSize(MaxMovementSpeed * DeltaSeconds);
	const FRotator NewRotation = NewMoveVector.Rotation();

	FHitResult Hit(1.f);
	RootComponent->MoveComponent(NewDirection, NewRotation, true, &Hit);

	if (Hit.IsValidBlockingHit())
	{
		const FVector Normal2D = Hit.Normal.GetSafeNormal2D();
		const FVector Deflection = FVector::VectorPlaneProject(NewDirection, Normal2D) * (1.f - Hit.Time);
		RootComponent->MoveComponent(Deflection, NewRotation, true);
	}
}

void AAgent::CalculateNewMoveVector()
{
	ResetComponents();

	if (VisionSphere)
	{
		VisionSphere->GetOverlappingActors(Neighbourhood, AAgent::StaticClass());

		//Compute Alignment Component Vector
		for (AActor* Actor : Neighbourhood)
		{
			if (AAgent* Agent = Cast<AAgent>(Actor))
			{
				AlignmentComponent += Agent->CurrentMoveVector.GetSafeNormal();
			}
		}

		AlignmentComponent = AlignmentComponent.GetSafeNormal();

		Neighbourhood.Remove(Cast<AActor>(this));

		if (Neighbourhood.Num() > 0)
		{
			//Compute Cohesion Component Vector
			for (AActor* Actor : Neighbourhood)
			{
				FVector LocDiff = Actor->GetActorLocation() - GetActorLocation();
				CohesionComponent += LocDiff;
			}

			CohesionComponent = CohesionComponent / (float)Neighbourhood.Num() / 100.f;

			//Compute Separation Component Vector
			for (AActor* Actor : Neighbourhood)
			{
				FVector LocDiff = GetActorLocation() - Actor->GetActorLocation();
				FVector LocDiffNor = LocDiff.GetSafeNormal();

				SeparationComponent += LocDiffNor / FMath::Abs(LocDiff.Size() - AgentPhysicalRadius * 2.f);
			}

			FVector SeparationLoc = SeparationComponent * 100.f;
			SeparationComponent = SeparationLoc + SeparationLoc * (5.f / Neighbourhood.Num());
		}

		ComputeStimuliComponent();
	}

}

void AAgent::ResetComponents()
{
	AlignmentComponent = FVector::ZeroVector;
	CohesionComponent = FVector::ZeroVector;
	SeparationComponent = FVector::ZeroVector;
	NegativeStimuliComponent = FVector::ZeroVector;
	NegativeStimuliMaxFactor = 0.f;
	PositiveStimuliComponent = FVector::ZeroVector;
	PositiveStimuliMaxFactor = 0.f;
}

void AAgent::ComputeStimuliComponent()
{
	if (VisionSphere)
	{
		TSet<AActor*> Output;
		VisionSphere->GetOverlappingActors(Output);

		for (AActor* Actor : Output)
		{
			if (AStimulus* Stimulus = Cast<AStimulus>(Actor))
			{
				FVector LocDiff = Stimulus->GetActorLocation() - GetActorLocation();

				if (Stimulus->Value < 0)
				{
					//Compute Negative Stimuli Component Vector
					FVector StimuliCompLoc = LocDiff.GetSafeNormal() / FMath::Abs(LocDiff.Size() - AgentPhysicalRadius) * 100.f * Stimulus->Value;
					NegativeStimuliComponent += StimuliCompLoc;

					NegativeStimuliMaxFactor = FMath::Max(StimuliCompLoc.Size(), NegativeStimuliMaxFactor);
					//GEngine->AddOnScreenDebugMessage(-1, 0.1f, FColor::Red, FString::SanitizeFloat(NegativeStimuliMaxFactor));
				}
				else
				{
					float StimuliFactor = Stimulus->Value / LocDiff.Size();
					if (StimuliFactor > PositiveStimuliMaxFactor)
					{
						PositiveStimuliComponent = LocDiff.GetSafeNormal() * Stimulus->Value;
						PositiveStimuliMaxFactor = StimuliFactor;
					}
				}
			}
		}

		NegativeStimuliComponent = NegativeStimuliComponent.GetSafeNormal() * NegativeStimuliMaxFactor;

		NewMoveVector = AlignmentComponent * AlignmentWeight + CohesionComponent * CohesionWeight + SeparationComponent * SeparationWeight + NegativeStimuliComponent + PositiveStimuliComponent;
	}
}
