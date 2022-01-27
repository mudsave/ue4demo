#pragma once
#include <functional>

#include "Core.h"
#include "KBEDefine.h"

namespace KBEngine
{
	class KBENGINE_API KBEEvent
	{
	public:
		template<typename T>
		struct ClassMethon
		{
			typedef void(T::*FuncType)(const FVariantArray &);
		};

		typedef void(*EventFuncPtr)(const FVariantArray &);
		typedef std::function<void(const FVariantArray &)> EventFunc;

		typedef struct
		{
			void* objAddr = nullptr;
			void* funcAddr = nullptr;
			EventFunc func;
		}EventPair;

		typedef TArray<EventPair*> EventFuncArray;

		typedef struct
		{
			FString name;
			FVariantArray args;
		}EventObj;

	public:
		KBEEvent();
		~KBEEvent();

		void Clear();

		void Pause()   { isPauseOut_ = true; }
		void Resume()  { isPauseOut_ = false; }
		bool IsPause() { return isPauseOut_; }

		template <class T>
		bool Register(const FString& eventName, T* obj, typename ClassMethon<T>::FuncType func);
		bool Register(const FString& eventName, EventFuncPtr func);

		template <class T>
		bool Deregister(const FString& eventName, const T* obj, typename ClassMethon<T>::FuncType func);
		bool Deregister(const FString& eventName, EventFuncPtr func);

		/*
		ע������obj�йص�ע�ắ����
		no efficiency
		*/
		template <class T>
		bool Deregister(const T *obj);

		void Fire(const FString& eventName, const FVariantArray &args);
		void AsyncFire(const FString& eventName, const FVariantArray &args);
		void ProcessAsyncEvents();

	public:
		static KBEEvent *Instance()
		{
			if (!s_event_.Get())
				s_event_ = TSharedPtr<KBEEvent>(new KBEEvent());
			return s_event_.Get(); 
		}

	private:
		EventFuncArray* GetEvent(const FString& eventName);
		// �̰߳�ȫ���¼�����
		void CopyEvent(EventFuncArray& out, const FString& eventName);

		// �̰߳�ȫ�Ĳ����¼���������
		void AddEvent(const FString& eventName, EventPair* eventPair);

		bool HasRegister(const FString& eventName);

		void MonitorEnter(FCriticalSection& cs) { cs.Lock(); }
		void MonitorExit(FCriticalSection& cs) { cs.Unlock(); }

	private:
		static TSharedPtr<KBEEvent> s_event_;

	private:
		/* penghuawei: ��ܿ�����UE4��һ��bug
		   �����ǣ�TMap<FString, EventFuncArray> events_
		   ����������ʱ��ʹ��KBEEvent::Instance()->Register("f1", obj, func)ע��һ��f1�¼�ʱ��һ��������
		   ���ǣ��������ٴ�ע��һ��f1�¼��Ĵ�����ʱ��
		   ��������д��빹����FVariant()ʵ������events_["f1"][0].func�ĵ�ַ�������Ч���ƺ����ͷ��ˣ�
		   ���ԣ���Fire("f1")�¼�ʱ����������ڴ������Ч�����⣬����crash��
		   ��ˣ������Ϊʹ������ָ�룬����������ʵ��
		*/
		TMap<FString, EventFuncArray *> events_;
		TArray<EventObj> firedEvents_;

		FCriticalSection cs_events_;
		FCriticalSection cs_firedEvents_;

		bool isPauseOut_ = false;

	};





	template <class T>
	bool KBEEvent::Deregister(const FString& eventName, const T* obj, typename ClassMethon<T>::FuncType func)
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

		union
		{
			void(T::*f)(const FVariantArray &);
			void* t;
		}ut;

		ut.f = func;

		for (int i = 0; i < lst->Num(); i++)
		{
			auto& pair = (*lst)[i];
			if ((void *)obj == pair->objAddr && pair->funcAddr == ut.t)
			{
				KBE_DEBUG(TEXT("KBEEvent::Deregister: 2 - event(%s:%p:%p)!"), *eventName, obj, ut.t);
				delete pair;
				lst->RemoveAt(i);
				MonitorExit(cs_events_);
				return true;
			}
		}

		MonitorExit(cs_events_);
		return false;
	}

	template <class T>
	bool KBEEvent::Deregister(const T *obj)
	{
		int count = 0;
		MonitorEnter(cs_events_);

		for (auto it : events_)
		{
			EventFuncArray* lst = it.Value;
			// �Ӻ���ǰ�������Ա�����;ɾ��������
			for (int i = lst->Num() - 1; i >= 0; i--)
			{
				const auto& o = (*lst)[i];
				if ((void *)obj == o->objAddr)
				{
					KBE_DEBUG(TEXT("KBEEvent::Deregister: 1 - event(%s:%p)!"), *it.Key, o->objAddr);
					delete o;
					lst->RemoveAt(i);
					count++;
				}
			}

		}

		MonitorExit(cs_events_);
		return count > 0;
	}

	template <class T>
	bool KBEEvent::Register(const FString& eventName, T* obj, typename ClassMethon<T>::FuncType func)
	{
		union
		{
			void(T::*f)(const FVariantArray &);
			void* t;
		}ut;

		ut.f = func;

		EventPair* pair = new EventPair();
		pair->objAddr = (void *)obj;
		pair->funcAddr = ut.t;
		pair->func = std::bind(func, obj, std::placeholders::_1);

		AddEvent(eventName, pair);
		return true;
	}

}
