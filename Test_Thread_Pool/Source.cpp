#include <iostream>
#include <type_traits>
#include <iterator>
#include "Job.hpp"
#include "ThreadPool.hpp"

void test(void) { std::cout << "default test" << std::endl; }
void test2(int a) { std::cout << "test2 with a = " << a << std::endl; }
void test3(double a, int b) { std::cout << "test2 with a = " << a  << " and b = " << b << std::endl; }


int main()
{
	Job_Base* test_job1 = new Job<void>(test);
	Job_Base* test_job2 = new Job<void, int>(test2, 10);
	Job_Base* test_job3 = new Job<void, double, int>(test3, 3.14, 9);
	ThreadPool my_pool(2);

	my_pool.Add_Job(test_job1);
	my_pool.Add_Job(test_job2);
	my_pool.Add_Job(test_job3);
	
	auto r = my_pool.Start_All_Threads();
	
	my_pool.Synchronize();

	return 0;
}








