#pragma once

#include "KBEngine.h"
#include "Avatar.h"


class MovePlatform : public Avatar
{
	KBE_DECLARE_ENTITY_MAP();

	typedef Avatar Supper;

public:
	MovePlatform();
	~MovePlatform();

	float MoveSpeed() { return mMoveSpeed * 100; }
	void set_moveSpeed(const float &nv, const float &ov);

	virtual void OnEnterScenes() override;

	virtual eEntityType EntityType() override { return Avatar::eEntityType::MovePlatform; }

protected:
	virtual void Set_Position(const FVector &oldVal) override;
	virtual void Set_Direction(const FVector &oldVal) override;
	virtual void OnUpdateVolatileData() override;

	virtual void OnEnterWorld() override;            // ��Entity��������ʱ���˷���������
	virtual void OnLeaveWorld() override;            // ��Entity�뿪���磨������ʱ��ʱ���˷���������
	virtual void OnEnterSpace() override;            // ��Entity�����ͼʱ���˷���������
	virtual void OnLeaveSpace() override;            // ��Entity�뿪��ͼʱ���˷���������	

private:
	float mMoveSpeed = 0.0f;
};

