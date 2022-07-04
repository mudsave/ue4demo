#include "BaseApp.h"
#include "KBEnginePrivatePCH.h"
#include "KBEngineApp.h"
#include "NetworkInterfaceBase.h"
#include "NetworkInterfaceTCP.h"
#include "NetworkInterfaceKCP.h"
#include "KBEDebug.h"
#include "Bundle.h"
#include "KBEPersonality.h"
#include "Entity.h"
#include "KBEMath.h"
#include "KBEErrors.h"
#include "EntityDef.h"
#include "ScriptModule.h"
#include "Mailbox.h"

namespace KBEngine
{
	BaseApp::BaseApp(KBEngineApp* app)
		: app_(app)
	{
		KBE_ASSERT(app_);

		messages_ = app->pMessages();
		messageReader_ = new MessageReader(this, messages_, KBEngineApp::app->GetTcpRecvBufferMax());
	}

	BaseApp::~BaseApp()
	{
		KBE_DEBUG(TEXT("BaseApp::~BaseApp()"));
		ClearNetwork();
		SAFE_DELETE(messageReader_);

		// �������Entity����
		for (auto it : bufferedCreateEntityMessage_)
			delete it.Value;
		bufferedCreateEntityMessage_.Reset();

		ClearEntities(true);
	}

	bool BaseApp::NetworkIsValid()
	{
		return networkInterface_ && networkInterface_->Valid();
	}

	void BaseApp::ClearNetwork()
	{
		if (networkInterface_)
		{
			networkInterface_->Close();
			delete networkInterface_;
			networkInterface_ = nullptr;
		}
	}

	Entity* BaseApp::Player()
	{
		auto **e = entities_.Find(entity_id_);
		if (e)
			return *e;

		return nullptr;
	}

	Entity* BaseApp::FindEntity(int32 entityID)
	{
		Entity** e = entities_.Find(entityID);
		if (e)
			return *e;

		return nullptr;
	}

	int32 BaseApp::GetAoiEntityIDFromStream(MemoryStream &stream)
	{
		if (!app_->UseAliasEntityID())
			return stream.ReadInt32();

		int32 id = 0;
		if (entityIDAliasIDList_.Num() > 255)
		{
			id = stream.ReadInt32();
		}
		else
		{
			byte aliasID = stream.ReadUint8();

			// ���Ϊ0�ҿͻ�����һ�����ص�½���������������ҷ����entity�ڶ����ڼ�һֱ��������״̬
			// ����Ժ����������, ��Ϊcellapp����һֱ����baseapp����ͬ����Ϣ�� ���ͻ���������ʱδ��
			// ����˳�ʼ�����迪ʼ���յ�ͬ����Ϣ, ��ʱ����ͻ����
			if (entityIDAliasIDList_.Num() <= aliasID)
				return 0;

			id = entityIDAliasIDList_[aliasID];
		}

		return id;
	}

	MemoryStream* BaseApp::FindBufferedCreateEntityMessage(int32 entityID)
	{
		MemoryStream **p = bufferedCreateEntityMessage_.Find(entityID);
		if (p)
			return *p;
		return nullptr;
	}

	void BaseApp::ClearEntities(bool isall)
	{
		controlledEntities_.Empty();
		if (!isall)
		{
			Entity* entity = Player();

			if (entity)
			{
				for (auto iter = entities_.CreateIterator(); iter; ++iter)
				{
					if (entity->ID() == iter.Key())
						continue;

					if (iter.Value()->InWorld())
						iter.Value()->LeaveWorld();

					iter.Value()->Destroy();
					iter.RemoveCurrent();
				}

			}
		}
		else
		{
			for (auto iter = entities_.CreateIterator(); iter; ++iter)
			{
				if (iter.Value()->InWorld())
					iter.Value()->LeaveWorld();

				iter.Value()->Destroy();
				iter.RemoveCurrent();
			}

		}
	}

	void BaseApp::ClearSpace(bool isall)
	{
		entityIDAliasIDList_.Reset();
		spaceDatas_.Reset();
		ClearEntities(isall);
		isLoadedGeometry_ = false;
		spaceID_ = 0;
	}

	bool BaseApp::NeedAdditionalUpdate(Entity *entity)
	{
		if (entity->Parent() != nullptr)
		{
			if (this->additionalUpdateCount_ > 0)
			{
				this->additionalUpdateCount_--;
				return true;
			}
		}

		return false;
	}

	void BaseApp::ResetAdditionalUpdateCount()
	{
		this->additionalUpdateCount_ = BaseApp::ADDITIONAL_UPDATE_COUNT;
	}

	bool BaseApp::PlayerNeedUpdate(Entity *entity, bool moveChanged)
	{
		bool needUpdate = true;
		if (moveChanged)
		{
			this->ResetAdditionalUpdateCount();
		}
		else
		{
			needUpdate = NeedAdditionalUpdate(entity);
		}

		return needUpdate;
	}

	void BaseApp::UpdatePlayerToServer()
	{
		if (!app_->SyncPlayer() || spaceID_ == 0)
		{
			return;
		}

		// ��ʱ����в������㣬������֤ÿ�뷢��10��
		auto now = FDateTime::UtcNow();
		auto span = now - lastUpdateToServerTime_;

		if (span.GetTicks() < 1000000)
			return;

		lastUpdateToServerTime_ = now - (span - FTimespan(1000000));

		Entity* playerEntity = Player();
		if (playerEntity == NULL || playerEntity->InWorld() == false)
			return;

		// ��ʼ��������Լ���������Ϣ
		if (!playerEntity->IsControlled())
		{
			const FVector &position = playerEntity->Position();
			const FVector &direction = playerEntity->Direction();

			bool posHasChanged;
			bool dirHasChanged;
			bool needUpdate;

			//��parent��������localλ�ã������Ƿ�ı�������Ҫ��Ҫͬ�������ҽ�local������������궼����������
			if (playerEntity->Parent() != nullptr)
			{
				const FVector &localPosition = playerEntity->localPosition_;
				const FVector &localDirection = playerEntity->localDirection_;

				posHasChanged = FVector::Dist(playerEntity->lastSyncLocalPos_, localPosition) > 0.1f;
				dirHasChanged = FVector::Dist(playerEntity->lastSyncLocalDir_, localDirection) > 0.1f;
				needUpdate = this->PlayerNeedUpdate(playerEntity, posHasChanged || dirHasChanged);
				if (needUpdate)
				{
					playerEntity->lastSyncLocalPos_ = localPosition;
					playerEntity->lastSyncLocalDir_ = localDirection;
					playerEntity->lastSyncPos_ = position;
					playerEntity->lastSyncDir_ = direction;

					Bundle* bundle = new Bundle();
					bundle->NewMessage(messages_->GetMessage("Baseapp_onUpdateDataFromClientOnParent"));
					bundle->WriteInt32(playerEntity->Parent()->ID());

					auto localPos = KBEMath::Unreal2KBEnginePosition(localPosition);

					bundle->WriteFloat(localPos.X);
					bundle->WriteFloat(localPos.Y);
					bundle->WriteFloat(localPos.Z);

					auto pos = KBEMath::Unreal2KBEnginePosition(position);

					bundle->WriteFloat(pos.X);
					bundle->WriteFloat(pos.Y);
					bundle->WriteFloat(pos.Z);

					auto dir = KBEMath::Unreal2KBEngineDirection(direction);

					bundle->WriteFloat(dir.X);
					bundle->WriteFloat(dir.Y);
					bundle->WriteFloat(dir.Z);
					bundle->WriteUint8((uint8)(playerEntity->IsOnGround() == true ? 1 : 0));
					bundle->WriteUint32(spaceID_);
					bundle->Send(networkInterface_);
					delete bundle;
				}

			}

			// û��parent������������λ�ã������Ƿ�ı�������Ҫ��Ҫͬ��������ֻ�����������������
			else
			{
				posHasChanged = FVector::Dist(playerEntity->lastSyncPos_, position) > 0.1f;
				dirHasChanged = FVector::Dist(playerEntity->lastSyncDir_, direction) > 0.1f;
				needUpdate = this->PlayerNeedUpdate(playerEntity, posHasChanged || dirHasChanged);
				if (needUpdate)
				{
					playerEntity->lastSyncPos_ = position;
					playerEntity->lastSyncDir_ = direction;

					auto pos = KBEMath::Unreal2KBEnginePosition(position);

					Bundle* bundle = new Bundle();
					bundle->NewMessage(messages_->GetMessage("Baseapp_onUpdateDataFromClient"));
					bundle->WriteFloat(pos.X);
					bundle->WriteFloat(pos.Y);
					bundle->WriteFloat(pos.Z);

					auto dir = KBEMath::Unreal2KBEngineDirection(direction);

					bundle->WriteFloat(dir.X);
					bundle->WriteFloat(dir.Y);
					bundle->WriteFloat(dir.Z);
					bundle->WriteUint8((uint8)(playerEntity->IsOnGround() == true ? 1 : 0));
					bundle->WriteUint32(spaceID_);
					bundle->Send(networkInterface_);
					delete bundle;
				}
			}
		}

		// ��ʼͬ�����б������˵�entity��λ��
		for (auto* entity : controlledEntities_)
		{
			const FVector &position = entity->Position();
			const FVector &direction = entity->Direction();

			bool posHasChanged;
			bool dirHasChanged;
			bool needUpdate;

			//��parent��������localλ�ã������Ƿ�ı�������Ҫ��Ҫͬ�������ҽ�local������������궼����������
			if (entity->Parent() != nullptr)
			{
				const FVector &localPosition = entity->localPosition_;
				const FVector &localDirection = entity->localDirection_;

				posHasChanged = FVector::Dist(entity->lastSyncLocalPos_, localPosition) > 0.1f;
				dirHasChanged = FVector::Dist(entity->lastSyncLocalDir_, localDirection) > 0.1f;
				needUpdate = this->PlayerNeedUpdate(entity, posHasChanged || dirHasChanged);
				if (needUpdate)
				{
					entity->lastSyncLocalPos_ = localPosition;
					entity->lastSyncLocalDir_ = localDirection;
					entity->lastSyncPos_ = position;
					entity->lastSyncDir_ = direction;

					Bundle* bundle = new Bundle();
					bundle->NewMessage(messages_->GetMessage("Baseapp_onUpdateDataFromClientForControlledEntityOnParent"));
					bundle->WriteInt32(entity->ID());
					bundle->WriteInt32(entity->Parent()->ID());

					auto localPos = KBEMath::Unreal2KBEnginePosition(localPosition);

					bundle->WriteFloat(localPos.X);
					bundle->WriteFloat(localPos.Y);
					bundle->WriteFloat(localPos.Z);

					auto pos = KBEMath::Unreal2KBEnginePosition(position);

					bundle->WriteFloat(pos.X);
					bundle->WriteFloat(pos.Y);
					bundle->WriteFloat(pos.Z);

					auto dir = KBEMath::Unreal2KBEngineDirection(direction);

					bundle->WriteFloat(dir.X);
					bundle->WriteFloat(dir.Y);
					bundle->WriteFloat(dir.Z);
					bundle->WriteUint8((uint8)(entity->IsOnGround() == true ? 1 : 0));
					bundle->WriteUint32(spaceID_);
					bundle->Send(networkInterface_);
					delete bundle;
				}

			}

			// û��parent������������λ�ã������Ƿ�ı�������Ҫ��Ҫͬ��������ֻ�����������������
			else
			{
				posHasChanged = FVector::Dist(entity->lastSyncPos_, position) > 0.1f;
				dirHasChanged = FVector::Dist(entity->lastSyncDir_, direction) > 0.1f;
				needUpdate = this->PlayerNeedUpdate(entity, posHasChanged || dirHasChanged);
				if (needUpdate)
				{
					entity->lastSyncPos_ = position;
					entity->lastSyncDir_ = direction;

					auto pos = KBEMath::Unreal2KBEnginePosition(position);

					Bundle* bundle = new Bundle();
					bundle->NewMessage(messages_->GetMessage("Baseapp_onUpdateDataFromClientForControlledEntity"));
					bundle->WriteInt32(entity->ID());
					bundle->WriteFloat(pos.X);
					bundle->WriteFloat(pos.Y);
					bundle->WriteFloat(pos.Z);

					auto dir = KBEMath::Unreal2KBEngineDirection(direction);

					bundle->WriteFloat(dir.X);
					bundle->WriteFloat(dir.Y);
					bundle->WriteFloat(dir.Z);
					bundle->WriteUint8((uint8)(entity->IsOnGround() == true ? 1 : 0));
					bundle->WriteUint32(spaceID_);
					bundle->Send(networkInterface_);
					delete bundle;
				}
			}
		}
	}

