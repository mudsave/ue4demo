#pragma once
#include <functional>

#include "KBEDefine.h"
#include "Core.h"
#include "MessagesHandler.h"

namespace KBEngine
{
	class KBEngineApp;
	class NetworkInterfaceBase;
	class MessageReader;
	class Messages;
	class Entity;

	class KBENGINE_API BaseApp : public MessagesHandler
	{
		const static int ADDITIONAL_UPDATE_COUNT = 8;

	public:
		/* ���ӻص�����
		int�Ĳ�����ʾ�����룬�������Ϣ���Բο�KBEngineApp::serverErrs_
		*/
		typedef std::function<void(int32)> ConnectCallbackFunc;

	public:
		BaseApp(KBEngineApp* app);
		virtual ~BaseApp();

		// ���ӵ�������
		void Connect(const FString& host, uint16 tcpPort, uint16 udpPort, ConnectCallbackFunc func);

		// �����Ͽ���������
		void Disconnect();

		// ��������Ƿ���Ч�������ϣ�
		bool NetworkIsValid();

		// ��¼
		void Login(const FString& account, const FString& password, ConnectCallbackFunc func);

		/*
		�ص�¼������(baseapp)
		һЩ�ƶ���Ӧ�����׵��ߣ�����ʹ�øù��ܿ��ٵ����������˽���ͨ��
		*/
		void Relogin(ConnectCallbackFunc func);


		// �����¼
		void AcrossLogin(FString& accountName, FString& password, CLIENT_TYPE clientType, UINT16 loginKey, ConnectCallbackFunc func);
		// �����¼�ɹ�
		void AcrossLoginSuccess();


		// ���˺�����
		void BindAccountEmail(const FString& emailAddress, ConnectCallbackFunc func);

		// �޸�����
		void NewPassword(const FString& old_password, const FString& new_password, ConnectCallbackFunc func);

		// ÿ��Tickִ��һ��
		void Process();

		// ��ȡ�������Ľ�ɫEntity
		Entity* Player();

		// ͨ��entityID�ҵ���Ӧ��entityʵ��
		Entity* FindEntity(int32 entityID);

		// ��ȡEntity�ֵ�
		const TMap<int32, Entity*>* Entities() { return &entities_; }

		// ����Keyȡ��Ӧ��SpaceData
		const FString& GetSpaceData(const FString& key);

		// �������Է���������Ϣ
		virtual void HandleMessage(const FString &name, MemoryStream *stream) override;
		virtual void HandleMessage(const FString &name, const TArray<FVariant> &args) override;

	public:
		// for internal

		void EntityServerPos(FVector pos) { entityServerPos_ = pos; }
		Messages* pMessages() { return messages_; }
		NetworkInterfaceBase* pNetworkInterface() { return networkInterface_; }
		void OnLoseConnect();  // ʧȥ������������ӣ��������Ͽ���

		bool IsAcrossServer() { return isAcrossServer_; }

	private:
		void UpdatePlayerToServer();
		void ClearNetwork();
		MemoryStream* FindBufferedCreateEntityMessage(int32 entityID);
		int32 GetAoiEntityIDFromStream(MemoryStream &stream);
		void ClearEntities(bool isall);
		void ClearSpace(bool isall);
		void SendTick();


		void OnConnected(const FString& host, uint16 port, bool success);

		void CmdHello();
		void CmdImportClientMessages();
		void CmdImportClientEntityDef();

		void Client_onHelloCB(MemoryStream &stream);
		void Client_onVersionNotMatch(MemoryStream &stream);
		void Client_onScriptVersionNotMatch(MemoryStream &stream);
		void Client_onImportClientMessages(MemoryStream &stream);
		void Client_onImportClientEntityDef(MemoryStream &stream);
		
		void Client_onLoginBaseappFailed(uint16 failedcode);
		void Client_onLoginBaseappSuccessfully(MemoryStream &stream);
		void Client_onReloginBaseappFailed(uint16 failedcode);
		void Client_onReloginBaseappSuccessfully(MemoryStream &stream);
		void Client_onReqAccountBindEmailCB(uint16 failcode);
		void Client_onReqAccountNewPasswordCB(uint16 failcode);

		void Client_onAppActiveTickCB();
		void Client_onKicked(uint16 failedcode);
		void Client_onControlEntity(int32 eid, int8 isControlled);
		void Client_onCreatedProxies(uint64 rndUUID, int32 eid, FString entityType);
		void Client_onEntityDestroyed(int32 eid);

