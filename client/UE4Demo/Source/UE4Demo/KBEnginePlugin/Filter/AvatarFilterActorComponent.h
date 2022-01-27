// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <queue>

#include "FilterActorComponent.h"
#include "AvatarFilterActorComponent.generated.h"

class UAvatarFilterActorComponent;

class SmoothFilter
{
public:
	static const uint32 NUM_STORED_INPUTS = 8;
	
	// ����ʱ����ÿ�����ݵĽ���ʱ����
	const float NONZERO_TIME_DIFFERENCE = 0.05f;


	SmoothFilter();
	SmoothFilter(const SmoothFilter& filter);
	virtual ~SmoothFilter();

	void owner(UAvatarFilterActorComponent* o) { owner_ = o; }
	UAvatarFilterActorComponent* owner() { return owner_; }

	bool isActive() { return true; }

	void reset(double time) { reset_ = true; }

	virtual void input(double time, int32 spaceID, int32 vehicleID, const FVector& pos, const FVector& posError, const FVector& dir);

	void output(double time);

	bool getLastInput(double& time, int32& spaceID, int32& vehicleID, FVector& pos, FVector& posError, FVector& direction);

	/**
	*	This is an internal structure used to encapsulate a single set of
	*	received input values for use by the AvatarFilter. Currently the
	*	avatar filter ignores 'roll' and that is continued here.
	*/
	struct StoredInput
	{
		double		time_;
		int32		spaceID_;
		int32		vehicleID_;
		FVector		position_;
		FVector		positionError_;
		FVector		direction_;
		bool		onGround_;
	};

protected:

	/**
	*	This structure stores a location in time and space for the filter
	*	output to move too or from.
	*/
	struct Waypoint
	{
		double			time_;
		int32			spaceID_;
		int32			vehicleID_;
		FVector			position_;
		FVector			direction_;

		StoredInput		storedInput_;

		void changeCoordinateSystem(int32 spaceID, int32 vehicleID);
	};


protected:
	void resetStoredInputs(double time, int32 spaceID, int32 vehicleID, const FVector& position, const FVector& positionError, const FVector& direction);
	void extract(double time, int32& outputSpaceID, int32& outputVehicleID, FVector& outputPosition, FVector& outputVelocity, FVector& outputDirection);
	void chooseNextWaypoint(double time);

public:
	StoredInput& getStoredInput(uint32 index);

protected:
	const StoredInput& getStoredInput(uint32 index) const;


	// Doxygen comments for all members can be found in the .cpp
	StoredInput		storedInputs_[NUM_STORED_INPUTS];
	uint32			currentInputIndex_;

	Waypoint		nextWaypoint_;
	Waypoint		previousWaypoint_;

	float			latency_;
	float			idealLatency_;
	double			timeOfLastOutput_;
	bool			gotNewInput_;
	bool			reset_;
	
	// ��¼�����һ�ε�����ʱ�䣬�����������룬
	// �Ա�����̫��ʱ��û����������ӳ��㷨��������Ӱ��
	double			timeOfLastInput_;

	UAvatarFilterActorComponent* owner_;

public:
	static float s_latencyVelocity_;
	static float s_latencyMinimum_;
	static float s_latencyFrames_;
	static float s_latencyCurvePower_;
};

/**
 * һ�����ӷ�����ָ�ӣ���һ����ʱ����ʵ�Ĳ�ֵ�ƶ���������ָ����λ�ã������ڷ���������������ҡ���������꼰����
 */
UCLASS( )
class UE4DEMO_API UAvatarFilterActorComponent : public UFilterActorComponent
{	
	GENERATED_BODY()

	typedef UFilterActorComponent Supper;

	UAvatarFilterActorComponent();
	
	// Called when the game starts
	virtual void BeginPlay() override;

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	virtual ~UAvatarFilterActorComponent()
	{}

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void OnUpdateVolatileData(const FVector& position, const FVector& direction, int32 parentID) override;
	virtual void OnUpdateVolatileDataByParent(const FVector& position, const FVector& direction, int32 parentID) override;
	virtual void SetPosition(const FVector& position, int32 parentID) override;
	virtual void SetDirection(const FVector& direction, int32 parentID) override;
	virtual void OnGotParentEntity(KBEngine::Entity* parentEnt) override;
	virtual void OnLoseParentEntity() override;
	virtual void SpeedChangedNotify(float speed) override;
	virtual void InitFilter(UAvatarActorComponent* _comp) override;

	void Pos(int32 vehicleID, const FVector& position, const FVector& direction, const FVector& velocity);
	void setRunFlag(bool flag);
	void ResetComponent();

private:
	SmoothFilter mFilter;
	FVector vLastPos = FVector::ZeroVector;
	FVector mVelocity = FVector::ZeroVector;
	bool runFlag = true;
};