	void BaseApp::SendTick()
	{
		if (!messages_->BaseappMessageImported() || app_->TickInterval() == 0)
			return;

		auto span = FDateTime::UtcNow() - lastTicktime_;

		if (span.GetTotalSeconds() >= app_->TickInterval())
		{
			// phw: �����жϴ�����Ҫkbe 0.8.10���ϰ汾�ķ�����֧��
			span = lastTickCBTime_ - lastTicktime_;

			// ��������ص�����ʱ��С����������ʱ�䣬˵��û���յ��ص�
			// ��ʱӦ��֪ͨ�ͻ��˵�����
			if (span.GetTotalSeconds() < 0)
			{
				KBE_ERROR(TEXT("BaseApp::SendTick: Receive appTick timeout!lastTickCBTime_(%s) - lastTicktime_(%s) < 0, span(%s)"), 
					*lastTickCBTime_.ToString(), *lastTicktime_.ToString(), *span.ToString());
				networkInterface_->WillClose();
				return;
			}

			const Message* Baseapp_onClientActiveTickMsg = NULL;

			Baseapp_onClientActiveTickMsg = messages_->GetMessage("Baseapp_onClientActiveTick");

			if (Baseapp_onClientActiveTickMsg != NULL)
			{
				Bundle* bundle = new Bundle();
				bundle->NewMessage(messages_->GetMessage("Baseapp_onClientActiveTick"));
				bundle->Send(networkInterface_);
				delete bundle;
				//KBE_ERROR(TEXT("shufeng --->>> BaseApp::SendTick: send message Baseapp_onClientActiveTick time:%s:%d"), 
				//	*FDateTime::UtcNow().ToString(), FDateTime::UtcNow().GetMillisecond());
			}

			lastTicktime_ = FDateTime::UtcNow();
		}
	}






	void BaseApp::OnLoseConnect()
	{
		ClearEntities(true);
	}
	
	void BaseApp::Disconnect()
	{
		host_ = TEXT("");
		tcpPort_ = 0;
		connectedCallbackFunc_ = nullptr;

		ClearNetwork();
	}

