#include"./Resource/ThreadPool.h"
#include<stdlib.h>
#include<conio.h>

int main() 
{
	//we use heap to maintain our pool,so that we can see the threads return in console
	CThreadPool* pool =new CThreadPool(8,16);

	//or use heap to storage our jobs,
	//but we should maintain their lifecycle by ourselves
	//if we have that demand,use heap
	CJob jobs[11];
	BindingJobs BindingAndSubmitJobs= BindingJobs(pool);

	for (UINT32 i = 0; i < 11; ++i)
	{
		jobs[i].m_Index = i + 1;
	}

	//map
	jobs[1].m_Precursor.push_back(&jobs[7].m_signal);
	jobs[1].m_Precursor.push_back(&jobs[10].m_signal);

	jobs[2].m_Precursor.push_back(&jobs[7].m_signal);

	jobs[0].m_Precursor.push_back(&jobs[2].m_signal);

	jobs[5].m_Precursor.push_back(&jobs[2].m_signal);

	jobs[3].m_Precursor.push_back(&jobs[1].m_signal);

	jobs[6].m_Precursor.push_back(&jobs[5].m_signal);

	jobs[4].m_Precursor.push_back(&jobs[0].m_signal);
	jobs[4].m_Precursor.push_back(&jobs[3].m_signal);

	jobs[8].m_Precursor.push_back(&jobs[4].m_signal);

	jobs[9].m_Precursor.push_back(&jobs[4].m_signal);


	BindingAndSubmitJobs(jobs,11);
	BindingAndSubmitJobs.WaitAllJobs();

	//close thread pool and release all threads
	delete pool;
	cout << "main thread returned\n";
	_getch();
	return 1;
}
