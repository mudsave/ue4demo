#pragma once
#include "PacketSenderBase.h"
#include "NetworkInterfaceBase.h"
#include "HAL/RunnableThread.h"

namespace KBEngine
{

class PacketSenderTCP : public PacketSenderBase, FRunnable
{
public:
	PacketSenderTCP(NetworkInterfaceBase* networkInterface, uint32 buffLength = 65535);

	~PacketSenderTCP();

	bool Send(uint8* datas, uint32 length) override;

	void StartBackgroundSend();

	uint32 Run() override;

protected:
	void BackgroundSend();

	void DoThreadedWork();

	void RealBackgroundSend(uint32 sendSize, int32& bytesSent);

	uint32 SendSize() const;

	// �����п���������socket�У���������ӿڿ��ܻᵼ�¿������ⲿ�ǲ������ɱ���
	void StopBackgroundSend();

	void InitPipe();
	void ClosePipe();
	FString ReadPipe();
	void WritePipe();

protected:
	uint8* buffer_;
	uint32	bufferLength_ = 0;

	uint32 wpos_ = 0;				// д�������λ��
	uint32 spos_ = 0;				// ������ϵ�����λ��
	bool sending_ = false;

	FRunnableThread* thread_ = nullptr;
	bool breakThread_ = false;

	void* readPipe_;
	void* writePipe_;

};

}	// end namespace KBEngine
