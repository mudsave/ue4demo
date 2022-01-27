// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "KBEngine.h"
#include "KBEnginePlugin/Filter/FilterActorComponent.h"
#include "Components/ActorComponent.h"
#include "AvatarActorComponent.generated.h"

class UFilterActorComponent;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class UE4DEMO_API UAvatarActorComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UAvatarActorComponent();

	// Called when the game starts
	virtual void BeginPlay() override;
	
	// Called every frame
	virtual void TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction ) override;

	FORCEINLINE KBEngine::Entity* entity()
	{
		return mEntity;
	}

	FORCEINLINE void entity(KBEngine::Entity* _entity)
	{
		mEntity = _entity;
	}

	FORCEINLINE UFilterActorComponent* filterComponent()
	{
		return mFilterComponent;
	}

	FORCEINLINE void filterComponent(UFilterActorComponent* _comp)
	{
		mFilterComponent = _comp;
	}

	virtual void __init__(KBEngine::Entity* _entity) { }
	virtual void SetPosition(const FVector& oldVal) { }
	virtual void SetDirection(const FVector& oldVal) { }
	virtual void onUpdateVolatileData(const FVector& position, const FVector& direction);
	virtual void OnFilterSpeedChanged(float speed) { }
	virtual void SetMoveSpeed() { }
	virtual void OnControlled(bool isControlled) { }
	virtual void OnGotParentEntity();
	virtual void OnLoseParentEntity();
	virtual void OnUpdateVolatileDataByParent();

	/* ʹ��������������
	 * @param position: [in/out]�����������꼰���������������
	 * @param ignoreNotOnGround: �Ƿ������Щ���ڵر��ϵĶ���ͨ������¶�Ӧ�ú��Ե���
	 */
	void FixOnGroundPosition(FVector& position, bool ignoreNotOnGround = true);


private:
	KBEngine::Entity* mEntity = nullptr;
	UFilterActorComponent* mFilterComponent = nullptr;	
};
