
#include "packetReceiverKCP.h"
#include "NetworkInterfaceBase.h"
#include "NetworkInterfaceKCP.h"
#include "ikcp.h"
#include "Misc/DateTime.h"

namespace KBEngine
{

PacketReceiverKCP::~PacketReceiverKCP()
{
	if (udpBuffer_)
		delete udpBuffer_;
}

// �������룬kcp����ʹ�ö��߳�
void PacketReceiverKCP::BackgroundRecv() 
{
	if (!networkInterface_ || !networkInterface_->Valid())
	{
		breakThread_ = true;
		return;
	}

	int32 bytesRead = 0;
	if (!networkInterface_->Socket()->RecvFrom(udpBuffer_, UDP_PACKET_LENTH, bytesRead, *remoteAddr_))
	{
		KBE_ERROR(TEXT("PacketReceiverKCP::BackgroundRecv: RecvFrom is not success!wpos_(%d), rpos_(%d)"), wpos_, rpos_);
		return;
	}

	auto networkInterfaceKCP = (NetworkInterfaceKCP*)networkInterface_;

	auto result = ikcp_input(networkInterfaceKCP->KCP(), (const char*)(udpBuffer_), bytesRead);
	if (result < 0)
	{
		KBE_ERROR(TEXT("PacketReceiverKCP::BackgroundRecv ikcp_input error %d"), result);
		hexlike(buffer_, wpos_, bytesRead);
	}
	else
	{
		while (true)
		{
			result = ikcp_recv(networkInterfaceKCP->KCP(), (char*)(udpBuffer_), UDP_PACKET_LENTH);
			if (result < 0)
			{
				//KBE_INFO(TEXT("PacketReceiverKCP::BackgroundRecv ikcp_recv result: %d, wpos_: %d, rpos_: %d"), result, wpos_, rpos_);
				break;
			}
			else
			{
				//hexlike(buffer_, wpos_, bytesRead);
				//KBE_DEBUG(TEXT("PacketReceiverKCP::BackgroundRecv ikcp_recv copy bytes %d, space:%d wpos_:%d at time(%s:%d)"),
				//	result, CheckForSpace(), wpos_, *FDateTime::UtcNow().ToString(), FDateTime::UtcNow().GetMillisecond());

				int startPos = 0;		// ��ʼ�����ֽڵ�λ��
				while (result > 0)
				{
					int space = CheckForSpace();
					if (space == 0)
					{
						KBE_ERROR(TEXT("PacketReceiverKCP::BackgroundRecv: no space!wpos_(%d), rpos_(%d)"), wpos_, rpos_);
						return;
					}

					int cpyBytes = result;		// �Ѿ������˶����ֽڣ�һ��ʼ���������ֽ�
					if (result > space)			// �ռ䲻�����������ֽ�
					{
						cpyBytes = space;		// �����ֽ���
					}

					memcpy(&buffer_[wpos_], &udpBuffer_[startPos], cpyBytes);
					startPos += cpyBytes;		// �Ѹ��Ƶ��ֽ�

					wpos_ += cpyBytes;		   	// ����дλ��

					result -= cpyBytes;			// ʣ���ֽڣ�û������һѭ������
				}
			}
		}
	}
}

void PacketReceiverKCP::Process(MessageReader & messageReader)
{
	if (!networkInterface_ || !networkInterface_->Valid())
	{
		breakThread_ = true;
		return;
	}

	uint32 dataSize = 0;
	while (networkInterface_->Socket()->HasPendingData(dataSize))
	{
		int32 bytesRead = 0;
		if (networkInterface_->Socket()->RecvFrom(udpBuffer_, UDP_PACKET_LENTH, bytesRead, *remoteAddr_))
		{
			auto networkInterfaceKCP = (NetworkInterfaceKCP*)networkInterface_;
			networkInterfaceKCP->NextTickKcpUpdate();

			auto result = ikcp_input(networkInterfaceKCP->KCP(), (const char*)(udpBuffer_), bytesRead);
			if (result < 0)
			{
				KBE_ERROR(TEXT("PacketReceiverKCP::BackgroundRecv ikcp_input error %d"), result);
				//hexlike(buffer_, wpos_, bytesRead);
				continue;
			}
			else
			{
				while (true)
				{
					result = ikcp_recv(networkInterfaceKCP->KCP(), (char*)(udpBuffer_), UDP_PACKET_LENTH);
					if (result < 0)
					{
						//KBE_INFO(TEXT("PacketReceiverKCP::BackgroundRecv ikcp_recv result: %d, wpos_: %d, rpos_: %d"), result, wpos_, rpos_);
						break;
					}

					if (result > 0)
					{
						messageReader.ProcessData(udpBuffer_, result);
					}
				}
			}
		}
	}
	//PacketReceiverBase::Process(messageReader);
}

uint32 PacketReceiverKCP::CheckForSpace()
{
	int space = FreeWriteSpace();

	// �����пռ��д�����������������߳���ֱ���пռ�Ϊֹ
	int retries = 0;
	while (space == 0)
	{
		retries += 1;

		if (retries > 10)
		{
			KBE_ERROR(TEXT("PacketReceiverKCP::CheckForSpace(): no space! disconnect to server!"));
			breakThread_ = true;
			if (!willClose_)
				networkInterface_->WillClose();
			return 0;
		}

		KBE_WARNING(TEXT("PacketReceiverKCP::CheckForSpace(): waiting for space, Please adjust 'UDP_RECV_BUFFER_MAX'! retries = %d, wpos_ = %d, rpos_ = %d"),
			retries, wpos_, rpos_);

		FPlatformProcess::Sleep(0.1);

		space = FreeWriteSpace();
	}

	return space;
}


}	// end namespace KBEngine
