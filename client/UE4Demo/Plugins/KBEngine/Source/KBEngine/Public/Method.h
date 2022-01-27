#pragma once
#include "KBEDefine.h"

namespace KBEngine
{
	class KBEDATATYPE_BASE;

	/*
	ʵ�嶨��ķ���ģ��
	�����һ��def�ļ��ж���ķ�������ģ�����ṩ�˸÷��������������Ϣ
	���磺�����Ĳ�����������id��������Ӧ�ű���handler
	*/
	class KBENGINE_API Method
	{
	public:
		FString name;
		uint16 methodUtype = 0;
		int16 aliasID = -1;
		TArray<KBEDATATYPE_BASE *> args;
		MessageHandler handler = NULL;

		Method()
		{
		}
	};

}
