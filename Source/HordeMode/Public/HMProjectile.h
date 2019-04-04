#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HMProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UParticleSystem;
class UDamageType;

UCLASS()
class HORDEMODE_API AHMProjectile : public AActor
{
	GENERATED_BODY()
public:	
	AHMProjectile();

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void OnExplode();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	UParticleSystem * ExplodeEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	TSubclassOf<UDamageType> DamageType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LifeSpan")
	float LifeSpan;

	virtual void LifeSpanExpired() override;

	virtual void BeginPlay() override;
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	USphereComponent* CollisionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	UProjectileMovementComponent* ProjectileMovement;
};