	void BaseApp::Connect(const FString& host, uint16 tcpPort, uint16 udpPort, ConnectCallbackFunc func)
	{
		KBE_ASSERT(!networkInterface_);
		host_ = host;
		tcpPort_ = tcpPort;
		baseappUdpPort_ = udpPort;
		connectedCallbackFunc_ = func;

		uint16 connectPort = tcpPort_;
		if (KBEngineApp::app->IsForceDisableUDP() || baseappUdpPort_ == 0)
			networkInterface_ = new NetworkInterfaceTCP(messageReader_);
		else
		{
			networkInterface_ = new NetworkInterfaceKCP(messageReader_);
			connectPort = baseappUdpPort_;
		}

		networkInterface_->ConnectTo(host, connectPort, std::bind(&BaseApp::OnConnected, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	}

	void BaseApp::OnConnected(const FString& host, uint16 port, bool success)
	{
		if (!success)
		{
			KBE_ERROR(TEXT("BaseApp::OnConnected(): connect %s:%u is error!"), *host, port);
			ClearNetwork();
			if (connectedCallbackFunc_)
				connectedCallbackFunc_((int)ERROR_TYPE::CONNECT_TO_BASEAPP_FAULT);
			return;
		}

		KBE_DEBUG(TEXT("BaseApp::OnConnected(): connect %s:%u is success!"), *host, port);

		CmdHello();
	}

	void BaseApp::CmdHello()
	{
		KBE_DEBUG(TEXT("BaseApp::CmdHello: send Baseapp_hello ..."));
		Bundle* bundle = new Bundle();
		bundle->NewMessage(messages_->GetMessage("Baseapp_hello"));
		bundle->WriteString(app_->ClientVersion());
		bundle->WriteString(app_->ClientScriptVersion());
		bundle->WriteBlob(app_->EncryptedKey());
		bundle->Send(networkInterface_);
		delete bundle;
	}

	void BaseApp::Client_onHelloCB(MemoryStream &stream)
	{
		FString serverVersion = stream.ReadString();
		FString serverScriptVersion = stream.ReadString();
		FString serverProtocolMD5 = stream.ReadString();
		FString serverEntitydefMD5 = stream.ReadString();
		int32 ctype = stream.ReadInt32();

		KBE_DEBUG(TEXT("BaseApp::Client_onHelloCB: verInfo(%s), scriptVersion(%s), srvProtocolMD5(%s), srvEntitydefMD5(%s), + ctype(%d)!"),
			*serverVersion, *serverScriptVersion, *serverProtocolMD5, *serverEntitydefMD5, ctype);

		KBE_ASSERT(!messages_->BaseappMessageImported());
		KBE_ASSERT(!EntityDef::EntityDefImported());

		bool digestMatch = false;
		if (app_->pPersistentInofs())
			digestMatch = app_->pPersistentInofs()->OnServerDigest(SERVER_APP_TYPE::BaseApp, serverProtocolMD5, serverEntitydefMD5);

		KBE_DEBUG(TEXT("BaseApp::Client_onHelloCB: digest match(%s)"), digestMatch ? TEXT("true") : TEXT("false"));
		// ���Լ��ر�����Ϣ����
		bool success = digestMatch;
		if (success)
		{
			MemoryStream out;
			success = app_->pPersistentInofs()->LoadBaseappMessages(out);
			if (success)
				success = messages_->ImportMessagesFromStream(out, SERVER_APP_TYPE::BaseApp);
		}

		// ����ʧ���������������
		if (!success)
			CmdImportClientMessages();

		// ���Լ��ر���EntityDef����
		success = digestMatch;
		if (success)
		{
			MemoryStream out;
			success = app_->pPersistentInofs()->LoadEntityDef(out);
			if (success)
				success = EntityDef::ImportEntityDefFromStream(out);
		}

		// ����ʧ���������������
		if (!success)
			CmdImportClientEntityDef();

		if (messages_->BaseappMessageImported() && EntityDef::EntityDefImported() && connectedCallbackFunc_)
			connectedCallbackFunc_((int)ERROR_TYPE::SUCCESS);
	}

	void BaseApp::Client_onVersionNotMatch(MemoryStream &stream)
	{
		auto serverVersion = stream.ReadString();

		KBE_ERROR(TEXT("BaseApp::Client_onVersionNotMatch: verInfo=%s, server=%s"), *app_->ClientVersion(), *serverVersion);
		if (app_->pPersistentInofs() != NULL)
			app_->pPersistentInofs()->ClearAllMessageFiles();

		ClearNetwork();
		if (connectedCallbackFunc_)
			connectedCallbackFunc_((int)ERROR_TYPE::VERSION_NOT_MATCH);
	}

	void BaseApp::Client_onScriptVersionNotMatch(MemoryStream &stream)
	{
		auto serverScriptVersion = stream.ReadString();

		KBE_ERROR(TEXT("BaseApp::Client_onScriptVersionNotMatch: verInfo=%s, server=%s"), *app_->ClientScriptVersion(), *serverScriptVersion);
		if (app_->pPersistentInofs() != NULL)
			app_->pPersistentInofs()->ClearAllMessageFiles();

		ClearNetwork();
		if (connectedCallbackFunc_)
			connectedCallbackFunc_((int)ERROR_TYPE::SCRIPT_VERSION_NOT_MATCH);
	}


	void BaseApp::CmdImportClientMessages()
	{
		KBE_DEBUG(TEXT("BaseApp::CmdImportClientMessages: send Baseapp_importClientMessages ..."));
		Bundle* bundle = new Bundle();
		bundle->NewMessage(messages_->GetMessage("Baseapp_importClientMessages"));
		bundle->Send(networkInterface_);
		delete bundle;
	}

	void BaseApp::Client_onImportClientMessages(MemoryStream &stream)
	{
		KBE_DEBUG(TEXT("BaseApp::Client_onImportClientMessages: stream size: %d"), stream.Length());

		// �ȸ���һ�ݳ�������Ϊд����׼��
		MemoryStream datas(stream);

		messages_->ImportMessagesFromStream(stream, SERVER_APP_TYPE::BaseApp);

		if (app_->pPersistentInofs() != NULL)
		{
			app_->pPersistentInofs()->WriteBaseappMessages(datas);
		}

		if (EntityDef::EntityDefImported() && connectedCallbackFunc_)
			connectedCallbackFunc_((int)ERROR_TYPE::SUCCESS);
	}

	void BaseApp::CmdImportClientEntityDef()
	{
		KBE_DEBUG(TEXT("BaseApp::CmdImportClientEntityDef: send Baseapp_importClientEntityDef ..."));
		Bundle* bundle = new Bundle();
		bundle->NewMessage(messages_->GetMessage("Baseapp_importClientEntityDef"));
		bundle->Send(networkInterface_);
		delete bundle;
	}

	void BaseApp::Client_onImportClientEntityDef(MemoryStream &stream)
	{
		KBE_DEBUG(TEXT("BaseApp::Client_onImportClientEntityDef: stream size: %d"), stream.Length());

		// �ȸ���һ�ݳ�������Ϊд����׼��
		MemoryStream datas(stream);

		EntityDef::ImportEntityDefFromStream(stream);

		if (app_->pPersistentInofs())
		{
			app_->pPersistentInofs()->WriteEntityDef(datas);
		}

		if (messages_->BaseappMessageImported() && connectedCallbackFunc_)
			connectedCallbackFunc_((int)ERROR_TYPE::SUCCESS);
	}

	void BaseApp::Login(const FString& account, const FString& password, ConnectCallbackFunc func)
	{
		if (!networkInterface_)
		{
			if (func)
				func(-4);
			return;
		}
		
		//KBE_DEBUG(TEXT("BaseApp::Login(): send login! username=%s"), *account);
		account_ = account;
		password_ = password;
		connectedCallbackFunc_ = func;
		Bundle* bundle = new Bundle();
		bundle->NewMessage(messages_->GetMessage("Baseapp_loginBaseapp"));
		bundle->WriteString(account);
		bundle->WriteString(password);
		bundle->Send(networkInterface_);
		delete bundle;
	}

	void BaseApp::Relogin(ConnectCallbackFunc func)
	{
		KBE_ASSERT(NetworkIsValid());

		connectedCallbackFunc_ = func;
		Bundle* bundle = new Bundle();
		bundle->NewMessage(messages_->GetMessage("Baseapp_reloginBaseapp"));
		bundle->WriteString(account_);
		bundle->WriteString(password_);
		bundle->WriteUint64(entity_uuid_);
		bundle->WriteInt32(entity_id_);
		bundle->Send(networkInterface_);
		delete bundle;
	}

	void BaseApp::AcrossLogin(FString& accountName, FString& password, CLIENT_TYPE clientType, UINT16 loginKey, ConnectCallbackFunc func)
	{
		connectedCallbackFunc_ = func;
		Bundle* bundle = new Bundle();
		bundle->NewMessage(messages_->GetMessage("Baseapp_acrossLogin"));
		bundle->WriteString(accountName);
		bundle->WriteString(password);
		bundle->WriteInt8((int8)clientType);
		bundle->WriteUint64(loginKey);
		bundle->Send(networkInterface_);
		delete bundle;
	}

	void BaseApp::AcrossLoginSuccess()
	{
		isAcrossServer_ = true;
	}

	void BaseApp::Client_onLoginBaseappFailed(uint16 failedcode)
	{
		KBE_ERROR(TEXT("BaseApp::Client_onLoginBaseappFailed: failedcode(%d)!"), failedcode);
		if (connectedCallbackFunc_)
			connectedCallbackFunc_(failedcode);
	}

	void BaseApp::Client_onLoginBaseappSuccessfully(MemoryStream &stream) 
	{
		entity_uuid_ = stream.ReadUint64();
		KBE_DEBUG(TEXT("BaseApp::Client_onLoginBaseappSuccessfully: name(%s)!"), *account_);
		if (connectedCallbackFunc_)
			connectedCallbackFunc_((int32)ERROR_TYPE::SUCCESS);
	}

	void BaseApp::Client_onReloginBaseappFailed(uint16 failedcode)
	{
		KBE_ERROR(TEXT("BaseApp::Client_onReloginBaseappFailed: failed code(%d)!"), failedcode);
		if (connectedCallbackFunc_)
			connectedCallbackFunc_(failedcode);
	}

	void BaseApp::Client_onReloginBaseappSuccessfully(MemoryStream &stream)
	{
		entity_uuid_ = stream.ReadUint64();
		KBE_DEBUG(TEXT("BaseApp::Client_onReloginBaseappSuccessfully: name(%s)!"), *account_);
		if (connectedCallbackFunc_)
			connectedCallbackFunc_((int32)ERROR_TYPE::SUCCESS);
	}

	void BaseApp::BindAccountEmail(const FString& emailAddress, ConnectCallbackFunc func)
	{
		connectedCallbackFunc_ = func;

		Bundle* bundle = new Bundle();
		bundle->NewMessage(messages_->GetMessage("Baseapp_reqAccountBindEmail"));
		bundle->WriteInt32(entity_id_);
		bundle->WriteString(password_);
		bundle->WriteString(emailAddress);
		bundle->Send(networkInterface_);
		delete bundle;
	}

	void BaseApp::Client_onReqAccountBindEmailCB(uint16 failcode)
	{
		if (failcode != (int)ERROR_TYPE::SUCCESS)
		{
			KBE_ERROR(TEXT("BaseApp::Client_onReqAccountBindEmailCB: '%s' is failed! code=%d!"), *account_, failcode);
		}
		else
		{
			KBE_DEBUG(TEXT("BaseApp::Client_onReqAccountBindEmailCB: '%s' is successfully!"), *account_);
		}

		if (connectedCallbackFunc_)
			connectedCallbackFunc_(failcode);
	}

	// �޸�����
	void BaseApp::NewPassword(const FString& old_password, const FString& new_password, ConnectCallbackFunc func)
	{
		connectedCallbackFunc_ = func;

		Bundle* bundle = new Bundle();
		bundle->NewMessage(messages_->GetMessage("Baseapp_reqAccountNewPassword"));
		bundle->WriteInt32(entity_id_);
		bundle->WriteString(old_password);
		bundle->WriteString(new_password);
		bundle->Send(networkInterface_);
		delete bundle;
	}

	void BaseApp::Client_onReqAccountNewPasswordCB(uint16 failcode)
	{
		if (failcode != (int)ERROR_TYPE::SUCCESS)
		{
			KBE_ERROR(TEXT("BaseApp::Client_onReqAccountNewPasswordCB: '%s' is failed! code=%d!"), *account_, failcode);
		}
		else
		{
			KBE_DEBUG(TEXT("BaseApp::Client_onReqAccountNewPasswordCB: '%s' is successfully!"), *account_);
		}

		if (connectedCallbackFunc_)
			connectedCallbackFunc_(failcode);
	}

	void BaseApp::Client_onControlEntity(int32 eid, int8 isControlled)
	{
		Entity **pp = entities_.Find(eid);
		if (!pp)
		{
			KBE_ERROR(TEXT("BaseApp::Client_onControlEntity: entity(%d) not found!"), eid);
			return;
		}

		Entity *entity = *pp;

		bool isCont = isControlled != 0;
		if (isCont)
		{
			// �����������������Լ����Ǳ�ʾ����Լ��������˿�����
			// ��������Լ���Ӧ�ý�������������б�
			if (Player()->ID() != entity->ID())
			{
				controlledEntities_.Add(entity);
			}
		}
		else
		{
			controlledEntities_.Remove(entity);
		}

		entity->SetControlled(isCont);
	}

	void BaseApp::Client_onKicked(uint16 failedcode)
	{
		KBE_DEBUG(TEXT("BaseApp::Client_onKicked: failedcode=%d(%s)"), failedcode, *KBEErrors::ErrorName(failedcode));
	}

	void BaseApp::Client_onCreatedProxies(uint64 rndUUID, int32 eid, FString entityType)
	{
		KBE_DEBUG(TEXT("BaseApp::Client_onCreatedProxies: eid(%d), entityType(%s)!"), eid, *entityType);

		entity_uuid_ = rndUUID;
		entity_id_ = eid;
		entity_type_ = entityType;

		if (entities_.Contains(eid))
		{
			//KBE_WARNING(TEXT("BaseApp::Client_onCreatedProxies: eid(%d) has exist!"), eid);
			MemoryStream *entityMessage = FindBufferedCreateEntityMessage(eid);

			if (entityMessage)
			{
				Client_onUpdatePropertys(*entityMessage);
				bufferedCreateEntityMessage_.Remove(eid);
				SAFE_DELETE(entityMessage);
			}
			return;
		}

		ScriptModule* module = EntityDef::GetScriptModule(entityType);
		if (!module)
		{
			KBE_ERROR(TEXT("BaseApp::Client_onCreatedProxies: not found module '%s'!"), *entityType);
			return;
		}

		Entity* entity = module->CreateEntity(eid);
		if (!entity)
		{
			KBE_ERROR(TEXT("BaseApp::Client_onCreatedProxies: module '%s' has no entity script!"), *entityType);
			return;
		}

		auto *mb = new Mailbox(eid, entityType, Mailbox::MAILBOX_TYPE::MAILBOX_TYPE_BASE);
		entity->BaseMailbox(mb);

		entities_.Add(eid, entity);

		MemoryStream* entityMessage = FindBufferedCreateEntityMessage(eid);
		if (entityMessage != NULL)
		{
			Client_onUpdatePropertys(*entityMessage);
			bufferedCreateEntityMessage_.Remove(eid);
			SAFE_DELETE(entityMessage);
		}

		entity->__init__();
		entity->Inited(true);

		if (app_->IsOnInitCallPropertysSetMethods())
			entity->CallPropertysSetMethods();
	}

	void BaseApp::Client_onUpdatePropertysOptimized(MemoryStream &stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);
		OnUpdatePropertys(eid, stream);
	}

	void BaseApp::Client_onUpdatePropertys(MemoryStream &stream)
	{
		int32 eid = stream.ReadInt32();
		OnUpdatePropertys(eid, stream);
	}

	void BaseApp::OnUpdatePropertys(int32 eid, MemoryStream& stream)
	{
		Entity* entity = FindEntity(eid);

		if (!entity)
		{
			MemoryStream* entityMessage = FindBufferedCreateEntityMessage(eid);
			if (entityMessage)
			{
				KBE_ERROR(TEXT("BaseApp::OnUpdatePropertys: entity(%d) has more then one buffer message, has bug?"), eid);
				return;
			}

			MemoryStream* stream1 = new MemoryStream();
			stream1->Append(&(stream.Data()[stream.RPos() - 4]), stream.Length() + 4);
			bufferedCreateEntityMessage_.Add(eid, stream1);
			return;
		}

		ScriptModule* sm = EntityDef::GetScriptModule(entity->ClassName());

		while (stream.Length() > 0)
		{
			uint16 utype = 0;

			if (sm->UsePropertyDescrAlias())
			{
				utype = stream.ReadUint8();
			}
			else
			{
				utype = stream.ReadUint16();
			}

			Property* propertydata = sm->GetProperty(utype);
			utype = propertydata->properUtype;
			PropertyHandler setmethod = propertydata->setmethod;

			FVariant val = propertydata->utype->CreateFromStream(&stream);
			FVariant oldval = entity->GetDefinedPropertyByUType(utype);

			//KBE_DEBUG(TEXT("BaseApp::OnUpdatePropertys: %s(id=%d %s=%s), hasSetMethod=%p!"), *entity.className, eid, *propertydata.name, FVariant2FString(val), setmethod);

			if (propertydata->name == TEXT("position"))
			{
				FVector newV = val.GetValue<FVector>();
				entity->OnPositionSet(KBEMath::KBEngine2UnrealPosition(newV));
			}
			else if (propertydata->name == TEXT("direction"))
			{
				FVector newV = val.GetValue<FVector>();
				entity->OnDirectionSet(KBEMath::KBEngine2UnrealDirection(newV));
			}
			else
			{

				entity->SetDefinedPropertyByUType(utype, val);

				//if (!setmethod)
				//	return;

				if (propertydata->IsBase())
				{
					if (entity->Inited())
					{
						//setmethod(entity, oldval);
						entity->OnUpdateProperty(propertydata->name, val, oldval);
					}
				}
				else
				{
					if (entity->InWorld())
					{
						//setmethod(entity, oldval);
						entity->OnUpdateProperty(propertydata->name, val, oldval);
					}
				}
			}
		}
	}

	void BaseApp::Client_onRemoteMethodCallOptimized(MemoryStream &stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);
		OnRemoteMethodCall(eid, stream);
	}

