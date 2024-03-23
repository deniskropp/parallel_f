// === (C) 2020-2024 === parallel_f (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#pragma once

#include <any>
#include <memory>

#include "log.hpp"



#include "task_base.hpp"


namespace parallel_f {


// parallel_f :: task_info == implementation

class task_info : public task_base, public std::enable_shared_from_this<task_info>
{
public:
    class Value
    {
        friend class task_info;

    private:
        std::shared_ptr<task_info> task;

    public:
        Value(std::shared_ptr<task_info> task) : task(task) {}

		template <typename _T>
		_T get() {
			return std::any_cast<_T>(task->value);
		}
	};

protected:
	std::any value;

protected:
	task_info() {}

	template <typename Callable, typename... Args>
	task_info(Callable callable, Args... args)
	{
		if (sizeof...(args)) {
			LOG_DEBUG("task_info::task_info(): [[%s]], showing %zu arguments...\n", typeid(Callable).name(), sizeof...(args));

			if (getDebugLevel() > 0)
				(DumpArg<Args>(args), ...);
		}
		else
			LOG_DEBUG("task_info::task_info(): [[%s]], no arguments...\n", typeid(Callable).name());
	}

public:
	Value result() { return Value(this->shared_from_this()); }

private:
	template <typename ArgType>
	void DumpArg(ArgType arg)
	{
		LOG_DEBUG("task_info::task_info():   Type: %s\n", typeid(ArgType).name());
	}
};

template <>
inline std::any task_info::Value::get() {
	return task->value;
}

template <>
inline void task_info::DumpArg(Value arg)
{
	switch (parallel_f::getDebugLevel()) {
	case 2:
		arg.task->finish();
		/* fall through */
	case 1:
		LOG_DEBUG("task_info::task_info():   Type: Value( %s )\n", arg.get<std::any>().type().name());
		break;
	case 0:
		break;
	}
}


}
