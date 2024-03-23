# parallel_f (tasks, queues, lists in parallel threads)

This project aims at flexibility, performance and usability in the area of task-parallelism.


## Intro

Imagine you have knowledge of each task's dependencies, parallel_f will run all tasks accordingly.

This is done using the task_list class. You can also use the task_queue class and care about dependencies yourself,
where task queues run in parallel with their tasks being executed sequentially.

Threads running the tasks are being managed to avoid OS overhead creating a thread per task. Instead the vthread class is used for each
task running in its own virtual thread. The vthread manager spawns one hardware thread per CPU executing the vthreads that are ready (scheduled).


## Basic Example

This is a hello world example with one task in a list:

```c++
int main()
{
	auto task = parallel_f::make_task([]() {
			std::cout << "Hello World" << std::endl;
		});
	parallel_f::task_list tl;
	tl.append(task);
	tl.finish();
	parallel_f::stats::instance::get().show_stats();
}
```

The main thread is creating a task via make_task, appends it to the list and calls finish to schedule it, waiting for it to be done.


## Advanced Example

Let's look at a more complicated case with dependencies:

```c++
int main()
{
	auto func = [](auto a)
	{
		parallel_f::logInfo("Function %s...\n", a.c_str());
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		parallel_f::logInfo("Function %s done.\n", a.c_str());
	};
	std::vector<std::shared_ptr<parallel_f::task_base>> tasks;
	for (int i=0; i<17; i++)
		tasks.push_back(parallel_f::make_task(func, std::string("running task") + std::to_string(i)));
	parallel_f::task_list tl;
	auto a1_id = tl.append(tasks[0]);
	auto a2_id = tl.append(tasks[1]);
	auto a3_id = tl.append(tasks[2]);
	auto a4_id = tl.append(tasks[3]);
	auto a5_id = tl.append(tasks[4], a1_id, a2_id);
	auto a6_id = tl.append(tasks[5], a2_id, a3_id);
	auto a7_id = tl.append(tasks[6], a3_id, a4_id);
	auto a8_id = tl.append(tasks[7], a5_id);
	auto a9_id = tl.append(tasks[8], a6_id);
	auto a10_id = tl.append(tasks[9], a7_id);
	auto a11_id = tl.append(tasks[10], a8_id, a9_id, a10_id);
	auto a12_id = tl.append(tasks[11], a8_id, a9_id, a10_id);
	auto a13_id = tl.append(tasks[12], a8_id, a9_id, a10_id);
	auto a14_id = tl.append(tasks[13], a8_id, a9_id, a10_id);
	auto a15_id = tl.append(tasks[14], a8_id);
	auto a16_id = tl.append(tasks[15], a9_id);
	auto a17_id = tl.append(tasks[16], a10_id);
	tl.finish();
	parallel_f::stats::instance::get().show_stats();
}
```

The output looks like:

```text
(*) [07:55:26.430] ( 4140) Function running task0...
(*) [07:55:26.432] ( 5140) Function running task1...
(*) [07:55:26.432] ( 5312) Function running task3...
(*) [07:55:26.432] (11412) Function running task2...
(*) [07:55:26.538] (11412) Function running task2 done.
(*) [07:55:26.538] ( 5312) Function running task3 done.
(*) [07:55:26.538] ( 5140) Function running task1 done.
(*) [07:55:26.538] ( 5312) Function running task6...
(*) [07:55:26.538] ( 3256) Function running task5...
(*) [07:55:26.538] ( 4140) Function running task0 done.
(*) [07:55:26.539] (11412) Function running task4...
(*) [07:55:26.650] (11412) Function running task4 done.
(*) [07:55:26.650] ( 3256) Function running task5 done.
(*) [07:55:26.650] ( 4140) Function running task8...
(*) [07:55:26.650] (11412) Function running task7...
(*) [07:55:26.650] ( 5312) Function running task6 done.
(*) [07:55:26.651] ( 3256) Function running task9...
(*) [07:55:26.762] (11412) Function running task7 done.
(*) [07:55:26.762] ( 3256) Function running task9 done.
(*) [07:55:26.762] ( 5312) Function running task14...
(*) [07:55:26.762] ( 5140) Function running task16...
(*) [07:55:26.762] ( 4140) Function running task8 done.
(*) [07:55:26.762] (11412) Function running task10...
(*) [07:55:26.762] ( 3256) Function running task11...
(*) [07:55:26.762] ( 4140) Function running task12...
(*) [07:55:26.871] ( 4140) Function running task12 done.
(*) [07:55:26.871] ( 3256) Function running task11 done.
(*) [07:55:26.871] (11412) Function running task10 done.
(*) [07:55:26.871] ( 3256) Function running task13...
(*) [07:55:26.871] ( 5140) Function running task16 done.
(*) [07:55:26.871] ( 5312) Function running task14 done.
(*) [07:55:26.871] (11412) Function running task15...
(*) [07:55:26.991] (11412) Function running task15 done.
(*) [07:55:26.991] ( 3256) Function running task13 done.
Load '0': 0.588 (3 vthreads)
Load '1': 0.386 (2 vthreads)
Load '2': 0.998 (5 vthreads)
Load '3': 0.587 (3 vthreads)
Load '4': 0.809 (4 vthreads)
Load all: 3.368 (17 vthreads), total busy 330.526% (0.570 seconds)
```