	void BaseApp::Client_onRemoteMethodCall(MemoryStream &stream)
	{
		int32 eid = stream.ReadInt32();
		OnRemoteMethodCall(eid, stream);
	}

	void BaseApp::OnRemoteMethodCall(int32 eid, MemoryStream& stream)
	{
		Entity* entity = FindEntity(eid);

		if (!entity)
		{
			KBE_ERROR(TEXT("BaseApp::OnRemoteMethodCall: entity(%d) not found!"), eid);
			return;
		}

		uint16 methodUtype = 0;
		ScriptModule *module = EntityDef::GetScriptModule(entity->ClassName());
		if (!module)
		{
			KBE_ERROR(TEXT("BaseApp::OnRemoteMethodCall: module(%s) not found! entity id: %d"), *entity->ClassName(), eid);
			return;
		}

		if (module->UseMethodDescrAlias())
			methodUtype = stream.ReadUint8();
		else
			methodUtype = stream.ReadUint16();

		Method* methoddata = module->GetMethod(methodUtype);

		//KBE_DEBUG(TEXT("BaseApp::OnRemoteMethodCall: %s.%s"), *entity->ClassName(), *methoddata->name);

		FVariantArray args;
		args.SetNum(methoddata->args.Num());

		for (int i = 0; i<methoddata->args.Num(); i++)
		{
			args[i] = methoddata->args[i]->CreateFromStream(&stream);
		}

		//methoddata.handler.Invoke(entity, args);
		entity->RemoteMethodCall(methoddata->name, args);
	}

	void BaseApp::Client_onEntityEnterWorld(MemoryStream &stream)
	{
		int32 eid = stream.ReadInt32();
		if (entity_id_ > 0 && entity_id_ != eid)
			entityIDAliasIDList_.Add(eid);

		uint16 uentityType;
		if (EntityDef::ScriptModuleNum() > 255)
			uentityType = stream.ReadUint16();
		else
			uentityType = stream.ReadUint8();

		int8 isOnGround = 1;

		if (stream.Length() > 0)
			isOnGround = stream.ReadInt8();

		int32 parentID = 0;
		FVector localposition(0.0);
		FVector localdirection(0.0);
		if (stream.Length() > 0)
		{
			parentID = stream.ReadInt32();
			localposition.X = stream.ReadFloat();
			localposition.Y = stream.ReadFloat();
			localposition.Z = stream.ReadFloat();

			localdirection.X = stream.ReadFloat();
			localdirection.Y = stream.ReadFloat();
			localdirection.Z = stream.ReadFloat();
		}
			
		const FString& entityType = EntityDef::GetScriptModule(uentityType)->Name();
		KBE_DEBUG(TEXT("BaseApp::Client_onEntityEnterWorld: %s(%d), spaceID(%d)!"), *entityType, eid, spaceID_);

		Entity* entity = FindEntity(eid);
		if (!entity)
		{
			MemoryStream* entityMessage = FindBufferedCreateEntityMessage(eid);
			if (!entityMessage)
			{
				KBE_ERROR(TEXT("BaseApp::Client_onEntityEnterWorld: entity(%d) not found!"), eid);
				return;
			}

			ScriptModule* module = EntityDef::GetScriptModule(entityType);
			if (!module)
			{
				KBE_ERROR(TEXT("BaseApp::Client_onEntityEnterWorld: not found module '%s'!"), *entityType);
				bufferedCreateEntityMessage_.Remove(eid);
				SAFE_DELETE(entityMessage);
				return;
			}

			entity = module->CreateEntity(eid);
			if (!entity)
			{
				KBE_ERROR(TEXT("BaseApp::Client_onEntityEnterWorld: module '%s' has no entity script!"), *entityType);
				bufferedCreateEntityMessage_.Remove(eid);
				SAFE_DELETE(entityMessage);
				return;
			}

			auto *mb = new Mailbox(eid, entityType, Mailbox::MAILBOX_TYPE::MAILBOX_TYPE_CELL);
			entity->CellMailbox(mb);

			entities_.Add(eid, entity);

			Client_onUpdatePropertys(*entityMessage);
			bufferedCreateEntityMessage_.Remove(eid);
			SAFE_DELETE(entityMessage);

			entity->IsOnGround(isOnGround > 0);

			if (parentID > 0)
			{
				// ���ܸ������Ƿ���ڣ���Ӧ���������
				entity->parentID_ = parentID;

				Entity* parentEntity = FindEntity(parentID);
				if (parentEntity)
				{
					entity->SetParentOnEnterWorld(parentEntity, KBEMath::KBEngine2UnrealPosition(localposition), KBEMath::KBEngine2UnrealDirection(localdirection));
				}
				else
				{
					// @TODO(penghuawei): ���ｫ����Ҫ���ǵȸ������������һ����֣��Ա����ӽ��ϵĲ������飬
					//                    ��Ȼ��Ҳ������ʹ�����Լ���entity.enterWorld()�������parentID��parent״̬�Լ�����
				}
			}

			entity->__init__();
			entity->Inited(true);
			entity->InWorld(true);
			entity->EnterWorld();

			if (app_->IsOnInitCallPropertysSetMethods())
				entity->CallPropertysSetMethods();

			// ֪ͨ����entity�����µ�entity�����ˣ�
			// ��������������entity������entity��ָ����
			// @TODO(penghuawei): ��ǰ�ȼ򵥱�����������Կ����Ż���ֻ���и������entity�㲥���ԉ������ܡ�
			for (auto iter : entities_)
			{
				if (iter.Value->parentID_ == entity->ID())
					iter.Value->SetParent(entity);
			}
		}
		else
		{
			// �����ϣ�ֻ����Ҷ���������ʱ��Ż��ߵ�����
			//KBE_ASSERT(entity->ID() == entity_id_);		//���changeToRealʱ��Ұ������entityҲ���ߵ�����������Ҫע�͵�
			if (!entity->InWorld())
			{
				KBE_DEBUG(TEXT("BaseApp::Client_onEntityEnterWorld: entity(%d) EnterWorld."), eid);

				// ��ȫ����� �������һ��
				// ����������ʹ��giveClientTo�л�����Ȩ
				// ֮ǰ��ʵ���Ѿ��������磬 �л����ʵ��Ҳ�������磬 ������ܻ����֮ǰ�Ǹ�ʵ������������Ϣ
				entityIDAliasIDList_.Reset();
				ClearEntities(false);
				entities_.Add(entity->ID(), entity);

				auto *mb = new Mailbox(eid, entityType, Mailbox::MAILBOX_TYPE::MAILBOX_TYPE_CELL);
				entity->CellMailbox(mb);

				entity->IsOnGround(isOnGround > 0);
				entity->InWorld(true);
				entity->EnterWorld();

				if (app_->IsOnInitCallPropertysSetMethods())
					entity->CallPropertysSetMethods();
			}
			else
			{
				KBE_ERROR(TEXT("BaseApp::Client_onEntityEnterWorld: entity(%d) re-EnterWorld!!!"), eid);
			}
		}
	}

