#pragma once
#include <type_traits> // size_t
#include <thread>      // thread
#include <vector>      // vector
#include <mutex>       // mutex, lock_guard
#include "Job.hpp"
#include "JobQueue.hpp"

class ThreadPool
{
	using Guard_t = std::lock_guard<std::mutex>;
public:
	enum THREAD_SIGNALS
	{
		STARTING,
		WORKING,
		IDLE,
		SIGTERM,
		TERMINATING
	}; // end enum THREAD_SIGNALS

	// Disallow any kind of copy/move operation on thread pools
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool(ThreadPool&&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
	ThreadPool& operator=(ThreadPool&&) = delete;


	///<summary>
	/// Initializes the thread pool to support <paramref name="_nthreads"/> threads.
	/// and sets the yield duration to <paramref name="_sleep_duration"/>.
	///</summary>
	///<param name="_nthreads">The number of threads this pool should support.</param>
	///<param name="_sleep_duration">The amount of time to yield when threads are idle.</param>
	///<remarks>
	/// The threads will not be started the moment the pool is initialized, 
	/// to start the threads, Start_All_Threads or Start_N_Threads must be invoked.
	///</remarks>
	ThreadPool(std::size_t _nthreads = 0, std::chrono::microseconds _sleep_duration = std::chrono::duration<int, std::micro>(100))
	{
		// threads must be started explicitly
		m_nrunning = 0;
		m_nthreads = _nthreads;
		m_sleep_duration = _sleep_duration;
		m_threads.reserve(m_nthreads);
	} // end Constructor(2)
	

	///<summary>
	/// Alerts all threads to terminate, clears the job queue, and then waits for all threads to terminate.
	///</summary>
	///<remarks>
	/// Threads will be allowed to finish before being terminated, however, if jobs are still in the job
	/// queue, they will not be executed but instead discarded.
	///</remarks>
	~ThreadPool(void)
	{
		// signal all threads to terminate
		m_signals_mtx.lock();
		std::for_each(m_signals.begin(), m_signals.end(),
			[&](auto& sigterm) 
			{
				sigterm = THREAD_SIGNALS::SIGTERM; 
			} // end lambda
		); // end foreach
		m_signals_mtx.unlock();

		Empty_Job_Queue();
		
		// wait for all threads to terminate
		for (auto& t : m_threads)
		{
			if (t.joinable())
			{
				t.join();
			} // end if
		} // end for t
	} // end Destructor


	///<summary>
	/// Starts all threads. All threads will begin executing jobs.
	///</summary>
	///<returns>
	/// True on success, false if an issue occurs. 
	///</returns>
	///<remarks>
	/// The threads will begin executing jobs as they become available.
	///</remarks>
	bool Start_All_Threads(void)
	{
		return Start_N_Threads(m_nthreads);
	} // end method 


	///<summary>
	/// Starts <paramref name="_n"/> threads. These threads will begin executing jobs.
	///</summary>
	///<param name="_n">The number of threads to start.</param>
	///<returns>
	/// True on success, false if an issue occurs. 
	///</returns>
	///<remarks>
	/// The value of <paramref name="_n"/> must be at least as large as 
	/// the number of running threads in order for this call to succeed.
	/// By providing an <paramref name="_n"/> value larger than the capacity
	/// of the thread pool, the pool can be grown in size.
	///</remarks>
	bool Start_N_Threads(std::size_t _n)
	{
		if (_n < m_nrunning)
		{
			return false;
		} // end if
		else if (_n > m_nthreads)
		{
			m_nthreads = _n;
		} // end elif

		if (m_nrunning != m_nthreads)
		{
			m_threads.reserve(_n);

			m_jobs_mtx.lock();
			m_signals_mtx.lock();
			for (auto i = m_nrunning; i < _n; i++)
			{
				m_signals.push_back(THREAD_SIGNALS::STARTING);
				m_threads.push_back(std::thread(Idle, this, i));
			} // end for i

			m_nrunning = m_threads.size();

			m_jobs_mtx.unlock();
			m_signals_mtx.unlock();
		} // end if

		return true;
	} // end method Start_N_Threads


	///<summary>
	/// Adds the given job <paramref name="_job"/> to the end of the execution queue.
	///</summary>
	///<param name="_job">A ready-to-execute job that should be executed.</param>
	void Add_Job(Job_Base* _job)
	{
		Guard_t guard(m_jobs_mtx);
		m_jobs.Add_Job(_job);
	} // end method Add_Job


	///<summary>
	/// Terminates all threads currently running in the thread pool. If <paramref name="wait_to_finish"/> 
	/// is set, the pool will sychronize before terminating the running threads.
	///</summary>
	///<param name="wait_to_finish">Whether or not to block and synchronize before terminating.</param>
	///<remarks>
	/// The job queue will not be cleared by this function, an explicit call to Empty_Job_Queue is required.
	///</remarks>
	void Kill_All(bool wait_to_finish = false)
	{
		if (wait_to_finish)
		{
			Synchronize();
		} // end if

		// signal all threads to terminate
		m_signals_mtx.lock();
		std::for_each(m_signals.begin(), m_signals.end(),
			[&](auto& sigterm)
			{
				sigterm = THREAD_SIGNALS::SIGTERM;
			} // end lambda
		); // end foreach
		m_signals_mtx.unlock();

		// wait for all threads to terminate
		for (auto& t : m_threads)
		{
			if (t.joinable())
			{
				t.join();
			} // end if
		} // end for t

		m_threads.clear();

		m_nrunning = 0;
	} // end method Stop


	///<summary>
	/// Removes all pending jobs from the queue and destroys them.
	///</summary>
	void Empty_Job_Queue(void)
	{
		Guard_t guard(m_jobs_mtx);
		m_jobs.Clear();
	} // end method Empty_Job_Queue


	///<summary>
	/// Blocks until all pending jobs have been completed.
	///</summary>
	///<returns>
	/// True on successful completion of all pending jobs.
	/// False if no threads are running.
	///</returns>
	bool Synchronize(bool show_progress = false)
	{
		if (m_nrunning == 0)
		{
			std::cerr << "ThreadPool::Synchronize invoked with 0 running threads! Did you call Start_All?" << std::endl;
			return false;
		} // end if

		std::size_t total_jobs = m_jobs.Size();
		auto start = _Clock::now();

		if (show_progress) 
		{
			std::cout << "[Thread Pool]: Synchronizing ..." << std::endl;
		} // end if
		// wait for all jobs to be assigned to a thread
		while (!m_jobs.Empty())
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			if (show_progress) 
			{
				Progress_Bar(total_jobs, start);
			} // end if
		} // end while		

		if (show_progress)
		{
			std::cout << std::endl;
		} // end if

		// wait for all threads to complete their current work
		for (auto& _signal : m_signals)
		{
			while (_signal == THREAD_SIGNALS::WORKING) 
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			} // end while
		} // end for

