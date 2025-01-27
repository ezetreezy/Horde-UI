#include "HordeMode/Public/HMWeapon.h"
#include <Components/SkeletalMeshComponent.h>
#include <DrawDebugHelpers.h>
#include <Kismet/GameplayStatics.h>
#include <Particles/ParticleSystem.h>
#include <Components/SkeletalMeshComponent.h>
#include <Particles/ParticleSystemComponent.h>
#include "HordeMode/HordeMode.h"
#include <PhysicalMaterials/PhysicalMaterial.h>
#include <TimerManager.h>
#include "Net/UnrealNetwork.h"
#include <UnrealMathUtility.h>



/* Rifle Blueprint = Default Weapon */


const int32 DISTANCE = 10000;

AHMWeapon::AHMWeapon()
{
	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;
	MuzzleSocketName = "MuzzleSocket";
	TracerTargetName = "Target";
	BaseHitPointDamage = 1.0f;
	RateOfFire = 600.0f;
	SetReplicates(true);
	// defaults to min = 2.0f, default = 100
	NetUpdateFrequency = 66.0f;
	MinNetUpdateFrequency = 33.0f;
	BulletSpread = 2.0f;
}

void AHMWeapon::BeginPlay()
{
	Super::BeginPlay();

	TimeBetweenShots = 60.0f / RateOfFire;
}

void AHMWeapon::StartFire()
{
	// Note: Guarantee's delay of TimeBetwenShots between single shots, automatic fire for everything else
	float FirstDelay = FMath::Max(LastFireTime + TimeBetweenShots - GetWorld()->TimeSeconds, 0.0f);

	GetWorldTimerManager().SetTimer(TimerHandle_TimeBtwnShots,this, &AHMWeapon::Fire, TimeBetweenShots, true, FirstDelay);
}
void AHMWeapon::StopFire()
{
	GetWorldTimerManager().ClearTimer(TimerHandle_TimeBtwnShots);
}

void AHMWeapon::Fire()
{
	if (Role < ROLE_Authority)
	ServerFire();

	AActor * Owner = GetOwner();
	if (Owner) {
		FVector EyeLocation;
		FRotator EyeRotation;


		Owner->GetActorEyesViewPoint(EyeLocation, EyeRotation);

		// bullet spread (don't have 100% accuracy
		FVector ShotDirection = EyeRotation.Vector();
		float halfRad = FMath::DegreesToRadians(BulletSpread);
		ShotDirection = FMath::VRandCone(ShotDirection, halfRad, halfRad);
		FVector TraceEnd = EyeLocation + (ShotDirection * DISTANCE);

		

		FCollisionQueryParams QParams;
		QParams.AddIgnoredActor(Owner);
		QParams.AddIgnoredActor(this);
		QParams.bReturnPhysicalMaterial = true;
		QParams.bTraceComplex = true; // more expensive, but nails the polygon on the mesh(accurate)

		// For Tracer Particle Effect
		FVector TracerEndPoint = TraceEnd;

		EPhysicalSurface SurfaceType = SurfaceType_Default;

		// struct that holds our hit data, normal, etc..
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, EyeLocation, TraceEnd, COLLISION_HMWEAPON, QParams)) {

			//blocking hit! process damage
			AActor * HitActor = Hit.GetActor();

			EPhysicalSurface SurfaceType = UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());

			float damageApplied = BaseHitPointDamage;

			if (SurfaceType == SURFACE_FLESH_VULNERABLE)
			damageApplied = 2.0f;
			
			UGameplayStatics::ApplyPointDamage(HitActor, damageApplied, ShotDirection, Hit, Owner->GetInstigatorController(), Owner, DamageType);

			PlayImpactEffect(SurfaceType, Hit.ImpactPoint);

			TracerEndPoint = Hit.ImpactPoint;
		}

		// play on current client
		PlayerWeaponFireEffects(TracerEndPoint);
	
		// update this struct,  updates all clients to play effect via calling
		if (Role == ROLE_Authority) {
			HitScan.TraceEnd = TracerEndPoint;
			HitScan.SurfaceType = SurfaceType;
		}

		LastFireTime = GetWorld()->TimeSeconds;
	}
}

void AHMWeapon::ServerFire_Implementation()
{
	Fire();
}

// intended for anti-cheat, validates code
bool AHMWeapon::ServerFire_Validate()
{
	return true;
}

void AHMWeapon::PlayerWeaponFireEffects(FVector &TracerEndPoint) {

	//Fname == lookups, FText = localizable
	if (MuzzleEffect) {
		UGameplayStatics::SpawnEmitterAttached(MuzzleEffect, MeshComponent, MuzzleSocketName);
	}

	if (BulletTracer) {
		FVector MuzzleLocation = MeshComponent->GetSocketLocation(MuzzleSocketName);
		UParticleSystemComponent * TracerComp = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BulletTracer, MuzzleLocation);
		if (TracerComp) {
			TracerComp->SetVectorParameter(TracerTargetName, TracerEndPoint);
		}
	}

	APawn * myOwner = Cast<APawn>(GetOwner());
	if (myOwner) {
		APlayerController * controller = Cast<APlayerController>(myOwner->GetController());
		if (controller && FireCameraShake) {
			controller->ClientPlayCameraShake(FireCameraShake);
		}
	}

}


void AHMWeapon::PlayImpactEffect(EPhysicalSurface SurfaceType, FVector ImpactPoint)
{
	UParticleSystem * SelectedEffect = nullptr;
	switch (SurfaceType)
	{
	case SURFACE_FLESH_DEFAULT:
	case SURFACE_FLESH_VULNERABLE:
		SelectedEffect = FleshImpactEffect;
		break;
	default:
		SelectedEffect = DefaultImpactEffect;
		break;
	}

	if (SelectedEffect) {
		FVector Muzzle = MeshComponent->GetSocketLocation(MuzzleSocketName);
		FVector shotDirection = ImpactPoint - Muzzle;
		shotDirection.Normalize();
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), SelectedEffect, ImpactPoint, shotDirection.Rotation());
	}
}
 // Server Play
void AHMWeapon::OnRep_HitScanTrace()
{
	PlayerWeaponFireEffects(HitScan.TraceEnd);
	PlayImpactEffect(HitScan.SurfaceType, HitScan.TraceEnd);
}

// doesn't need to specified in the header file, unreal header tool auto generates this for us
void AHMWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AHMWeapon, HitScan, COND_SkipOwner);
}


