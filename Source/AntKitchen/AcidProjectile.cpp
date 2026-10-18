#include "AcidProjectile.h"
#include "HumanCharacter.h"
#include "AntCharacter.h"
#include "AntPlayerController.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"

AAcidProjectile::AAcidProjectile()
{
	bReplicates = true;
	SetReplicatingMovement(true);
	SetNetCullDistanceSquared(1.0e9f);
	InitialLifeSpan = 4.f;

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	Sphere->InitSphereRadius(6.f);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionObjectType(ECC_WorldDynamic);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Sphere->SetGenerateOverlapEvents(true);
	Sphere->SetCanEverAffectNavigation(false);
	RootComponent = Sphere;

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Sphere);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetGenerateOverlapEvents(false);
	Visual->SetCastShadow(false);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Sphere;
	Movement->InitialSpeed = 2600.f;
	Movement->MaxSpeed = 5000.f;
	Movement->bRotationFollowsVelocity = true;
	Movement->bInitialVelocityInLocalSpace = false;
	Movement->bShouldBounce = false;
	Movement->Bounciness = 0.6f;
	Movement->ProjectileGravityScale = 0.15f;

	Sphere->OnComponentBeginOverlap.AddDynamic(this, &AAcidProjectile::OnOverlap);
	Movement->OnProjectileStop.AddDynamic(this, &AAcidProjectile::OnStop);
	Movement->OnProjectileBounce.AddDynamic(this, &AAcidProjectile::OnBounce);
}

void AAcidProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAcidProjectile, InitialVelocity);
	DOREPLIFETIME(AAcidProjectile, Color);
	DOREPLIFETIME(AAcidProjectile, VisualSize);
	DOREPLIFETIME(AAcidProjectile, GravityScale);
	DOREPLIFETIME(AAcidProjectile, bBounce);
}

void AAcidProjectile::Setup(const FAntWeapon& Weapon, const FVector& Direction)
{
	Damage = Weapon.Damage;
	Splash = Weapon.Splash;
	bBounce = Weapon.bBounce;
	GravityScale = Weapon.Gravity;
	VisualSize = Weapon.Size;
	Color = FMath::Lerp(FLinearColor(0.4f, 1.f, 0.15f), AntKit::RarityColor(Weapon.Rarity), Weapon.Rarity > 0 ? 0.7f : 0.f);
	InitialVelocity = Direction.GetSafeNormal() * Weapon.ProjectileSpeed;

	Movement->InitialSpeed = Weapon.ProjectileSpeed;
	Movement->MaxSpeed = Weapon.ProjectileSpeed * 1.5f;
	Movement->Velocity = InitialVelocity;
	Movement->ProjectileGravityScale = GravityScale;
	Movement->bShouldBounce = bBounce;
}

void AAcidProjectile::BeginPlay()
{
	Super::BeginPlay();

	Movement->ProjectileGravityScale = GravityScale;
	Movement->bShouldBounce = bBounce;
	if (!HasAuthority() && !InitialVelocity.IsNearlyZero())
	{
		Movement->Velocity = InitialVelocity;
	}

	if (APawn* Inst = GetInstigator())
	{
		Sphere->IgnoreActorWhenMoving(Inst, true);
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		Visual->SetStaticMesh(AntKit::ShapeMesh(EAntShape::Sphere));
		Visual->SetRelativeScale3D(FVector(VisualSize));
		AntKit::Paint(Visual, Color);
	}
}

void AAcidProjectile::OnStop(const FHitResult& Hit)
{
	if (HasAuthority())
	{
		// Knock physics props around (grapes, bottle caps, sugar cubes...).
		if (UPrimitiveComponent* C = Hit.GetComponent())
		{
			if (C->IsSimulatingPhysics())
			{
				const FVector Dir = (Hit.ImpactPoint - GetActorLocation()).GetSafeNormal();
				C->AddImpulseAtLocation((Dir.IsNearlyZero() ? GetActorForwardVector() : Dir) * (300.f + Damage * 40.f), Hit.ImpactPoint);
			}
		}
		Explode(nullptr);
	}
	else
	{
		Visual->SetVisibility(false);
	}
}

void AAcidProjectile::OnBounce(const FHitResult& Hit, const FVector& ImpactVelocity)
{
	if (HasAuthority() && ++Bounces >= 3)
	{
		Explode(nullptr);
	}
}

void AAcidProjectile::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || !OtherActor || OtherActor == GetInstigator())
	{
		return;
	}
	if (AHumanCharacter* Human = Cast<AHumanCharacter>(OtherActor))
	{
		Explode(Human);
	}
	// Other ants: no friendly fire, the blob flies through.
}

void AAcidProjectile::Explode(AActor* DirectHit)
{
	if (!HasAuthority() || bExploded)
	{
		return;
	}
	bExploded = true;

	AController* InstController = GetInstigatorController();
	AAntPlayerController* PC = Cast<AAntPlayerController>(InstController);
	const FVector Here = GetActorLocation();

	auto Hurt = [this, InstController, PC, &Here](AActor* Victim, float Amount)
	{
		const bool bCrit = FMath::FRand() < 0.12f;
		const float Final = bCrit ? Amount * 2.f : Amount;
		UGameplayStatics::ApplyDamage(Victim, Final, InstController, this, UDamageType::StaticClass());
		if (PC)
		{
			PC->ClientDamageNumber(Here, FMath::RoundToInt(Final), bCrit);
		}
	};

	if (DirectHit)
	{
		Hurt(DirectHit, Damage);
	}

	if (Splash > 0.f)
	{
		for (TActorIterator<AHumanCharacter> It(GetWorld()); It; ++It)
		{
			AHumanCharacter* H = *It;
			if (!H || H == DirectHit)
			{
				continue;
			}
			const UCapsuleComponent* Cap = H->GetCapsuleComponent();
			const FVector Up = FVector::UpVector * Cap->GetScaledCapsuleHalfHeight();
			const FVector Closest = FMath::ClosestPointOnSegment(Here, H->GetActorLocation() - Up, H->GetActorLocation() + Up);
			if (FVector::Dist(Here, Closest) - Cap->GetScaledCapsuleRadius() < Splash)
			{
				Hurt(H, Damage * 0.6f);
			}
		}
	}

	Destroy();
}
