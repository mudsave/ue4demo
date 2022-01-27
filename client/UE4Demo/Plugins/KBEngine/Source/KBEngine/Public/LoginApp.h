#pragma once
#include <functional>

#include "Core.h"
#include "MessagesHandler.h"

namespace KBEngine
{
	class KBEngineApp;
	class NetworkInterfaceBase;
	class MessageReader;
	class Messages;

	class KBENGINE_API LoginApp : public MessagesHandler
	{
	public:
		/* ���ӻص�����
		int�Ĳ�����ʾ�����룬�������Ϣ���Բο�KBEngineApp::serverErrs_
		*/
		typedef std::function<void(int32)> ConnectCallbackFunc;

	public:
		LoginApp(KBEngineApp* app);
		virtual ~LoginApp();

		// ���ӵ�������
		void Connect(const FString& host, uint16 port, ConnectCallbackFunc func);

		// �����Ͽ���������
		void Disconnect();

		// ��������Ƿ���Ч�������ϣ�
		bool NetworkIsValid();

		// ��¼
		void Login(const FString& account, const FString& password, const TArray<uint8>& datas, CLIENT_TYPE clientType, ConnectCallbackFunc func);

		// ��������
		void ResetPassword(const FString& account, ConnectCallbackFunc func);

		/*
		�����˺�
		*/
		void CreateAccount(const FString& username, const FString& password, const TArray<uint8>& datas, ConnectCallbackFunc func);

		// ÿ��Tickִ��һ��
		void Process();

		virtual void HandleMessage(const FString &name, MemoryStream *stream) override;
		virtual void HandleMessage(const FString &name, const TArray<FVariant> &args) override;

		const FString& BaseAppAccount() { return baseappAccount_; }
		const FString& BaseAppHost() { return baseappHost_; }
		uint16 BaseAppTcpPort() { return baseappTcpPort_; }
		uint16 BaseAppUdpPort() { return baseappUdpPort_; }

	public:
		// for internal

		NetworkInterfaceBase* pNetworkInterface() { return networkInterface_; }
		void OnLoseConnect();  // ʧȥ������������ӣ��������Ͽ���


	private:
		void ClearNetwork();
		void SendTick();

		void OnConnected(const FString& host, uint16 port, bool success);

		void CmdHello();
		void CmdImportClientMessages();
		void CmdImportServerErrorsDescr();

		void Client_onHelloCB(MemoryStream &stream);
		void Client_onVersionNotMatch(MemoryStream &stream);
		void Client_onScriptVersionNotMatch(MemoryStream &stream);
		void Client_onImportClientMessages(MemoryStream &stream);
		void Client_onImportServerErrorsDescr(MemoryStream &stream);

		void Client_onLoginFailed(MemoryStream &stream);
		void Client_onLoginSuccessfully(MemoryStream &stream);
		void Client_onReqAccountResetPasswordCB(uint16 failcode);
		void Client_onCreateAccountResult(MemoryStream &stream);

		void Client_onAppActiveTickCB();

	private:
		KBEngineApp* app_ = nullptr;

		// ��Ϣ������
		MessageReader *messageReader_ = nullptr;

		// ��Ϣ��ִ����
		Messages *messages_ = nullptr;

		NetworkInterfaceBase* networkInterface_ = nullptr;

		// ��¼�µ�¼ʱ�ĵ�ַ���˺ŵ���Ϣ
		FString host_;
		uint16 port_;
		FString account_;
		FString password_;
		ConnectCallbackFunc connectedCallbackFunc_;


		// ��¼�ɹ���loginapp���ص�BaseApp��¼��Ϣ
		FString baseappAccount_;
		FString baseappHost_;
		uint16 baseappTcpPort_ = 0;
		uint16 baseappUdpPort_ = 0;

		// ���һ����������ʱ�䡢���һ���յ������ظ���ʱ��
		// ������Ϊ�Ƿ���ߵ��ж�����
		FDateTime lastTicktime_ = FDateTime::UtcNow();
		FDateTime lastTickCBTime_ = FDateTime::UtcNow();


	};  // end of class LoginApp;


}