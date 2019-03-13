#pragma once
#include <queue>    // queue
#include "Job.hpp"  // Job_Base

class JobQueue
{
public:
	JobQueue() = default;
	~JobQueue() = default;


	///<summary>
	///	Adds the given <paramref name="job"/> to the queue.
	///</summary>
	///<param name="Job">A job that is ready to execute.</param>
	///<remarks>
	/// Adding a job that is not ready to execute to the queue could
	/// result in it being executed prematurely, resulting in an exception.
	///</remarks>
	void Add_Job(Job_Base* job)
	{
		m_jobs.push(job);
	} // end method Add_Job


	///<summary>
	///	Returns the job that is next-in-line to the caller and removes it from the queue.
	///</summary>
	///<returns>A pointer to a Job object, or nullptr if no jobs are available.</returns>
	Job_Base* Get_Work(void)
	{
		if (m_jobs.empty())
		{
			return nullptr;
		} // end if
		else
		{
			auto job = m_jobs.front();
			m_jobs.pop();
			return job;
		} // end else
	} // end method Get_Work


	///<summary>
	///	Accessor for queue length.
	///</summary>
	///<returns>The number of jobs currently awaiting execution.</returns>
	std::size_t Size(void)
	{
		return m_jobs.size();
	} // end method Size


	///<summary>
	///	Accessor to check if the queue is empty.
	///</summary>
	///<returns>True iff at least one job is waiting for execution.</returns>
	bool Empty(void)
	{
		return m_jobs.empty();
	} // end method Empty


	///<summary>
	///	Clears the queue by destroying all stored jobs.
	///</summary>
	void Clear(void)
	{
		while (!m_jobs.empty())
		{
			delete m_jobs.front();
			m_jobs.pop();
		} // end while
	} // end method Clear


private:
	std::queue<Job_Base*> m_jobs;
}; // end class JobQueue