		if (show_progress)
		{
			std::cout << "[Thread Pool]: Synchronization completed." << std::endl;
		} // end if

		return true;
	} // end method Synchronize


	///<summary>
	/// Accessor for the number of threads running.
	///</summary>
	///<returns>The number of threads currently running.</returns>
	std::size_t N_Threads_Running(void)
	{
		return m_nrunning;
	} // end method N_Threads_Running


	///<summary>
	/// Accessor for the number of jobs not completed.
	///</summary>
	///<returns>The number of jobs that remain in the queue.</returns>
	std::size_t N_Jobs_Remaining(void)
	{
		return m_jobs.Size();
	} // end method N_Jobs_Remaining


	///<summary>
	/// Returns a list of all threads and their current states at the time of invocation.
	///</summary>
	///<returns>A vector of all thread states.</returns>
	std::vector<THREAD_SIGNALS> Thread_States(void)
	{
		Guard_t guard(m_signals_mtx);

		return std::vector<THREAD_SIGNALS>(m_signals);
	} // end method Thread_States


protected:
	///<summary>
	/// Thread main function. Threads will idle until work is available 
	/// and they have not received a sigterm from the main thread.
	///</summary>
	///<param name="_my_pool">The thread pool this thread belongs to.</param>
	///<param name="_my_id">The id of this thread within the thread pool.</param>
	static void Idle(ThreadPool* _my_pool, const std::size_t _my_id)
	{
		auto run = true;

		while(run)
		{
			_my_pool->m_jobs_mtx.lock();

			auto job = _my_pool->m_jobs.Get_Work();

			_my_pool->m_jobs_mtx.unlock();

			if (job != nullptr)
			{
				_my_pool->m_signals_mtx.lock();
				if (_my_pool->m_signals[_my_id] != THREAD_SIGNALS::SIGTERM)
				{
					_my_pool->m_signals[_my_id] = THREAD_SIGNALS::WORKING;
					_my_pool->m_signals_mtx.unlock();
					job->Execute();
				} // end if
				else
				{	// thread has been instructed to terminate
					_my_pool->m_signals_mtx.unlock();
					job->Execute();
				} // end else

				delete job;
			} // end if
			else
			{
				_my_pool->m_signals_mtx.lock();

				if (_my_pool->m_signals[_my_id] != THREAD_SIGNALS::SIGTERM)
				{
					_my_pool->m_signals[_my_id] = THREAD_SIGNALS::IDLE;
				} // end if

				_my_pool->m_signals_mtx.unlock();
				std::this_thread::sleep_for(_my_pool->m_sleep_duration);
			} // end else

			switch (_my_pool->m_signals.at(_my_id))
			{
				case THREAD_SIGNALS::SIGTERM:
					run = false;
					break;
				default:
					break;
			} // end switch
		} // end while
		_my_pool->m_signals_mtx.lock();
		_my_pool->m_signals.at(_my_id) = THREAD_SIGNALS::TERMINATING;
		_my_pool->m_signals_mtx.unlock();
	} // end method Idle


	///<summary>
	/// Prints a fancy progress bar with percentage completed and time elapsed in seconds.
	///</summary>
	///<param name="total_jobs">The total number of jobs.</param>
	///<param name="start">Start time.</param>
	void Progress_Bar(std::size_t total_jobs, std::chrono::high_resolution_clock::time_point& start)
	{
		constexpr std::size_t slots = 50;
		float progress = (static_cast<float>(total_jobs - m_jobs.Size())) / static_cast<float>(total_jobs);
		int current = static_cast<int>(progress * static_cast<float>(slots));
		std::cout << "\t\tProgress: [";

		for (auto i = 0; i < slots; i++)
		{
			if (i < current)
			{
				std::cout << "=";
			} // end if
			else if (i == current)
			{
				std::cout << ">";
			} // end elif
			else
			{
				std::cout << " ";
			} // end else
		} // end for i
		std::cout << "] " << static_cast<int>(progress * 100) << "% -- Time: " << chrono_duration<std::chrono::seconds>(start, _Clock::now()) << "s\r";
		std::cout.flush();
	} // end method Progress_Bar


private:	
	std::chrono::microseconds m_sleep_duration;
	std::size_t m_nthreads;
	std::size_t m_nrunning;
	std::vector<std::thread> m_threads;
	std::vector<THREAD_SIGNALS> m_signals;
	mutable std::mutex m_jobs_mtx;
	mutable std::mutex m_signals_mtx;
	JobQueue m_jobs;

}; // end class ThreadPool