		// �ͻ�������ֵ�ı�֪ͨ
		void Client_onUpdatePropertysOptimized(MemoryStream &stream);
		void Client_onUpdatePropertys(MemoryStream &stream);
		void OnUpdatePropertys(int32 eid, MemoryStream& stream);

		// �ͻ���Զ�̷�������
		void Client_onRemoteMethodCallOptimized(MemoryStream &stream);
		void Client_onRemoteMethodCall(MemoryStream &stream);
		void OnRemoteMethodCall(int32 eid, MemoryStream& stream);

		// ���롢�뿪����
		void Client_onEntityEnterWorld(MemoryStream &stream);
		void Client_onEntityLeaveWorldOptimized(MemoryStream &stream);
		void Client_onEntityLeaveWorld(int32 eid);

		// ���롢�뿪��ǰ��ͼ
		void Client_onEntityEnterSpace(MemoryStream &stream);
		void Client_onEntityLeaveSpace(int32 eid);

		// ��ͼ�������
		void AddSpaceGeometryMapping(uint32 spaceID, const FString& respath);
		void Client_initSpaceData(MemoryStream &stream);
		void Client_setSpaceData(uint32 spaceID, const FString& key, const FString& value);
		void Client_delSpaceData(uint32 spaceID, const FString& key);

		/*
		����˸�����ҵĻ���λ�úͳ��� �ͻ������������λ�ü��ϱ���ֵ����������Χʵ�������
		*/
		void Client_onUpdateBasePos(float x, float y, float z);
		void Client_onUpdateBasePosXZ(float x, float z);
		void Client_onUpdateBaseDir(MemoryStream &stream);

		/*
		�����ǿ����������ҵ�����
		���磺�ڷ����ʹ��avatar.position=(0,0,0), �������λ�����ٶ��쳣ʱ��ǿ�����ص�һ��λ��
		*/
		void Client_onSetEntityPosAndDir(MemoryStream &stream);

		// ���ꡢ����ı���أ����Ż��߾���
		void Client_onUpdateData(MemoryStream &stream);
		void Client_onUpdateData_ypr(MemoryStream &stream);
		void Client_onUpdateData_yp(MemoryStream &stream);
		void Client_onUpdateData_yr(MemoryStream &stream);
		void Client_onUpdateData_pr(MemoryStream &stream);
		void Client_onUpdateData_y(MemoryStream &stream);
		void Client_onUpdateData_p(MemoryStream &stream);
		void Client_onUpdateData_r(MemoryStream &stream);
		void Client_onUpdateData_xz(MemoryStream &stream);
		void Client_onUpdateData_xz_ypr(MemoryStream &stream);
		void Client_onUpdateData_xz_yp(MemoryStream &stream);
		void Client_onUpdateData_xz_yr(MemoryStream &stream);
		void Client_onUpdateData_xz_pr(MemoryStream &stream);
		void Client_onUpdateData_xz_y(MemoryStream &stream);
		void Client_onUpdateData_xz_p(MemoryStream &stream);
		void Client_onUpdateData_xz_r(MemoryStream &stream);
		void Client_onUpdateData_xyz(MemoryStream &stream);
		void Client_onUpdateData_xyz_ypr(MemoryStream &stream);
		void Client_onUpdateData_xyz_yp(MemoryStream &stream);
		void Client_onUpdateData_xyz_yr(MemoryStream &stream);
		void Client_onUpdateData_xyz_pr(MemoryStream &stream);
		void Client_onUpdateData_xyz_y(MemoryStream &stream);
		void Client_onUpdateData_xyz_p(MemoryStream &stream);
		void Client_onUpdateData_xyz_r(MemoryStream &stream);

