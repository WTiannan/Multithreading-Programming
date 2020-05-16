#pragma once
#include<windows.h>
#include <iostream>
#include <vector>
#include<deque>
#include <mutex>

using namespace std;

class CSignal
{
public:
	HANDLE m_Handle;
	CSignal(bool manualReset)
	{
		m_Handle = ::CreateEventW(NULL, manualReset, FALSE, NULL);
	}
	~CSignal() 
	{
		::CloseHandle(m_Handle);
	}

	int Wait(int timeMilliseconds)
	{
		if (::WaitForSingleObject(m_Handle, timeMilliseconds) == WAIT_OBJECT_0)
		{
			return 1;
		}
		return 0;
	}

	void Trigger() 
	{
		::SetEvent(m_Handle);
	}

	void Reset() 
	{
		::ResetEvent(m_Handle);
	}

};


class CMutex
{
public:
	HANDLE m_Handle;
	CMutex() 
	{
		m_Handle = ::CreateMutexW(NULL, FALSE, NULL);
	};
	~CMutex() 
	{
		::CloseHandle(m_Handle);
	};

	int Wait(int timeMilliseconds) 
	{
		if (::WaitForSingleObject(m_Handle, timeMilliseconds) == WAIT_OBJECT_0)
		{
			return 1;
		}
		return 0;
	}
	void Unlock() 
	{
		::ReleaseMutex(m_Handle);
	}
	
};
class CThreadPool;

class CJob 
{
public:
	UINT32 m_Index;//data
	vector<CSignal*> m_Precursor;
	CSignal m_signal;

	CJob() :
		m_signal(true) {}

	~CJob() 
	{
		m_Precursor.clear();
	}
	int operator()() 
	{
		printf("Task %d Executed.\n", m_Index);
		return 1;
	}
};

class BindingJobs
{
public:
	vector<CJob*> jobs;
	CThreadPool* pool;
	BindingJobs(CThreadPool *p):pool(p){}
	void operator()(CJob* j,UINT32 size);
	//void PushJob(CJob* j) 
	//{
	//	jobs.push_back(j);
	//}
	int WaitAllJobs()
	{
		HANDLE *h = (HANDLE*)alloca(sizeof(HANDLE) * jobs.size());
		for (UINT32 i=0;i<jobs.size();i++) 
		{
			h[i] = (*jobs[i]).m_signal.m_Handle;
		}
		if (::WaitForMultipleObjects(jobs.size(),h,true,-1) == WAIT_OBJECT_0)
		{
			return 1;
		}
		return 0;
	}
	
};

DWORD WINAPI WinThreadEntry(LPVOID cookie);

class CThread
{
public:
	CThreadPool *m_ThreadPool;
	CSignal m_AwakeSignal;
	HANDLE m_Handle;
	void* callback = this;

	CThread(CThreadPool* threadpool) :
		m_ThreadPool(threadpool),
		m_AwakeSignal(false) 
	{
		m_Handle = ::CreateThread(NULL, 0, &WinThreadEntry, callback, 0, NULL);
	}
	~CThread();

	int Invoke();
	int Wait(int timeMilliseconds)
	{
		if (::WaitForSingleObject(m_Handle, timeMilliseconds) == WAIT_OBJECT_0)
		{
			return 1;
		}
		return 0;
	}
};

class CThreadPool
{
public:
	UINT32 m_MinThreads;
	UINT32 m_MaxThreads;
	
	//worker thread array
	vector<CThread*> m_WorkerThreads;
	vector<CThread*> m_IdleThreads;

	//job queue
	deque<CJob*> m_JobQueue;

	//Mutex to visit WorkerThread Array
	CMutex m_ThreadArrayMtx;
	//Mutex to visit ThreadPool
	CMutex m_JobQueueMtx;

	bool m_bExiting;

	CThreadPool(UINT32 minThreads, UINT32 maxThreads);
	~CThreadPool();

	void CreateWorkerThread();
	int SubmitJob(BindingJobs jobbinding);
};


