#pragma once

#include "Core.h"
#include "MemoryStream.h"

namespace KBEngine
{
	/*
	 KBEngine������࣬��Ҫʹ��KBEngine���������̳�������࣬��������ʵ���Լ��ض�����Ϊ��
	*/
	class KBENGINE_API KBEPersonality
	{
	public:
		/* ��ĳ��space data���ı������ʱ���˷��������� */
		virtual void OnSetSpaceData(uint32 spaceID, const FString &key, const FString &value) {}

		/* ��ĳ��space data��ɾ��ʱ���˷��������� */
		virtual void OnDelSpaceData(uint32 spaceID, const FString &key) {}

		/* ���������е�Space�����˵�ͼ�ĵ�������ʱ���˷��������� */
		virtual void OnAddSpaceGeometryMapping(uint32 spaceID, const FString &respath) {}

		/* ����¼ʧ��ʱ���˷��������� */
		virtual void OnLoginFailed(int32 errCode, const FString& errName, const FString& errDesc) {}

		/* ��������������ӶϿ�ʱ���˷��������� */
		virtual void OnDisconnect() {}




		/* ��ͨ��KBEngineApp::ReLoginBaseapp()���п������µ�¼�������ͨ���˷����ص� */
		virtual void OnReLoginBaseapp(int32 errCode, const FString& errName, const FString& errDesc) {}

		/* ��ͨ��KBEngineApp::NewPassword()�޸����룬�����ͨ���˷����ص� */
		virtual void OnNewPassword(int32 errCode, const FString& errName, const FString& errDesc) {}

		/* ��ͨ��KBEngineApp::BindAccountEmail()�޸������ַ�������ͨ���˷����ص� */
		virtual void OnBindAccountEmail(int32 errCode, const FString& errName, const FString& errDesc) {}

		/* ���ɹ�ͨ��KBEngineApp::CreateAccount()�������˺��Ժ󣬴˷��������� */
		virtual void OnCreateAccountSuccess(const FString& account) {}

		/* ���ɹ�ͨ��KBEngineApp::ResetPassword()�����˺�����󣬴˷��������� */
		virtual void OnResetPasswordSuccess(const FString& account) {}

		// �����֪ͨ�Ѿ����ÿ����¼׼��������
		virtual void OnAcrossServerReady() {}
		// �������¼ʧ��ʱ���˷���������
		virtual void OnAcrossLoginFailed(int32 errCode, const FString& errName, const FString& errDesc) {}
		// �����¼�ɹ�
		virtual void OnAcrossLoginSuccess() {}

		/*
		�����֪ͨ���������ؿ�ʼ
		see also: �������ĵ���baseapp -> classes -> Proxy::streamStringToClient()
		*/
		void OnStreamDataStarted(int16 id, uint32 datasize, const FString& descr) {}
		void OnStreamDataRecv(MemoryStream &stream) {}
		void OnStreamDataCompleted(int16 id) {}

	public:
		static void Register(KBEPersonality * inst);
		static void Deregister();
		FORCEINLINE static KBEPersonality* Instance() { return instance_; }

	private:
		static KBEPersonality* instance_;
	};
}
