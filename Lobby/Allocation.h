#ifndef _ALLOC_H_
#define _ALLOC_H_

#include "stdafx.h"
#include <deque>
#include <map>
#include <boost/thread.hpp>
#include <boost/aligned_storage.hpp>
#include <boost/make_shared.hpp>

class CAlloc : private boost::noncopyable
{
private:
	boost::aligned_storage<1024*4> m_Storage;
	void *p;

public:
	CAlloc() : p(NULL)
	{
	}

	virtual ~CAlloc()
	{
	}

	void *allocate(size_t s)
	{
		return (p = ::operator new(s));
	}

	void deallocate()
	{
		operator delete(p);
	}
};

typedef std::map<std::pair<void *, int>, void *> TAllocationMap;
typedef std::map<void *, int> _TAllocationMap;

class CAllocManager
{
private:
	_TAllocationMap m_Allocations;
	TAllocationMap m_AllocationList;

	boost::mutex m_Mutex;
	boost::condition_variable m_Condition;

public:
	CAllocManager()
	{
		boost::thread t(boost::bind(&CAllocManager::Loop, this));
	}

	void Loop()
	{
		while(true)
		{
			for(TAllocationMap::iterator it = m_AllocationList.begin(); it != m_AllocationList.end(); ++it)
			{
				if(it->second == NULL)
				{
					it->second = malloc(it->first.second);
					memset(it->second, 0, it->first.second);
					m_Allocations.insert(_TAllocationMap::value_type(it->second, it->first.second));

					m_Condition.notify_all();
				}
			}
			for(_TAllocationMap::iterator it = m_Allocations.begin(); it != m_Allocations.end(); ++it)
			{
				if(it->second == 0)
				{
					delete [] it->first;
					m_Allocations.erase(it);
				}
			}
		}
	}

	void Pop(void *call)
	{
		boost::unique_lock<boost::mutex> Lock(m_Mutex);

		int size = 0;
		for(TAllocationMap::iterator it = m_AllocationList.begin(); it != m_AllocationList.end(); ++it)
		{
			if(it->first.first == call)
			{
				size = it->first.second;
				m_AllocationList.erase(it);

				m_Allocations.insert(_TAllocationMap::value_type(call, size));
				break;
			}
		}
	}
	
	void* Push(void *call, int size)
	{
		try
		{
			boost::unique_lock<boost::mutex> Lock(m_Mutex);

			void **v = new void*;
			memcpy(v, &call, sizeof(int));

			//::unique_lock<boost::mutex> tryLock(m_Mutex);

			auto it = m_AllocationList.insert(TAllocationMap::value_type(std::make_pair<void*, int>(call, size), (void*)NULL));

			while(it.first->second == NULL)
			{
				m_Condition.wait(Lock);
			}

			return it.first->second;
		} catch(std::exception &e)
		{
			(void)e.what();
		}

		return NULL;
	}

	template<typename T>
	T *NEW(void *Caller)
	{
		//boost::shared_ptr<boost::unique_lock<boost::mutex>> tryLock = boost::make_shared<boost::unique_lock<boost::mutex>>(boost::ref(boost::unique_lock<boost::mutex>(m_Mutex)));
		void *f = Push(Caller, sizeof(T));
		Pop(Caller);
		return (T*)f;
	}

	template<typename T>
	void _DELETE(T *t)
	{
		try
		{
			boost::unique_lock<boost::mutex> tryLock(m_Mutex);

			_TAllocationMap::iterator it = std::find(m_Allocations.begin(), m_Allocations.end(), (void*)t);

			if(it != m_Allocations.end())
			{
				it->first = NULL;
			}
		} catch(std::exception &e)
		{
			(void)e.what();
		}
	}
};

CAllocManager AllocationManager;

#endif