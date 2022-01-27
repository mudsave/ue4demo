#pragma once
#include "MessageReader.h"

namespace KBEngine
{
	class MessageReader;
	class NetworkInterfaceBase;

	class PacketReceiverBase : public FRunnable
	{
	protected:
		const static int RECV_BUFFER_LENGTH = 65535;

	public:
		PacketReceiverBase(NetworkInterfaceBase* networkInterface, uint32 buffLength = RECV_BUFFER_LENGTH);
		~PacketReceiverBase();

		virtual void Process(MessageReader& messageReader);
		void StartBackgroundRecv();
		void WillClose() { willClose_ = true; }

	public:
		// for FRunnable
		virtual uint32 Run() override;
		void DoThreadedWork();


	protected:
		// ����������socket�У���������ӿڿ��ܻᵼ�¿������ⲿ�ǲ������ɱ���
		void StopBackgroundRecv();

		// ���߳��е��ã������д�Ļ������ռ�
		uint32 FreeWriteSpace();

		// ���߳��е��ã���ʼ��Socket�ж�ȡ����
		virtual void BackgroundRecv() {};

	protected:
		NetworkInterfaceBase* networkInterface_ = NULL;

		uint8* buffer_;
		uint32 bufferLength_ = 0;

		// socket�򻺳���д����ʼλ��
		uint32 wpos_ = 0;

		// ���̶߳�ȡ���ݵ���ʼλ��
		uint32 rpos_ = 0;

		FRunnableThread* thread_ = nullptr;
		bool breakThread_ = false;

		// ��NetworkInterface�ر�����ʱ֪ͨ��
		// �Ա����������ر�����ʱҲ����������Ϣ
		bool willClose_ = false;
	};

}