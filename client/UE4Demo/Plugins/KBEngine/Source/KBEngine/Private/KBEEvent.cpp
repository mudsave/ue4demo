#include "KBEEvent.h"
#include "KBEnginePrivatePCH.h"

namespace KBEngine
{
	TSharedPtr<KBEEvent> KBEEvent::s_event_ = nullptr;

	KBEEvent::KBEEvent()
	{
	}

	KBEEvent::~KBEEvent()
	{
		KBE_DEBUG(TEXT("KBEEvent::~KBEEvent()"));
		Clear();
	}

	void KBEEvent::Clear()
	{
		for (auto it : events_) 
		{
			EventFuncArray lst = *(it.Value);
			for (int i = 0; i < lst.Num(); i++)
			{
				delete lst[i];
			}

			delete it.Value;
		}

		events_.Empty();
		firedEvents_.Empty();

		isPauseOut_ = false;
	}

	KBEEvent::EventFuncArray* KBEEvent::GetEvent(const FString& eventName)
	{
		MonitorEnter(cs_events_);
		auto ent = events_.Find(eventName);
		MonitorExit(cs_events_);

		return ent ? *ent : nullptr;
	}

	void KBEEvent::CopyEvent(EventFuncArray& out, const FString& eventName)
	{
		MonitorEnter(cs_events_);
		auto ent = events_.Find(eventName);
		if (ent)
			out = **ent;
		MonitorExit(cs_events_);
	}

	void KBEEvent::AddEvent(const FString& eventName, EventPair* eventPair)
	{
		MonitorEnter(cs_events_);
		EventFuncArray** p = events_.Find(eventName);
		EventFuncArray* lst = p ? *p : nullptr;
		if (lst)
		{
			lst->Add(eventPair);
			MonitorExit(cs_events_);
			return;
		}

		EventFuncArray* funcArray = new EventFuncArray();
		funcArray->Add(eventPair);
		events_.Add(eventName, funcArray);
		MonitorExit(cs_events_);
	}

	bool KBEEvent::HasRegister(const FString& eventName)
	{
		return nullptr != GetEvent(eventName);
	}

	void KBEEvent::Fire(const FString& eventName, const FVariantArray &args)
	{
		EventFuncArray lst;
		// ����һ�ݺ����б�������Ա���ص���������Ϊע������������б�һ��
		// ����Ҫ����Ϊ�˱����ڶ��߳����߳����ظ���������
		CopyEvent(lst, eventName);
		if (lst.Num() == 0)
		{
			KBE_WARNING(TEXT("KBEEvent::AsyncFire: event(%s) not found!"), *eventName);
			return;
		}

		for (int j = 0; j < lst.Num(); j++)
		{
			auto pair = lst[j];
			if (pair->func)
				pair->func(args);
		}
	}

	void KBEEvent::AsyncFire(const FString& eventName, const FVariantArray &args)
	{
		EventObj eobj;
		eobj.name = eventName;
		eobj.args = args;

		MonitorEnter(cs_firedEvents_);
		firedEvents_.Add(eobj);
		MonitorExit(cs_firedEvents_);
	}

	void KBEEvent::ProcessAsyncEvents()
	{
		MonitorEnter(cs_firedEvents_);
		TArray<EventObj> doingEvents = firedEvents_;
		firedEvents_.Reset();
		MonitorExit(cs_firedEvents_);

		if (isPauseOut_)
			return;

		EventFuncArray lst;
		for (int i = 0; i < doingEvents.Num(); i++)
		{
			EventObj eobj = doingEvents[i];

			lst.Reset();
			// ����һ�ݺ����б�������Ա���ص���������Ϊע������������б�һ��
			// ����Ҫ����Ϊ�˱����ڶ��߳����߳����ظ���������
			CopyEvent(lst, eobj.name);
			if (lst.Num() == 0)
			{
				KBE_WARNING(TEXT("KBEEvent::AsyncFire: event(%s) not found!"), *eobj.name);
				continue;
			}

			for (int j = 0; j < lst.Num(); j++)
			{
				auto pair = lst[j];
				if (pair->func)
					pair->func(eobj.args);
			}
		}
	}

	bool KBEEvent::Register(const FString& eventName, EventFuncPtr func)
	{
		EventPair* pair = new EventPair();
		pair->objAddr = nullptr;
		pair->funcAddr = (void *)func;
		pair->func = EventFunc(func);

		AddEvent(eventName, pair);
		return true;
	}

	bool KBEEvent::Deregister(const FString& eventName, EventFuncPtr func)
	{
		MonitorEnter(cs_events_);

		// ������ GetEvent()���Ա����ظ���
		EventFuncArray** p = events_.Find(eventName);
		EventFuncArray* lst = p ? *p : nullptr;
		if (!lst)
		{
			MonitorExit(cs_events_);
			return false;
		}

		for (int i = 0; i < lst->Num(); i++)
		{
			auto& pair = (*lst)[i];
			if (pair->objAddr == nullptr && pair->funcAddr == (void *)func)
			{
				KBE_DEBUG(TEXT("KBEEvent::Deregister: 3 - event(%s:%p:)!"), *eventName, func);
				delete pair;
				lst->RemoveAt(i);
				MonitorExit(cs_events_);
				return true;
			}
		}

		MonitorExit(cs_events_);
		return false;
	}

}
