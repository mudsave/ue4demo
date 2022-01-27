// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "KBEngineApp.h"
#include "GameFramework/Actor.h"
#include "KBEMain.generated.h"

/*
��������ģ��
���������а�װ����Ҫ�������¼�(installEvents)��ͬʱ��ʼ��KBEngine(initKBEngine)
*/

UCLASS()
class KBENGINE_API AKBEMain : public AActor
{
	GENERATED_BODY()

	static AKBEMain* instance;

public:
	static AKBEMain* Instance()
	{
		return AKBEMain::instance;
	}

	// Sets default values for this actor's properties
	AKBEMain();
	~AKBEMain();

	virtual void PreInitializeComponents() override;

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Called every frame
	virtual void Tick(float DeltaSeconds) override;

	// Called whenever this actor is being removed from a level
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;


	virtual void KBEUpdate();
	virtual void InitKBEngine();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString host = "127.0.0.1";

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 port = 20013;
	
	//UPROPERTY(EditAnywhere, BlueprintReadWrite)
	KBEngine::CLIENT_TYPE clientType = KBEngine::CLIENT_TYPE::CLIENT_TYPE_MINI;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString persistentDataPath = "Application.PersistentDataPath";

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool syncPlayer = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TCP_SEND_BUFFER_MAX = 32768;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TCP_RECV_BUFFER_MAX = 65535;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool useAliasEntityID = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool isOnInitCallPropertysSetMethods = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool forceDisableUDP = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int UDP_SEND_BUFFER_MAX = 256;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int UDP_RECV_BUFFER_MAX = 128;

	KBEngine::KBEngineApp * pKBEApp = nullptr;

private:
	// for test������������������
	void TestResolveIPAddress();
};
