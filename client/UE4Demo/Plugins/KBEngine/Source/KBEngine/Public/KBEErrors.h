#pragma once

#include "Core.h"
#include "MemoryStream.h"

namespace KBEngine
{

	enum class ERROR_TYPE
	{
		SUCCESS = 0,                     // �ɹ�
		CONNECT_TO_LOGINAPP_FAULT = -1,  // �޷����ӵ���¼������
		CONNECT_TO_BASEAPP_FAULT = -2,   // �޷����ӵ����ط�����
		VERSION_NOT_MATCH = -3,          // �ͻ�����������İ汾��ƥ��
		SCRIPT_VERSION_NOT_MATCH = -4,   // �ͻ��˵���������Ĵ���汾��ƥ��
		INVALID_NETWORK = -5,            // ����������
		LOSE_SERVER_CONNECTED = -6,      // ʧȥ�������������
	};

	class KBENGINE_API KBEErrors
	{
		// ��������˷��ػ�ͻ��˱��صĴ�����Ϣ
		typedef struct
		{
			int32 id;
			FString name;
			FString descr;
		}Error;

	public:
		static bool ImportServerErrorsDescr(MemoryStream &stream);
		static FString ErrorName(int32 errcode);
		static FString ErrorDesc(int32 errcode);
		static bool ServerErrorsDescrImported() { return serverErrorsDescrImported_; }
		static void Clear();
		static void InitLocalErrors();

	private:

	private:
		/* ���з�����Լ��ͻ��˴������Ӧ�Ĵ�������,
		�������Ĵ�����ο���kbe/src/lib/server/server_errors.h
		�ͻ��˴�����ο���ERROR_TYPE��
		���ڿͻ��˺ͷ������Ĵ�����һ���б��У���˲�ʹ��uint16����Ϊint32;
		С��0�����ǿͻ��˴��󣬴���0��Ϊ����������
		����0��ʾ�ɹ���
		*/
		static TMap<int32, Error> errors_;

		// �Ƿ��ѳ�ʼ������
		static bool serverErrorsDescrImported_;
	};

}