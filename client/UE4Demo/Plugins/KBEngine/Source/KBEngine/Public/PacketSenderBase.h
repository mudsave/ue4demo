#pragma once

#include "KBEDebug.h"

namespace KBEngine
{
	class NetworkInterfaceBase;

	class PacketSenderBase
	{
	public:
		PacketSenderBase(NetworkInterfaceBase* networkInterface);
		virtual ~PacketSenderBase();

		virtual bool Send(uint8* datas, uint32 length) = 0;

		void WillClose() { willClose_ = true; }
		
	protected:
		NetworkInterfaceBase* networkInterface_ = NULL;

		// ��NetworkInterface�ر�����ʱ֪ͨ��
		// �Ա����������ر�����ʱҲ����������Ϣ
		bool willClose_ = false;
	};

}	// end namespace KBEngine
