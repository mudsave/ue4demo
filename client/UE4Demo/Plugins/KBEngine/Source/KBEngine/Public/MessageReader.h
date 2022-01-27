#pragma once
#include <utility>

#include "Memorystream.h"
#include "Message.h"

namespace KBEngine
{
#define MessageID uint16
#define MessageLength uint16
#define MessageLengthEx uint32

	class MessagesHandler;

	class KBENGINE_API MessageReader
	{
	public:
		MessageReader(MessagesHandler* handler, Messages *messages, uint32 bufferLength = 65535);
		~MessageReader();

		enum READ_STATE
		{
			// ��ϢID
			READ_STATE_MSGID = 0,

			// ��Ϣ�ĳ���65535����
			READ_STATE_MSGLEN = 1,

			// ���������Ϣ���ȶ��޷�����Ҫ��ʱʹ����չ����
			// uint32
			READ_STATE_MSGLEN_EX = 2,

			// ��Ϣ������
			READ_STATE_BODY = 3
		};

		// �������̴߳�������
		void Process();

		void ProcessData(const uint8* datas, MessageLengthEx length);
		
		// �������߳�д������
		uint32 Write(const uint8* datas, MessageLengthEx length);

		void Reset();

	private:
		uint32 FreeSpace();
		void Process_(const uint8* datas, MessageLengthEx length);

	private:
		MessagesHandler* messagesHandler_ = nullptr;
		Messages *messages_ = nullptr;

		uint32 rpos_ = 0;
		uint32 wpos_ = 0;
		uint8* buffer_ = nullptr;
		uint32 bufferLength_ = 0;

		MessageID msgid = 0;
		MessageLength msglen = 0;
		MessageLengthEx expectSize = 2;
		READ_STATE state = READ_STATE::READ_STATE_MSGID;
		MemoryStream stream;

	};

}
