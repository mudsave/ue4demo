#pragma once

namespace KBEngine
{
	class KBEDATATYPE_BASE;

	/*
	�����һ��entitydef�ж��������
	��ģ�����������Ե�id�Լ��������͵���Ϣ
	*/
	class KBENGINE_API Property
	{
	public:
		enum EntityDataFlags
		{
			ED_FLAG_UNKOWN = 0x00000000, // δ����
			ED_FLAG_CELL_PUBLIC = 0x00000001, // �������cell�㲥
			ED_FLAG_CELL_PRIVATE = 0x00000002, // ��ǰcell
			ED_FLAG_ALL_CLIENTS = 0x00000004, // cell�㲥�����пͻ���
			ED_FLAG_CELL_PUBLIC_AND_OWN = 0x00000008, // cell�㲥���Լ��Ŀͻ���
			ED_FLAG_OWN_CLIENT = 0x00000010, // ��ǰcell�Ϳͻ���
			ED_FLAG_BASE_AND_CLIENT = 0x00000020, // base�Ϳͻ���
			ED_FLAG_BASE = 0x00000040, // ��ǰbase
			ED_FLAG_OTHER_CLIENTS = 0x00000080, // cell�㲥�������ͻ���
		};

		FString name;
		KBEDATATYPE_BASE *utype = NULL;
		uint16 properUtype = 0;
		uint32 properFlags = 0;
		int16 aliasID = -1;

		FString defaultValStr;
		PropertyHandler setmethod = NULL;

		FVariant val;

		Property()
		{
		}

		bool IsBase()
		{
			return properFlags == (uint32)EntityDataFlags::ED_FLAG_BASE_AND_CLIENT ||
				properFlags == (uint32)EntityDataFlags::ED_FLAG_BASE;
		}

		bool IsOwnerOnly()
		{
			return properFlags == (uint32)EntityDataFlags::ED_FLAG_CELL_PUBLIC_AND_OWN ||
				properFlags == (uint32)EntityDataFlags::ED_FLAG_OWN_CLIENT;
		}

		bool IsOtherOnly()
		{
			return properFlags == (uint32)EntityDataFlags::ED_FLAG_OTHER_CLIENTS ||
				properFlags == (uint32)EntityDataFlags::ED_FLAG_OTHER_CLIENTS;
		}
	};
}
