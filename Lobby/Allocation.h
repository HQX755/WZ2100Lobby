#include "stdafx.h"
#include <deque>
#include <map>
#include <boost/thread.hpp>
#include <boost/aligned_storage.hpp>

class CAlloc : private boost::noncopyable
{
private:
	boost::aligned_storage<1024*4> m_Storage;

public:
	CAlloc()
	{
	}

	virtual ~CAlloc()
	{
	}

	void *allocate(size_t s)
	{
		return (m_Storage.address = ::operator new(s));
	}

	void deallocate()
	{
		operator delete(m_Storage.address);
	}
};

typedef std::map<void *, void *> TAllocationMap;
typedef std::map<void *, CAlloc*> _TAllocationMap;

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
	}

	void Loop()
	{
		while(true)
		{
			for(std::map<void *,void *>::iterator it = m_AllocationList.begin(); it != m_AllocationList.end(); ++it)
			{
				it->second = (new CAlloc())->allocate(2048);
				while(it->first != NULL)
				{
					m_Condition.notify_all();
				}
			}
			for(std::map<void*, CAlloc*>::iterator it = m_Allocations.begin(); it != m_Allocations.end(); ++it)
			{
				if(it->first == NULL)
				{
					it->second->deallocate();
					delete it->second;
					m_Allocations.erase(it);
				}
			}
		}
	}

	void* Push(void *Call)
	{
		try
		{
			boost::unique_lock<boost::mutex> tryLock(m_Mutex);

			auto it = m_AllocationList.insert(std::make_pair(Call, NULL));
			m_Condition.wait(tryLock);

			return (void*)it.second;
		} catch(std::exception &e)
		{
			(void)e.what();
		}

		return NULL;
	}

	template<typename T>
	T *NEW(void *Caller)
	{
		return (T*)Push(Caller);
	}

	template<typename T>
	void DELETE(T *t)
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
