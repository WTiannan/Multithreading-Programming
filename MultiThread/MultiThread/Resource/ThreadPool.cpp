#include"ThreadPool.h"

CThreadPool::CThreadPool(UINT32 minThreads, UINT32 maxThreads) :
	m_MinThreads(minThreads),
	m_MaxThreads(maxThreads)
{
	//Create Thread if needed;
	if (m_MinThreads)
	{
		for (UINT32 i = 0; i < m_MinThreads; ++i)
		{
			CreateWorkerThread();
		}
	}
}
CThreadPool::~CThreadPool()
{
	//release all threads and exit
	m_bExiting = true;
	
	m_ThreadArrayMtx.Wait(-1);

	//awake all IdleThreads
	for (auto& i : m_IdleThreads)
	{
		i->m_AwakeSignal.Trigger();
	}

	//wait for all worker threads return
	for (auto i : m_WorkerThreads)
	{
		i->Wait(-1);
		delete i;
	}
	m_WorkerThreads.clear();
	m_IdleThreads.clear();
	m_ThreadArrayMtx.Unlock();
}

int CThreadPool::SubmitJob(BindingJobs jobbinding)
{
	for (auto job:jobbinding.jobs) 
	{
		//push the job into queue
		//Mutex to visit thread pool
		m_JobQueueMtx.Wait(-1);
		m_JobQueue.push_back(job);
		m_JobQueueMtx.Unlock();

		// Check if we need to create a new thread.
		m_ThreadArrayMtx.Wait(-1);
		if (m_IdleThreads.empty() && m_WorkerThreads.size() < m_MaxThreads)
		{
			CreateWorkerThread();
		}

		// Awake one of the idle threads to process this job.
		if (!m_IdleThreads.empty())
		{
			CThread *wk = m_IdleThreads.back();
			m_IdleThreads.pop_back();
			wk->m_AwakeSignal.Trigger();
		}
		m_ThreadArrayMtx.Unlock();
	}
	return 1;
}

void CThreadPool::CreateWorkerThread() 
{
	CThread *th = new CThread(this);

	m_ThreadArrayMtx.Wait(-1);
	m_WorkerThreads.push_back(th);
	m_IdleThreads.push_back(th);
	m_ThreadArrayMtx.Unlock();
}

DWORD WINAPI WinThreadEntry(LPVOID cookie)
{
	CThread* w = (CThread *)cookie;
	w->Invoke();
	return 0;
}

CThread::~CThread() 
{
	Wait(-1);
	CloseHandle(m_Handle);
}

int CThread::Invoke() 
{
	while (true)
	{
		// Wait this thread to be awaken by pool.
		//block
		m_AwakeSignal.Wait(-1);

		// Repeatedly peek jobs from queue until the queue is empty.
		while (true)
		{
			CJob* peekedJob = nullptr;
			m_ThreadPool->m_JobQueueMtx.Wait(-1);
			// Search the queue and find one ready job.
			for (auto iter = m_ThreadPool->m_JobQueue.begin(); iter != m_ThreadPool->m_JobQueue.end(); ++iter)
			{

				//stack
				HANDLE* h = (HANDLE*)alloca(sizeof(HANDLE) * (*iter)->m_Precursor.size());
				for (UINT32 i = 0; i < (*iter)->m_Precursor.size(); ++i) 
				{
					h[i] = (*iter)->m_Precursor[i]->m_Handle;
				}

				//no precursor job or all precursor jobs executed.
				if ((*iter)->m_Precursor.empty() || 
					(::WaitForMultipleObjects((*iter)->m_Precursor.size(), h, true, 0) 
						== WAIT_OBJECT_0))
				{
					// Peek this job.
					peekedJob = *iter;
					m_ThreadPool->m_JobQueue.erase(iter);  
					break;
				}
			}
			m_ThreadPool->m_JobQueueMtx.Unlock();

			// Execute job if has.
			if (peekedJob) 
			{
				(*peekedJob)();
				peekedJob->m_signal.Trigger();
				peekedJob = nullptr;
			}
			else
			{
				// If we don't have jobs now, check for it a few time latter.
				// for other threads which need a lot of time to finish the jobs
				// scheduling between threads
				::SwitchToThread();
			}
			// Check if this thread should go to sleep.
			if (m_ThreadPool->m_JobQueue.empty())
			{
				break;
			}
		}

		if (m_ThreadPool->m_bExiting)
		{
			break;
		}

		// Go to sleep.
		m_ThreadPool->m_ThreadArrayMtx.Wait(-1);
		m_ThreadPool->m_IdleThreads.push_back(this);
		m_ThreadPool->m_ThreadArrayMtx.Unlock();
	}
	cout<< "Worker Thread Returned.\n";
	return 1;
}

void BindingJobs::operator()(CJob *j,UINT32 size)
{
	for (UINT32 i = 0; i < size; ++i)
	{
		jobs.push_back(j+i);
	}
	pool->SubmitJob(*this);
}
