#include <iostream>
#include <type_traits>
#include <iterator>
#include <future>
#include <mutex>

#include "ThreadPool.hpp"

std::mutex cout_mtx;

void test(void) { cout_mtx.lock(); std::cout << "default test" << std::endl; cout_mtx.unlock(); }
void test2(int a) { cout_mtx.lock(); std::cout << "test2 with a = " << a << std::endl; cout_mtx.unlock(); }
void test3(double a, int b) { cout_mtx.lock(); std::cout << "test2 with a = " << a  << " and b = " << b << std::endl; cout_mtx.unlock();  }
int x() { return 1; }
int x2(int y) { return y + 8; }
int x3(int k, double l) { return int(l) * k; }

auto add_heap_jobs(ThreadPool* _my_pool)
{
	// An example of adding jobs allocated on the heap to the thread pool
	// nothing changes for functions without return value
	_my_pool->Add_Job([](void) {auto f1 = std::bind(test3, 1.2345, 4); f1(); });

	// If there is a return value, we need to wrap the lambda in a task to get the future
	// and then wrap the task in a lambda to delete it after we leave the scope
	auto t = new std::packaged_task<int(void)>([](void) {auto f1 = std::bind(x3, 3, 3.14); auto res = f1(); return res; });

	// the lambda needs to get t from this scope so we can execute the task
	// and later release the memory associated with it
	_my_pool->Add_Job([t](void) { (*t)(); delete t; });

	// we return the future associated with t to the caller 
	// so they can receive the returned value
	return t->get_future();
}


int main()
{
	// storage for function results
	std::queue<std::future<int>> futures;

	// since this function is guaranteed to be running after all tasks are executed
	// we can put the jobs on the stack, otherwise they would need to be on the heap
	// we could also just directly put the void return type functions into the thread pool
	std::packaged_task<void(void)> t1(test);
	std::packaged_task<void(double, int)> t2(test3);
	std::packaged_task<void(void)> t3(std::bind(test3, 5.2, 3));
	std::packaged_task<int(void)> t4(std::bind(x));
	std::packaged_task<int(void)> t5(std::bind(x2, 5));
	std::packaged_task<int(void)> t6(std::bind(x3, 8, 1.3));

	// get our futures so we can receive return values from the thread pool
	// t1-t3 have a different type of future object, so we can't put them in 
	// the queue; we also don't need them as we will synchronize using the 
	// thread pool's built-in Synchronize function to wait for all jobs
	// to wait for a specific job you would need the future, though
	futures.push(t4.get_future());
	futures.push(t5.get_future());
	futures.push(t6.get_future());

	// instantiate a thread pool with 4 workers
	ThreadPool pool(4);

	// start up all threads so that jobs are executed as they come
	pool.Start_All_Threads();

	// add our tasks from above wrapped in lambdas
	pool.Add_Job([&](void) {t1(); });
	pool.Add_Job([&](void) {t2(3.1415, 11); });
	pool.Add_Job([&](void) {t3(); });
	pool.Add_Job([&](void) {t4(); });
	pool.Add_Job([&](void) {t5(); });
	pool.Add_Job([&](void) {t6(); });

	// we can also add functions directly with and without arguments
	// these two don't allow functions that return a value though
	// as we have no way of getting a future object for them
	pool.Add_Job(std::function<void(void)>(test));
	pool.Add_Job(std::function<void(void)>(std::bind(test3, 2.7182841828, 3)));

	// add some jobs living on the heap to the thread pool
	futures.push(add_heap_jobs(&pool));

	// wait for the pool to sync
	// we don't actually need to do this since we have futures, but if no futures are
	// stored, Synchronize must be invoked to wait for all tasks to finish before exiting the scope
	// as our jobs would be deleted at that point
	pool.Synchronize();

	// wait for futures to be ready and output results
	int i = 1;
	while (!futures.empty())
	{
		auto f = std::move(futures.front());
		f.wait();
		futures.pop();
		std::cout << "Task " << i << " result is: " << f.get() << std::endl;
		i++;
	}

	// instruct thread pool to terminate all running threads
	// this is unnecessary here as the destructor would do this anyway on the next line
	// however, it is recommended to do this if the threads are not needed for some time
	// as they will still get scheduled and waste CPU cycles on context switches only 
	// to yield the CPU if no jobs are available
	pool.Kill_All(false);

	return 0;
}


