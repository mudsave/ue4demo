#pragma once

#include <string.h>

#include "KBEDebug.h"
#include "Entity.h"
#include "Runtime/Core/Public/Misc/DateTime.h"
#include "KBEngineArgs.h"
#include "PersistentInfos.h"
#include "KBEUpdatable.h"
#include "Message.h"
#include "BaseApp.h"
#include "LoginApp.h"

#define byte uint8

namespace KBEngine
{
	class LoginApp;
	class BaseApp;

	class KBENGINE_API KBEngineApp
	{
	public:
		static KBEngineApp* app;

	public:
		KBEngineApp(KBEngineArgs* args);
		virtual ~KBEngineApp();

		// �������ѭ��������
		virtual void Process();

		// �Ͽ��������������
		void Disconnect();

		// ��ȡ�������Ľ�ɫEntity
		Entity* Player();

		/*
		��¼������ˣ������¼���loginapp������(baseapp)����¼���̲������
		*/
		void Login(const FString& username, const FString& password, const TArray<uint8>& datas);

		void CreateAccount(const FString& username, const FString& password, const TArray<uint8>& datas);
		void ResetPassword(const FString& username);

		void ReLoginBaseapp();
		void BindAccountEmail(const FString& emailAddress);
		void NewPassword(const FString old_password, const FString new_password);

		// ʹ��δ���ڵĵ�¼ƾ֤�����¼baseapp
		void AcrossLoginBaseapp();
		void AcrossServerReady(UINT64 loginKey, FString& baseappHost, UINT16 port);
		// �����½���������µ�½Դ������
		void AcrossLoginBack();

		// ���ձ�׼��ÿ���ͻ��˲��ֶ�Ӧ�ð����������
		const FString& Component() { return component_; }

		// ͨ��EntityID������֮��Ӧ��Entityʵ��
		Entity* FindEntity(int32 entityID);

		// ��ȡEntity�ֵ�
		const TMap<int32, Entity*>* Entities();

	public:
		// for internal

		PersistentInofs* pPersistentInofs() { return persistentInofs_; }

		BaseApp* pBaseApp()
		{
			if(baseApp_!= nullptr)
				return baseApp_;
			if (acrossBaseApp_ != nullptr) 
				return acrossBaseApp_;
			return nullptr;
		}

		LoginApp* pLoginApp() { return loginApp_; }
		Messages* pMessages() { return &messages_; }

		void OnLoseConnect();  // ʧȥ������������ӣ��������Ͽ���

	public:
		// args for internal
		const TArray<uint8>& EncryptedKey() { return args_->encryptedKey; }
		const FString& ClientVersion() { return args_->clientVersion; }
		const FString& ClientScriptVersion() { return args_->clientScriptVersion; }
		int32 TickInterval() { return args_->tickInterval; }
		bool IsOnInitCallPropertysSetMethods() { return args_->isOnInitCallPropertysSetMethods; }
		bool UseAliasEntityID() { return args_->useAliasEntityID; }
		bool SyncPlayer() { return args_->syncPlayer; }
		const FString& PersistentDataPath() { return args_->persistentDataPath; }
		CLIENT_TYPE ClientType() { return args_->clientType; }
		const FString& LoginappHost() { return args_->host; }
		uint16 LoginappPort() { return args_->port; }
		uint32 GetTcpRecvBufferMax() { return args_->TCP_RECV_BUFFER_MAX; }
		uint32 GetTcpSendBufferMax() {	return args_->TCP_SEND_BUFFER_MAX; }
		
		uint32 GetUdpRecvBufferMax() { return args_->UDP_RECV_BUFFER_MAX; }
		uint32 GetUdpSendBufferMax() { return args_->UDP_SEND_BUFFER_MAX; }
		bool IsForceDisableUDP() { return args_->forceDisableUDP; }

	private:
		// ȡ�ó�ʼ��ʱ�Ĳ���
		KBEngineArgs* GetInitArgs() { return args_; }

		void CloseLoginApp();
		void CloseBaseApp();
		void CloseAcrossBaseApp();

		void OnConnectToLoginappCB(int32 code, FString key);
		void OnLoginToLoginappCB(int32 code);

		void OnConnectToBaseappCB(int32 code);
		void OnLoginToBaseappCB(int32 code);

		void OnConnectAcrossBaseappCB(int32 code);
		void OnLoginAcrossBaseappCB(int32 code);

		void OnCreateAccountCB(int32 code);
		void OnResetPasswordCB(int32 code);

		void OnReLoginBaseappCB(int32 code);
		void OnBindAccountEmailCB(int32 code);
		void OnNewPasswordCB(int32 code);

	private:
		void ResetAcrossData();

		FString component_ = TEXT("client");

		LoginApp* loginApp_ = nullptr;
		BaseApp* baseApp_ = nullptr;

		BaseApp* acrossBaseApp_ = nullptr;	// �����¼��baseapp

		// ��Ϣ������
		Messages messages_;

		// ÿ֡��ִ�еĶ�������
		Updatables updatables_;

		// ����ʱ���ݽ��������в���
		KBEngineArgs* args_ = NULL;

		// �־û������Ϣ�� ���磺�ӷ���˵����Э����Գ־û������أ��´ε�¼�汾�������ı�
		// ����ֱ�Ӵӱ��ؼ������ṩ��¼�ٶ�
		PersistentInofs* persistentInofs_ = nullptr;

		FString username_;
		FString password_;
		TArray<uint8> clientdatas_;
		FString baseappAccount_;

		UINT64 acrossLoginKey_;
		FString acrossBaseappHost_;
		UINT16 acrossBaseappPort_;
		FDateTime acrossLoginReadyTime_;

		bool isInAcrossServer_ = false;

		bool loseConnectedFromServer_ = false;
	};

}


