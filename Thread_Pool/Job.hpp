#pragma once
#include <functional> // function

class Job_Base
{
public:
	Job_Base(void) = default;
	virtual ~Job_Base(void) = default;


	///<summary>
	/// Executes this job.
	///</summary>
	virtual void Execute(void) = 0;
}; // end class Job_Base


template<typename ReturnType, typename ...Args>
class Job : public Job_Base
{	
public:
	Job(void) = default;
	virtual ~Job(void) = default;


	///<summary>
	/// Constructor that registers the given function and binds the given parameters to the function.
	///</summary>
	///<param name="_f">The function that should be called upon execution of this job.</param>
	///<param name="args">The function arguments that should be bound to <paramref name="_f" />.</param>
	Job(std::function<ReturnType(Args...)> _f, Args... args)
	{
		Register_Function(_f, args...);
	} // end Constructor


	///<summary>
	/// Registers the given function within this job object and binds the given parameters to the function.
	///</summary>
	///<param name="_f">The function that should be called upon execution of this job.</param>
	///<param name="args">The function arguments that should be bound to <paramref name="_f" />.</param>
	void Register_Function(std::function<ReturnType(Args...)> _f, Args... args)
	{
		m_bound_function = std::bind(_f, args...);
		m_is_valid = true;
	} // end method Register_Function_With_Args
	

	///<summary>
	/// Executes this job.
	///</summary>
	virtual void Execute(void)
	{
		switch (m_is_valid)
		{
		case true:
			m_bound_function();
			break;
		default:
			throw std::invalid_argument("Function was not provided before Execute was invoked!");
		} // end switch
	} // end method Execute


private:
	std::function<ReturnType(Args...)> m_my_function;
	std::function<ReturnType(void)> m_bound_function;
	bool m_is_valid;
}; // end class Job