	void BaseApp::Client_onEntityLeaveWorldOptimized(MemoryStream &stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);
		Client_onEntityLeaveWorld(eid);
	}

	void BaseApp::Client_onEntityLeaveWorld(int32 eid)
	{
		Entity* entity = FindEntity(eid);
		if (!entity)
		{
			KBE_ERROR(TEXT("BaseApp::Client_onEntityLeaveWorld: entity(%d) not found!"), eid);
			return;
		}
		
		KBE_DEBUG(TEXT("BaseApp::Client_onEntityLeaveWorld: %s(%d)"), *entity->ClassName(), eid);
		if (entity->InWorld())
			entity->LeaveWorld();

		if (entity_id_ == eid)
		{
			ClearSpace(false);
			entity->CellMailbox(nullptr);
		}
		else
		{
			controlledEntities_.Remove(entity);
			entities_.Remove(eid);
			entity->Destroy();
			entityIDAliasIDList_.Remove(eid);
		}
	}

	void BaseApp::Client_onEntityEnterSpace(MemoryStream &stream)
	{
		int32 eid = stream.ReadInt32();
		spaceID_ = stream.ReadUint32();

		int8 isOnGround = 1;

		if (stream.Length() > 0)
			isOnGround = stream.ReadInt8();

		Entity* entity = FindEntity(eid);
		if (!entity)
		{
			KBE_ERROR(TEXT("BaseApp::Client_onEntityEnterSpace: entity(%d) not found!"), eid);
			return;
		}

		entity->IsOnGround(isOnGround > 0);
		entityServerPos_ = entity->Position();
		entity->EnterSpace();
	}

	void BaseApp::Client_onEntityLeaveSpace(int32 eid)
	{
		Entity* entity = FindEntity(eid);
		if (!entity)
		{
			KBE_ERROR(TEXT("BaseApp::Client_onEntityLeaveSpace: entity(%d) not found!"), eid);
			return;
		}

		entity->LeaveSpace();
		ClearSpace(false);
	}

	void BaseApp::AddSpaceGeometryMapping(uint32 spaceID, const FString& respath)
	{
		KBE_DEBUG(TEXT("BaseApp::AddSpaceGeometryMapping: spaceID(%u), respath(%s)!"), spaceID, *respath);

		isLoadedGeometry_ = true;
		spaceID_ = spaceID;
		spaceResPath_ = respath;

		if (KBEPersonality::Instance())
			KBEPersonality::Instance()->OnAddSpaceGeometryMapping(spaceID, respath);
	}

	void BaseApp::Client_initSpaceData(MemoryStream &stream)
	{
		ClearSpace(false);
		spaceID_ = stream.ReadUint32();

		while (stream.Length() > 0)
		{
			FString key = stream.ReadString();
			FString val = stream.ReadString();
			Client_setSpaceData(spaceID_, key, val);
		}

		KBE_DEBUG(TEXT("BaseApp::Client_initSpaceData: spaceID(%d), size(%d)!"), spaceID_, spaceDatas_.Num());
	}

	void BaseApp::Client_setSpaceData(uint32 spaceID, const FString& key, const FString& value)
	{
		KBE_DEBUG(TEXT("BaseApp::Client_setSpaceData: spaceID(%u), key(%s), value(%s)!"), spaceID, *key, *value);
		spaceDatas_.Add(key, value);

		if (key == "_mapping")
			AddSpaceGeometryMapping(spaceID, value);

		if (KBEPersonality::Instance())
			KBEPersonality::Instance()->OnSetSpaceData(spaceID, key, value);
	}

	void BaseApp::Client_delSpaceData(uint32 spaceID, const FString& key)
	{
		KBE_DEBUG(TEXT("BaseApp::Client_delSpaceData: spaceID(%u), key(%s)!"), spaceID, *key);
		spaceDatas_.Remove(key);

		if (KBEPersonality::Instance())
			KBEPersonality::Instance()->OnDelSpaceData(spaceID, key);
	}

	const FString& BaseApp::GetSpaceData(const FString& key)
	{
		FString* val = spaceDatas_.Find(key);

		if (!val)
		{
			static FString s_null_data;
			return s_null_data;
		}

		return *val;
	}

	void BaseApp::Client_onEntityDestroyed(int32 eid)
	{
		KBE_DEBUG(TEXT("BaseApp::Client_onEntityDestroyed: entity(%d)"), eid);

		Entity* entity = FindEntity(eid);
		if (!entity)
		{
			KBE_ERROR(TEXT("BaseApp::Client_onEntityDestroyed: entity(%d) not found!"), eid);
			return;
		}


		if (entity->InWorld())
		{
			if (entity_id_ == eid)
				ClearSpace(false);

			entity->LeaveWorld();
		}

		controlledEntities_.Remove(entity);
		entities_.Remove(eid);
		entity->Destroy();
	}

	void BaseApp::Client_onUpdateBasePos(float x, float y, float z)
	{
		FVector position(x, y, z);
		entityServerPos_ = KBEMath::KBEngine2UnrealPosition(position);

		Entity* entity = Player();
		if (entity && entity->IsControlled())
		{
			entity->position_ = entityServerPos_;

			if (entity->Parent())
				entity->localPosition_ = entity->Parent()->PositionWorldToLocal(entityServerPos_);
			else
				entity->localPosition_ = entityServerPos_;
			entity->SyncVolatileDataToChildren(true);

			entity->OnUpdateVolatileData();
		}
	}

	void BaseApp::Client_onUpdateBasePosXZ(float x, float z)
	{
		FVector old = KBEMath::Unreal2KBEnginePosition(entityServerPos_);
		Client_onUpdateBasePos(x, old.Y, z);
	}

	void BaseApp::Client_onUpdateBaseDir(MemoryStream &stream)
	{
		FVector direction(0.0);
		direction.X = stream.ReadFloat();
		direction.Y = stream.ReadFloat();
		direction.Z = stream.ReadFloat();

		auto dir = KBEMath::KBEngine2UnrealDirection(direction);

		Entity* entity = Player();
		if (entity && entity->IsControlled())
		{
			entity->direction_ = dir;

			if (entity->Parent())
				entity->localDirection_ = entity->Parent()->DirectionWorldToLocal(dir);
			else
				entity->localDirection_ = dir;
			entity->SyncVolatileDataToChildren(false);

			entity->OnUpdateVolatileData();
		}
	}

	void BaseApp::Client_onUpdateData(MemoryStream &stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		Entity* entity = FindEntity(eid);
		if (!entity)
		{
			KBE_ERROR(TEXT("BaseApp::Client_onUpdateData: entity(%d) not found!"), eid);
			return;
		}
	}

	void BaseApp::Client_onSetEntityPosAndDir(MemoryStream &stream)
	{
		int32 eid = stream.ReadInt32();

		Entity* entity = FindEntity(eid);
		if (!entity)
		{
			KBE_ERROR(TEXT("BaseApp::Client_onSetEntityPosAndDir: entity(%d) not found!"), eid);
			return;
		}

		FVector position(0.0);
		FVector direction(0.0);

		position.X = stream.ReadFloat();
		position.Y = stream.ReadFloat();
		position.Z = stream.ReadFloat();

		direction.X = stream.ReadFloat();
		direction.Y = stream.ReadFloat();
		direction.Z = stream.ReadFloat();

		entity->OnPositionSet(KBEMath::KBEngine2UnrealPosition(position));
		entity->OnDirectionSet(KBEMath::KBEngine2UnrealDirection(direction));
	}

	void BaseApp::Client_onUpdateData_ypr_optimized(MemoryStream &stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		int8 y = stream.ReadInt8();
		int8 p = stream.ReadInt8();
		int8 r = stream.ReadInt8();

		UpdateVolatileData(eid,FLT_MAX, FLT_MAX, FLT_MAX, y, p, r, -1, true);
	}

	void BaseApp::Client_onUpdateData_yp_optimized(MemoryStream &stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		int8 y = stream.ReadInt8();
		int8 p = stream.ReadInt8();

		UpdateVolatileData(eid, FLT_MAX, FLT_MAX, FLT_MAX, y, p, KBEDATATYPE_BASE::KBE_FLT_MAX, -1, true);
	}

	void BaseApp::Client_onUpdateData_yr_optimized(MemoryStream &stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		int8 y = stream.ReadInt8();
		int8 r = stream.ReadInt8();

		UpdateVolatileData(eid, FLT_MAX, FLT_MAX, FLT_MAX, y, KBEDATATYPE_BASE::KBE_FLT_MAX, r, -1, true);
	}

	void BaseApp::Client_onUpdateData_pr_optimized(MemoryStream &stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		int8 p = stream.ReadInt8();
		int8 r = stream.ReadInt8();

		UpdateVolatileData(eid, FLT_MAX, FLT_MAX, FLT_MAX, KBEDATATYPE_BASE::KBE_FLT_MAX, p, r, -1, true);
	}

	void BaseApp::Client_onUpdateData_y_optimized(MemoryStream &stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		float y = stream.ReadPackY();

		UpdateVolatileData(eid, FLT_MAX, FLT_MAX, FLT_MAX, y, KBEDATATYPE_BASE::KBE_FLT_MAX, KBEDATATYPE_BASE::KBE_FLT_MAX, -1, true);
	}

	void BaseApp::Client_onUpdateData_p_optimized(MemoryStream &stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		int8 p = stream.ReadInt8();

		UpdateVolatileData(eid, FLT_MAX, FLT_MAX, FLT_MAX, KBEDATATYPE_BASE::KBE_FLT_MAX, p, KBEDATATYPE_BASE::KBE_FLT_MAX, -1, true);
	}

	void BaseApp::Client_onUpdateData_r_optimized(MemoryStream &stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		int8 r = stream.ReadInt8();

		UpdateVolatileData(eid, FLT_MAX, FLT_MAX, FLT_MAX, KBEDATATYPE_BASE::KBE_FLT_MAX, KBEDATATYPE_BASE::KBE_FLT_MAX, r, -1, true);
	}

	void BaseApp::Client_onUpdateData_xz_optimized(MemoryStream &stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		FVector2D xz = stream.ReadPackXZ();

		UpdateVolatileData(eid, xz[0], FLT_MAX, xz[1], KBEDATATYPE_BASE::KBE_FLT_MAX, KBEDATATYPE_BASE::KBE_FLT_MAX, KBEDATATYPE_BASE::KBE_FLT_MAX, 1, true);
	}

	void BaseApp::Client_onUpdateData_xz_ypr_optimized(MemoryStream &stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		FVector2D xz = stream.ReadPackXZ();

		int8 y = stream.ReadInt8();
		int8 p = stream.ReadInt8();
		int8 r = stream.ReadInt8();

		UpdateVolatileData(eid, xz[0], FLT_MAX, xz[1], y, p, r, 1, true);
	}

	void BaseApp::Client_onUpdateData_xz_yp_optimized(MemoryStream &stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		FVector2D xz = stream.ReadPackXZ();

		int8 y = stream.ReadInt8();
		int8 p = stream.ReadInt8();

		UpdateVolatileData(eid, xz[0], FLT_MAX, xz[1], y, p, KBEDATATYPE_BASE::KBE_FLT_MAX, 1, true);
	}

	void BaseApp::Client_onUpdateData_xz_yr_optimized(MemoryStream &stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		FVector2D xz = stream.ReadPackXZ();

		int8 y = stream.ReadInt8();
		int8 r = stream.ReadInt8();

		UpdateVolatileData(eid, xz[0], FLT_MAX, xz[1], y, KBEDATATYPE_BASE::KBE_FLT_MAX, r, 1, true);
	}

	void BaseApp::Client_onUpdateData_xz_pr_optimized(MemoryStream &stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		FVector2D xz = stream.ReadPackXZ();

		int8 p = stream.ReadInt8();
		int8 r = stream.ReadInt8();

		UpdateVolatileData(eid, xz[0], FLT_MAX, xz[1], KBEDATATYPE_BASE::KBE_FLT_MAX, p, r, 1, true);
	}

	void BaseApp::Client_onUpdateData_xz_y_optimized(MemoryStream &stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);
		FVector2D xz = stream.ReadPackXZ();
		int8 yaw = stream.ReadInt8();
		UpdateVolatileData(eid, xz[0], FLT_MAX, xz[1], yaw, KBEDATATYPE_BASE::KBE_FLT_MAX, KBEDATATYPE_BASE::KBE_FLT_MAX, 1, true);
	}

	void BaseApp::Client_onUpdateData_xz_p_optimized(MemoryStream &stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		FVector2D xz = stream.ReadPackXZ();

		int8 p = stream.ReadInt8();

		UpdateVolatileData(eid, xz[0], FLT_MAX, xz[1], KBEDATATYPE_BASE::KBE_FLT_MAX, p, KBEDATATYPE_BASE::KBE_FLT_MAX, 1, true);
	}

	void BaseApp::Client_onUpdateData_xz_r_optimized(MemoryStream &stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		FVector2D xz = stream.ReadPackXZ();

		int8 r = stream.ReadInt8();

		UpdateVolatileData(eid, xz[0], FLT_MAX, xz[1], KBEDATATYPE_BASE::KBE_FLT_MAX, KBEDATATYPE_BASE::KBE_FLT_MAX, r, 1, true);
	}

	void BaseApp::Client_onUpdateData_xyz_optimized(MemoryStream &stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		FVector2D xz = stream.ReadPackXZ();
		float y = stream.ReadPackY();

		UpdateVolatileData(eid, xz[0], y, xz[1], KBEDATATYPE_BASE::KBE_FLT_MAX, KBEDATATYPE_BASE::KBE_FLT_MAX, KBEDATATYPE_BASE::KBE_FLT_MAX, 0, true);
	}

	void BaseApp::Client_onUpdateData_xyz_ypr_optimized(MemoryStream &stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		FVector2D xz = stream.ReadPackXZ();
		float y = stream.ReadPackY();

		int8 yaw = stream.ReadInt8();
		int8 p = stream.ReadInt8();
		int8 r = stream.ReadInt8();

		UpdateVolatileData(eid, xz[0], y, xz[1], yaw, p, r, 0, true);
	}

	void BaseApp::Client_onUpdateData_xyz_yp_optimized(MemoryStream &stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		FVector2D xz = stream.ReadPackXZ();
		float y = stream.ReadPackY();

		int8 yaw = stream.ReadInt8();
		int8 p = stream.ReadInt8();

		UpdateVolatileData(eid, xz[0], y, xz[1], yaw, p, KBEDATATYPE_BASE::KBE_FLT_MAX, 0, true);
	}

	void BaseApp::Client_onUpdateData_xyz_yr_optimized(MemoryStream &stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		FVector2D xz = stream.ReadPackXZ();
		float y = stream.ReadPackY();

		int8 yaw = stream.ReadInt8();
		int8 r = stream.ReadInt8();

		UpdateVolatileData(eid, xz[0], y, xz[1], yaw, KBEDATATYPE_BASE::KBE_FLT_MAX, r, 0, true);
	}

	void BaseApp::Client_onUpdateData_xyz_pr_optimized(MemoryStream &stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		FVector2D xz = stream.ReadPackXZ();
		float y = stream.ReadPackY();

		int8 p = stream.ReadInt8();
		int8 r = stream.ReadInt8();

		UpdateVolatileData(eid, xz[0], y, xz[1], KBEDATATYPE_BASE::KBE_FLT_MAX, p, r, 0, true);
	}

	void BaseApp::Client_onUpdateData_xyz_y_optimized(MemoryStream &stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		FVector2D xz = stream.ReadPackXZ();
		float y = stream.ReadPackY();

		int8 yaw = stream.ReadInt8();
		UpdateVolatileData(eid, xz[0], y, xz[1], yaw, KBEDATATYPE_BASE::KBE_FLT_MAX, KBEDATATYPE_BASE::KBE_FLT_MAX, 0, true);
	}

	void BaseApp::Client_onUpdateData_xyz_p_optimized(MemoryStream &stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		FVector2D xz = stream.ReadPackXZ();
		float y = stream.ReadPackY();

		int8 p = stream.ReadInt8();

		UpdateVolatileData(eid, xz[0], y, xz[1], KBEDATATYPE_BASE::KBE_FLT_MAX, p, KBEDATATYPE_BASE::KBE_FLT_MAX, 0, true);
	}

	void BaseApp::Client_onUpdateData_xyz_r_optimized(MemoryStream &stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		FVector2D xz = stream.ReadPackXZ();
		float y = stream.ReadPackY();

		int8 r = stream.ReadInt8();

		UpdateVolatileData(eid, xz[0], y, xz[1], KBEDATATYPE_BASE::KBE_FLT_MAX, KBEDATATYPE_BASE::KBE_FLT_MAX, r, 0, true);
	}




	void BaseApp::Client_onUpdateData_ypr(MemoryStream& stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		float y = stream.ReadFloat();
		float p = stream.ReadFloat();
		float r = stream.ReadFloat();

		UpdateVolatileData(eid, FLT_MAX, FLT_MAX, FLT_MAX, y, p, r, -1, false);
	}

	void BaseApp::Client_onUpdateData_yp(MemoryStream& stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		float y = stream.ReadFloat();
		float p = stream.ReadFloat();

		UpdateVolatileData(eid, FLT_MAX, FLT_MAX, FLT_MAX, y, p, KBEDATATYPE_BASE::KBE_FLT_MAX, -1, false);
	}

	void BaseApp::Client_onUpdateData_yr(MemoryStream& stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		float y = stream.ReadFloat();
		float r = stream.ReadFloat();

		UpdateVolatileData(eid, FLT_MAX, FLT_MAX, FLT_MAX, y, KBEDATATYPE_BASE::KBE_FLT_MAX, r, -1, false);
	}

	void BaseApp::Client_onUpdateData_pr(MemoryStream& stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		float p = stream.ReadFloat();
		float r = stream.ReadFloat();

		UpdateVolatileData(eid, FLT_MAX, FLT_MAX, FLT_MAX, KBEDATATYPE_BASE::KBE_FLT_MAX, p, r, -1, false);
	}

	void BaseApp::Client_onUpdateData_y(MemoryStream& stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		float y = stream.ReadFloat();

		UpdateVolatileData(eid, FLT_MAX, FLT_MAX, FLT_MAX, y, KBEDATATYPE_BASE::KBE_FLT_MAX, KBEDATATYPE_BASE::KBE_FLT_MAX, -1, false);
	}

	void BaseApp::Client_onUpdateData_p(MemoryStream& stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		float p = stream.ReadFloat();

		UpdateVolatileData(eid, FLT_MAX, FLT_MAX, FLT_MAX, KBEDATATYPE_BASE::KBE_FLT_MAX, p, KBEDATATYPE_BASE::KBE_FLT_MAX, -1, false);
	}

	void BaseApp::Client_onUpdateData_r(MemoryStream& stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		float r = stream.ReadFloat();

		UpdateVolatileData(eid, FLT_MAX, FLT_MAX, FLT_MAX, KBEDATATYPE_BASE::KBE_FLT_MAX, KBEDATATYPE_BASE::KBE_FLT_MAX, r, -1, false);
	}

	void BaseApp::Client_onUpdateData_xz(MemoryStream& stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		float x = stream.ReadFloat();
		float z = stream.ReadFloat();

		UpdateVolatileData(eid, x, FLT_MAX, z, KBEDATATYPE_BASE::KBE_FLT_MAX, KBEDATATYPE_BASE::KBE_FLT_MAX, KBEDATATYPE_BASE::KBE_FLT_MAX, 1, false);
	}

	void BaseApp::Client_onUpdateData_xz_ypr(MemoryStream& stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		float x = stream.ReadFloat();
		float z = stream.ReadFloat();

		float y = stream.ReadFloat();
		float p = stream.ReadFloat();
		float r = stream.ReadFloat();

		UpdateVolatileData(eid, x, FLT_MAX, z, y, p, r, 1, false);
	}

	void BaseApp::Client_onUpdateData_xz_yp(MemoryStream& stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		float x = stream.ReadFloat();
		float z = stream.ReadFloat();

		float y = stream.ReadFloat();
		float p = stream.ReadFloat();

		UpdateVolatileData(eid, x, FLT_MAX, z, y, p, KBEDATATYPE_BASE::KBE_FLT_MAX, 1, false);
	}

	void BaseApp::Client_onUpdateData_xz_yr(MemoryStream& stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		float x = stream.ReadFloat();
		float z = stream.ReadFloat();

		float y = stream.ReadFloat();
		float r = stream.ReadFloat();

		UpdateVolatileData(eid, x, FLT_MAX, z, y, KBEDATATYPE_BASE::KBE_FLT_MAX, r, 1, false);
	}

	void BaseApp::Client_onUpdateData_xz_pr(MemoryStream& stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		float x = stream.ReadFloat();
		float z = stream.ReadFloat();

		float p = stream.ReadFloat();
		float r = stream.ReadFloat();

		UpdateVolatileData(eid, x, FLT_MAX, z, KBEDATATYPE_BASE::KBE_FLT_MAX, p, r, 1, false);
	}

	void BaseApp::Client_onUpdateData_xz_y(MemoryStream& stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		float x = stream.ReadFloat();
		float z = stream.ReadFloat();

		float y = stream.ReadFloat();

		UpdateVolatileData(eid, x, FLT_MAX, z, y, KBEDATATYPE_BASE::KBE_FLT_MAX, KBEDATATYPE_BASE::KBE_FLT_MAX, 1, false);
	}

	void BaseApp::Client_onUpdateData_xz_p(MemoryStream& stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		float x = stream.ReadFloat();
		float z = stream.ReadFloat();

		float p = stream.ReadFloat();

		UpdateVolatileData(eid, x, FLT_MAX, z, KBEDATATYPE_BASE::KBE_FLT_MAX, p, KBEDATATYPE_BASE::KBE_FLT_MAX, 1, false);
	}

	void BaseApp::Client_onUpdateData_xz_r(MemoryStream& stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		float x = stream.ReadFloat();
		float z = stream.ReadFloat();

		float r = stream.ReadFloat();

		UpdateVolatileData(eid, x, FLT_MAX, z, KBEDATATYPE_BASE::KBE_FLT_MAX, KBEDATATYPE_BASE::KBE_FLT_MAX, r, 1, false);
	}

	void BaseApp::Client_onUpdateData_xyz(MemoryStream& stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		float x = stream.ReadFloat();
		float y = stream.ReadFloat();
		float z = stream.ReadFloat();

		UpdateVolatileData(eid, x, y, z, KBEDATATYPE_BASE::KBE_FLT_MAX, KBEDATATYPE_BASE::KBE_FLT_MAX, KBEDATATYPE_BASE::KBE_FLT_MAX, 0, false);
	}

	void BaseApp::Client_onUpdateData_xyz_ypr(MemoryStream& stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		float x = stream.ReadFloat();
		float y = stream.ReadFloat();
		float z = stream.ReadFloat();

		float yaw = stream.ReadFloat();
		float p = stream.ReadFloat();
		float r = stream.ReadFloat();

		UpdateVolatileData(eid, x, y, z, yaw, p, r, 0, false);
	}

	void BaseApp::Client_onUpdateData_xyz_yp(MemoryStream& stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		float x = stream.ReadFloat();
		float y = stream.ReadFloat();
		float z = stream.ReadFloat();

		float yaw = stream.ReadFloat();
		float p = stream.ReadFloat();

		UpdateVolatileData(eid, x, y, z, yaw, p, KBEDATATYPE_BASE::KBE_FLT_MAX, 0, false);
	}

	void BaseApp::Client_onUpdateData_xyz_yr(MemoryStream& stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		float x = stream.ReadFloat();
		float y = stream.ReadFloat();
		float z = stream.ReadFloat();

		float yaw = stream.ReadFloat();
		float r = stream.ReadFloat();

		UpdateVolatileData(eid, x, y, z, yaw, KBEDATATYPE_BASE::KBE_FLT_MAX, r, 0, false);
	}

	void BaseApp::Client_onUpdateData_xyz_pr(MemoryStream& stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		float x = stream.ReadFloat();
		float y = stream.ReadFloat();
		float z = stream.ReadFloat();

		float p = stream.ReadFloat();
		float r = stream.ReadFloat();

		UpdateVolatileData(eid, x, y, z, KBEDATATYPE_BASE::KBE_FLT_MAX, p, r, 0, false);
	}

	void BaseApp::Client_onUpdateData_xyz_y(MemoryStream& stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		float x = stream.ReadFloat();
		float y = stream.ReadFloat();
		float z = stream.ReadFloat();

		float yaw = stream.ReadFloat();

		UpdateVolatileData(eid, x, y, z, yaw, KBEDATATYPE_BASE::KBE_FLT_MAX, KBEDATATYPE_BASE::KBE_FLT_MAX, 0, false);
	}

	void BaseApp::Client_onUpdateData_xyz_p(MemoryStream& stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		float x = stream.ReadFloat();
		float y = stream.ReadFloat();
		float z = stream.ReadFloat();

		float p = stream.ReadFloat();

		UpdateVolatileData(eid, x, y, z, KBEDATATYPE_BASE::KBE_FLT_MAX, p, KBEDATATYPE_BASE::KBE_FLT_MAX, 0, false);
	}

	void BaseApp::Client_onUpdateData_xyz_r(MemoryStream& stream)
	{
		int32 eid = GetAoiEntityIDFromStream(stream);

		float x = stream.ReadFloat();
		float y = stream.ReadFloat();
		float z = stream.ReadFloat();

		float r = stream.ReadFloat();

		UpdateVolatileData(eid, x, y, z, KBEDATATYPE_BASE::KBE_FLT_MAX, KBEDATATYPE_BASE::KBE_FLT_MAX, r, 0, false);
	}

	void BaseApp::UpdateVolatileData(int32 entityID, float x, float y, float z, float yaw, float pitch, float roll, int8 isOnGround, bool isOptimized)
	{
		Entity* entity = FindEntity(entityID);
		if (!entity)
		{
			// ���Ϊ0�ҿͻ�����һ�����ص�½���������������ҷ����entity�ڶ����ڼ�һֱ��������״̬
			// ����Ժ����������, ��Ϊcellapp����һֱ����baseapp����ͬ����Ϣ�� ���ͻ���������ʱδ��
			// ����˳�ʼ�����迪ʼ���յ�ͬ����Ϣ, ��ʱ����ͻ����
			KBE_ERROR(TEXT("BaseApp::UpdateVolatileData: entity(%d) not found!"), entityID);
			return;
		}

		// С��0������
		if (isOnGround >= 0)
		{
			entity->IsOnGround(isOnGround > 0);
		}

		bool changeDirection = false;
		FVector direction = KBEMath::Unreal2KBEngineDirection(entity->localDirection_);

		if (roll != KBEDATATYPE_BASE::KBE_FLT_MAX)
		{
			changeDirection = true;
			if (isOptimized) 
			{
				direction.X = KBEMath::int82angle((int8)roll, false);
			}
			else 
			{
				direction.X = roll;
			}
		}

		if (pitch != KBEDATATYPE_BASE::KBE_FLT_MAX)
		{
			changeDirection = true;
			if (isOptimized)
			{
				direction.Y = KBEMath::int82angle((int8)pitch, false);
			}
			else
			{
				direction.Y = pitch;
			}
		}

		if (yaw != KBEDATATYPE_BASE::KBE_FLT_MAX)
		{
			changeDirection = true;
			if (isOptimized)
			{
				direction.Z = KBEMath::int82angle((int8)yaw, false);
			}
			else
			{
				direction.Z = yaw;
			}
		}

		direction = KBEMath::KBEngine2UnrealDirection(direction);

		// ����и������븸����м���õ����糯��
		if (changeDirection)
		{
			// ���������Ǹ��±��س�����˲����Ƿ��и����󣬶��ȸ��±��س���
			entity->localDirection_ = direction;

			if (entity->ParentID() > 0)
			{
				if (entity->Parent())
					direction = entity->Parent()->DirectionLocalToWorld(direction);
				else
					changeDirection = false;  // ��parentID���Ҳ���parent entity����ֱ�ӷ���
			}
		}

		bool done = false;
		if (changeDirection == true)
		{
			entity->direction_ = direction;
			done = true;
		}
		
		bool positionChanged = x != FLT_MAX || y != FLT_MAX || z != FLT_MAX;
		if (x == FLT_MAX) x = 0.0;
		if (y == FLT_MAX) y = 0.0;
		if (z == FLT_MAX) z = 0.0;

		FVector pos(x, y, z);
		pos = KBEMath::KBEngine2UnrealPosition(pos);

		if (positionChanged)
		{
			if (entity->ParentID() > 0)
			{
				entity->localPosition_ = pos;
				if (entity->Parent())
					// ����и��������븸������м���õ����糯��
					pos = entity->Parent()->PositionLocalToWorld(entity->localPosition_);
				else
					// �Ҳ�����������ֱ�ӷ���
					positionChanged = false;
			}
			else
			{
				// û�и�����������Ϊ���ļ������糯��
				if(isOptimized)
					pos += entityServerPos_;
				entity->localPosition_ = pos;
			}
		}

		if (positionChanged)
		{
			entity->position_ = pos;
			done = true;
		}

		if (done)
		{
			entity->SyncVolatileDataToChildren(!changeDirection);
			entity->OnUpdateVolatileData();
		}
	}

	void BaseApp::Client_onAppActiveTickCB()
	{
		lastTickCBTime_ = FDateTime::UtcNow();
		//KBE_ERROR(TEXT("shufeng -->>>BaseApp::Client_onAppActiveTickCB SendTick callback: Receive appTick ccccccccccccccbbbbbbbbbbbbb!at time:%s:%d"), 
		//	*FDateTime::UtcNow().ToString(), lastTickCBTime_.GetMillisecond());
	}

	void BaseApp::Client_onStreamDataStarted(int16 id, uint32 datasize, const FString& descr)
	{
		if (KBEPersonality::Instance())
			KBEPersonality::Instance()->OnStreamDataStarted(id, datasize, descr);
	}

	void BaseApp::Client_onStreamDataRecv(MemoryStream &stream)
	{
		if (KBEPersonality::Instance())
			KBEPersonality::Instance()->OnStreamDataRecv(stream);
	}

	void BaseApp::Client_onStreamDataCompleted(int16 id)
	{
		if (KBEPersonality::Instance())
			KBEPersonality::Instance()->OnStreamDataCompleted(id);
	}

	void BaseApp::Client_onParentChanged(int32 eid, int32 parentID)
	{
		Entity* ent = FindEntity(eid);
		if (!ent)
		{
			KBE_ERROR(TEXT("BaseApp::Client_onParentChanged(), invalid entity id '%d', parent '%d'"), eid, parentID);
			return;
		}

		if (parentID <= 0)
		{
			ent->SetParent(nullptr);
			return;
		}

		Entity* parentEnt = FindEntity(parentID);
		if (!parentEnt)
			ent->parentID_ = parentID;
		else
			ent->SetParent(parentEnt);
	}

	void BaseApp::Client_acrossServerReady(MemoryStream & stream)
	{
		UINT64 loginKey = stream.ReadUint64();
		FString baseappHost = stream.ReadString();
		UINT16 basePort = stream.ReadUint16();

		KBE_DEBUG(TEXT("LoginApp::Client_acrossServerReady: accountName(%s), addr(%s:%d)!"),
			*account_, *baseappHost, basePort);

		app_->AcrossServerReady(loginKey, baseappHost, basePort);
	}

	void BaseApp::Process()
	{
		if (networkInterface_)
		{
			networkInterface_->Process();
			messageReader_->Process();
		}

		if (networkInterface_ && networkInterface_->Valid())
		{
			UpdatePlayerToServer();
			SendTick();
		}
	}


	void BaseApp::HandleMessage(const FString &name, MemoryStream *stream)
	{
		//KBE_ERROR(TEXT("shufeng--->>>BaseApp::HandleMessage MemoryStream *stream 1111:  message name (%s)! at time :%s: %d"), 
		//	*name, *FDateTime::UtcNow().ToString(), FDateTime::UtcNow().GetMillisecond());
		if (name == "Client_onHelloCB") {
			Client_onHelloCB(*stream);
		}
		else if (name == "Client_onScriptVersionNotMatch") {
			Client_onScriptVersionNotMatch(*stream);
		}
		else if (name == "Client_onVersionNotMatch") {
			Client_onVersionNotMatch(*stream);
		}
		else if (name == "Client_onImportClientMessages") {
			Client_onImportClientMessages(*stream);
		}
		else if (name == "Client_onImportClientEntityDef") {

			Client_onImportClientEntityDef(*stream);
		}
		else if (name == "Client_onLoginBaseappSuccessfully") {

			Client_onLoginBaseappSuccessfully(*stream);
		}
		else if (name == "Client_onReloginBaseappSuccessfully") {

			Client_onReloginBaseappSuccessfully(*stream);
		}
		else if (name == "Client_onUpdatePropertysOptimized") {

			Client_onUpdatePropertysOptimized(*stream);
		}
		else if (name == "Client_onUpdatePropertys") {

			Client_onUpdatePropertys(*stream);
		}
		else if (name == "Client_onRemoteMethodCallOptimized") {

			Client_onRemoteMethodCallOptimized(*stream);
		}
		else if (name == "Client_onRemoteMethodCall") {

			Client_onRemoteMethodCall(*stream);
		}
		else if (name == "Client_onEntityEnterWorld") {

			Client_onEntityEnterWorld(*stream);
		}
		else if (name == "Client_onEntityLeaveWorldOptimized") {

			Client_onEntityLeaveWorldOptimized(*stream);
		}
		else if (name == "Client_onEntityEnterSpace") {

			Client_onEntityEnterSpace(*stream);
		}
		else if (name == "Client_initSpaceData") {

			Client_initSpaceData(*stream);
		}
		else if (name == "Client_onUpdateData") {

			Client_onUpdateData(*stream);
		}
		else if (name == "Client_onSetEntityPosAndDir") {

			Client_onSetEntityPosAndDir(*stream);
		}
		else if (name == "Client_onUpdateData_ypr") {

			Client_onUpdateData_ypr(*stream);
		}
		else if (name == "Client_onUpdateData_yp") {

			Client_onUpdateData_yp(*stream);
		}
		else if (name == "Client_onUpdateData_yr") {

			Client_onUpdateData_yr(*stream);
		}
		else if (name == "Client_onUpdateData_pr") {

			Client_onUpdateData_pr(*stream);
		}
		else if (name == "Client_onUpdateData_y") {

			Client_onUpdateData_y(*stream);
		}
		else if (name == "Client_onUpdateData_p") {

			Client_onUpdateData_p(*stream);
		}
		else if (name == "Client_onUpdateData_r") {

			Client_onUpdateData_r(*stream);
		}
		else if (name == "Client_onUpdateData_xz") {

			Client_onUpdateData_xz(*stream);
		}
		else if (name == "Client_onUpdateData_xz_ypr") {

			Client_onUpdateData_xz_ypr(*stream);
		}
		else if (name == "Client_onUpdateData_xz_yp") {

			Client_onUpdateData_xz_yp(*stream);
		}
		else if (name == "Client_onUpdateData_xz_yr") {

			Client_onUpdateData_xz_yr(*stream);
		}
		else if (name == "Client_onUpdateData_xz_pr") {

			Client_onUpdateData_xz_pr(*stream);
		}
		else if (name == "Client_onUpdateData_xz_y") {

			Client_onUpdateData_xz_y(*stream);
		}
		else if (name == "Client_onUpdateData_xz_p") {

			Client_onUpdateData_xz_p(*stream);
		}
		else if (name == "Client_onUpdateData_xz_r") {

			Client_onUpdateData_xz_r(*stream);
		}
		else if (name == "Client_onUpdateData_xyz") {

			Client_onUpdateData_xyz(*stream);
		}
		else if (name == "Client_onUpdateData_xyz_ypr") {

			Client_onUpdateData_xyz_ypr(*stream);
		}
		else if (name == "Client_onUpdateData_xyz_yp") {

			Client_onUpdateData_xyz_yp(*stream);
		}
		else if (name == "Client_onUpdateData_xyz_yr") {

			Client_onUpdateData_xyz_yr(*stream);
		}
		else if (name == "Client_onUpdateData_xyz_pr") {

			Client_onUpdateData_xyz_pr(*stream);
		}
		else if (name == "Client_onUpdateData_xyz_y") {

			Client_onUpdateData_xyz_y(*stream);
		}
		else if (name == "Client_onUpdateData_xyz_p") {

			Client_onUpdateData_xyz_p(*stream);
		}
		else if (name == "Client_onUpdateData_xyz_r") {

			Client_onUpdateData_xyz_r(*stream);
		}
		else if (name == "Client_onUpdateData_ypr_optimized") {

			Client_onUpdateData_ypr_optimized(*stream);
		}
		else if (name == "Client_onUpdateData_yp_optimized") {

			Client_onUpdateData_yp_optimized(*stream);
		}
		else if (name == "Client_onUpdateData_yr_optimized") {

			Client_onUpdateData_yr_optimized(*stream);
		}
		else if (name == "Client_onUpdateData_pr_optimized") {

			Client_onUpdateData_pr_optimized(*stream);
		}
		else if (name == "Client_onUpdateData_y_optimized") {

			Client_onUpdateData_y_optimized(*stream);
		}
		else if (name == "Client_onUpdateData_p_optimized") {

			Client_onUpdateData_p_optimized(*stream);
		}
		else if (name == "Client_onUpdateData_r_optimized") {

			Client_onUpdateData_r_optimized(*stream);
		}
		else if (name == "Client_onUpdateData_xz_optimized") {

			Client_onUpdateData_xz_optimized(*stream);
		}
		else if (name == "Client_onUpdateData_xz_ypr_optimized") {

			Client_onUpdateData_xz_ypr_optimized(*stream);
		}
		else if (name == "Client_onUpdateData_xz_yp_optimized") {

			Client_onUpdateData_xz_yp_optimized(*stream);
		}
		else if (name == "Client_onUpdateData_xz_yr_optimized") {

			Client_onUpdateData_xz_yr_optimized(*stream);
		}
		else if (name == "Client_onUpdateData_xz_pr_optimized") {

			Client_onUpdateData_xz_pr_optimized(*stream);
		}
		else if (name == "Client_onUpdateData_xz_y_optimized") {

			Client_onUpdateData_xz_y_optimized(*stream);
		}
		else if (name == "Client_onUpdateData_xz_p_optimized") {

			Client_onUpdateData_xz_p_optimized(*stream);
		}
		else if (name == "Client_onUpdateData_xz_r_optimized") {

			Client_onUpdateData_xz_r_optimized(*stream);
		}
		else if (name == "Client_onUpdateData_xyz_optimized") {

			Client_onUpdateData_xyz_optimized(*stream);
		}
		else if (name == "Client_onUpdateData_xyz_ypr_optimized") {

			Client_onUpdateData_xyz_ypr_optimized(*stream);
		}
		else if (name == "Client_onUpdateData_xyz_yp_optimized") {

			Client_onUpdateData_xyz_yp_optimized(*stream);
		}
		else if (name == "Client_onUpdateData_xyz_yr_optimized") {

			Client_onUpdateData_xyz_yr_optimized(*stream);
		}
		else if (name == "Client_onUpdateData_xyz_pr_optimized") {

			Client_onUpdateData_xyz_pr_optimized(*stream);
		}
		else if (name == "Client_onUpdateData_xyz_y_optimized") {

			Client_onUpdateData_xyz_y_optimized(*stream);
		}
		else if (name == "Client_onUpdateData_xyz_p_optimized") {

			Client_onUpdateData_xyz_p_optimized(*stream);
		}
		else if (name == "Client_onUpdateData_xyz_r_optimized") {

			Client_onUpdateData_xyz_r_optimized(*stream);
		}
		else if (name == "Client_onStreamDataRecv") {

			Client_onStreamDataRecv(*stream);
		}
		else if (name == "Client_onStreamDataRecv") {

			Client_onStreamDataRecv(*stream);
		}
		else if (name == "Client_onAppActiveTickCB") {
			Client_onAppActiveTickCB();
		}
		else if (name == "Client_acrossServerReady") {
			Client_acrossServerReady(*stream);
		}
		else
		{
			KBE_ERROR(TEXT("BaseApp::HandleMessage: 1 - unknown message '%s'"), *name);
		}

	}

	void BaseApp::HandleMessage(const FString &name, const TArray<FVariant> &args)
	{
		//KBE_ERROR(TEXT("shufeng--->>>BaseApp::HandleMessage TArray<FVariant> 2222:  message name (%s) at time(%s:%d)!"), 
		//	*name, *FDateTime::UtcNow().ToString(), FDateTime::UtcNow().GetMillisecond());

		if (name == "Client_onCreatedProxies") {
			uint64 rndUUID = args[0].GetValue<uint64>();
			int32 eid = args[1].GetValue<int32>();
			FString entityType = args[2].GetValue<FString>();

			Client_onCreatedProxies(rndUUID, eid, entityType);
		}
		else if (name == "Client_onEntityLeaveWorld") {
			Client_onEntityLeaveWorld(args[0].GetValue<int32>());
		}
		else if (name == "Client_onEntityLeaveSpace") {
			Client_onEntityLeaveSpace(args[0].GetValue<int32>());
		}
		else if (name == "Client_setSpaceData") {

			Client_setSpaceData(args[0].GetValue<uint32>(), args[1].GetValue<FString>(), args[2].GetValue<FString>());
		}
		else if (name == "Client_delSpaceData") {

			Client_delSpaceData(args[0].GetValue<uint32>(), args[1].GetValue<FString>());
		}
		else if (name == "Client_onControlEntity")
		{
			Client_onControlEntity(args[0].GetValue<int32>(), args[1].GetValue<int8>());
		}
		else if (name == "Client_onStreamDataStarted") {
			Client_onStreamDataStarted(args[0].GetValue<int16>(), args[1].GetValue<uint32>(), args[2].GetValue<FString>());
		}
		else if (name == "Client_onStreamDataCompleted") {
			Client_onStreamDataCompleted(args[0].GetValue<int16>());
		}
		else if (name == "Client_onKicked") {
			Client_onKicked(args[0].GetValue<uint16>());
		}
		else if (name == "Client_onUpdateBasePos") {

			Client_onUpdateBasePos(args[0].GetValue<float>(), args[1].GetValue<float>(), args[2].GetValue<float>());
		}
		else if (name == "Client_onUpdateBasePosXZ") {

			Client_onUpdateBasePosXZ(args[0].GetValue<float>(), args[1].GetValue<float>());
		}
		else if (name == "Client_onLoginBaseappFailed") {
			Client_onLoginBaseappFailed(args[0].GetValue<uint16>());
		}
		else if (name == "Client_onReloginBaseappFailed") {
			Client_onReloginBaseappFailed(args[0].GetValue<uint16>());
		}
		else if (name == "Client_onEntityDestroyed") {
			Client_onEntityDestroyed(args[0].GetValue<int32>());
		}
		else if (name == "Client_onParentChanged") {
			Client_onParentChanged(args[0].GetValue<int32>(), args[1].GetValue<int32>());
		}
		else
		{
			KBE_ERROR(TEXT("BaseApp::HandleMessage: 2 - unknown message '%s'"), *name);
		}
	}
}