		// ���ꡢ����ı���أ����ݱ��Ż�ѹ��
		void Client_onUpdateData_optimized(MemoryStream &stream);
		void Client_onUpdateData_ypr_optimized(MemoryStream &stream);
		void Client_onUpdateData_yp_optimized(MemoryStream &stream);
		void Client_onUpdateData_yr_optimized(MemoryStream &stream);
		void Client_onUpdateData_pr_optimized(MemoryStream &stream);
		void Client_onUpdateData_y_optimized(MemoryStream &stream);
		void Client_onUpdateData_p_optimized(MemoryStream &stream);
		void Client_onUpdateData_r_optimized(MemoryStream &stream);
		void Client_onUpdateData_xz_optimized(MemoryStream &stream);
		void Client_onUpdateData_xz_ypr_optimized(MemoryStream &stream);
		void Client_onUpdateData_xz_yp_optimized(MemoryStream &stream);
		void Client_onUpdateData_xz_yr_optimized(MemoryStream &stream);
		void Client_onUpdateData_xz_pr_optimized(MemoryStream &stream);
		void Client_onUpdateData_xz_y_optimized(MemoryStream &stream);
		void Client_onUpdateData_xz_p_optimized(MemoryStream &stream);
		void Client_onUpdateData_xz_r_optimized(MemoryStream &stream);
		void Client_onUpdateData_xyz_optimized(MemoryStream &stream);
		void Client_onUpdateData_xyz_ypr_optimized(MemoryStream &stream);
		void Client_onUpdateData_xyz_yp_optimized(MemoryStream &stream);
		void Client_onUpdateData_xyz_yr_optimized(MemoryStream &stream);
		void Client_onUpdateData_xyz_pr_optimized(MemoryStream &stream);
		void Client_onUpdateData_xyz_y_optimized(MemoryStream &stream);
		void Client_onUpdateData_xyz_p_optimized(MemoryStream &stream);
		void Client_onUpdateData_xyz_r_optimized(MemoryStream &stream);
		void UpdateVolatileData(int32 entityID, float x, float y, float z, float yaw, float pitch, float roll, int8 isOnGround, bool isOptimized);

		// �������������
		void Client_onStreamDataStarted(int16 id, uint32 datasize, const FString& descr);
		void Client_onStreamDataRecv(MemoryStream &stream);
		void Client_onStreamDataCompleted(int16 id);

		// ������֪ͨ�ͻ��ˣ�ĳ��entity��parent�ı���
		void Client_onParentChanged(int32 eid, int32 parentID);

		// ���׼����ϣ����Ե�¼��
		void Client_acrossServerReady(MemoryStream &stream);

		bool NeedAdditionalUpdate(Entity *entity);
		void ResetAdditionalUpdateCount();
		bool PlayerNeedUpdate(Entity *entity, bool moveChanged);

	private:
		KBEngineApp* app_ = nullptr;

		// ��Ϣ������
		MessageReader *messageReader_ = nullptr;

		// ��Ϣ��ִ����
		Messages *messages_ = nullptr;

		NetworkInterfaceBase* networkInterface_ = nullptr;

		FString host_;
		uint16 tcpPort_;
		uint16 baseappUdpPort_;
		ConnectCallbackFunc connectedCallbackFunc_;

		// ��ǰ��ҵ�ʵ��id��ʵ�����
		uint64 entity_uuid_ = 0;
		int32 entity_id_ = 0;
		FString entity_type_ = "";

		// ��¼ʱ���˺��������Ϣ
		FString account_;
		FString password_;

		// space�����ݣ����忴API�ֲ����spaceData
		// https://github.com/kbengine/kbengine/tree/master/docs/api
		TMap<FString, FString> spaceDatas_;

		// ����ʵ�嶼��������� ��ο�API�ֲ����entities����
		// https://github.com/kbengine/kbengine/tree/master/docs/api
		TMap<int32, Entity*> entities_;

		// �����AOI��ΧС��256��ʵ��ʱ���ǿ���ͨ��һ�ֽ��������ҵ�entity
		TArray<int32> entityIDAliasIDList_;

		// controlledBy�����м�¼�ı����ؿͻ��˿��Ƶ�Entity���Լ����⣩
		TArray<Entity *> controlledEntities_;

		TMap<int32, MemoryStream*> bufferedCreateEntityMessage_;

		// ��ҵ�ǰ���ڿռ��id�� �Լ��ռ��Ӧ����Դ
		uint32 spaceID_ = 0;
		FString spaceResPath_ = "";
		bool isLoadedGeometry_ = false;

		// ��ǰ��������һ��ͬ�����������λ��
		FVector entityServerPos_ = FVector::ZeroVector;

		// ���һ����������ʱ�䡢���һ���յ������ظ���ʱ��
		// ������Ϊ�Ƿ���ߵ��ж�����
		FDateTime lastTicktime_ = FDateTime::UtcNow();
		FDateTime lastTickCBTime_ = FDateTime::UtcNow();

		// ���һ��ͬ�����ꡢ�������������ʱ�䣬���ڿ���ͬ��Ƶ��
		FDateTime lastUpdateToServerTime_ = FDateTime::UtcNow();
		
		// ���ֹͣ�ƶ��󣬶���������ͬ��λ�ó���Ĵ���
		uint8 additionalUpdateCount_ = BaseApp::ADDITIONAL_UPDATE_COUNT;
		
		// �Ƿ�����
		bool isAcrossServer_ = false;

	};  // end of class BaseApp;


}