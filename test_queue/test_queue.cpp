// === (C) 2020 === parallel_f / test_queue (tasks, queues, lists in parallel
// threads) Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#include "../parallel_f.hpp"

// parallel_f :: task_queue == testing example

int main() {
  parallel_f::set_debug_level(0);
  parallel_f::system::instance().set_auto_flush(
      parallel_f::system::AutoFlush::EndOfLine);

  auto func1 = []() -> std::string {
    parallel_f::log_info("First function being called\n");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    return "Hello World";
  };

  auto func2 = [](parallel_f::core::task_info::Value msg) -> std::string {
    parallel_f::log_info("Second function receiving '%s'\n",
                         msg.get<std::string>().c_str());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    return "Good bye";
  };

  auto func3 = [](parallel_f::core::task_info::Value msg) -> std::string {
    parallel_f::log_info("Third function receiving '%s'\n",
                         msg.get<std::string>().c_str());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    return "End";
  };

  parallel_f::joinables j;
  parallel_f::core::task_queue tq;

  // round 1 = initial test with three tasks
  auto task1 = parallel_f::make_task(func1);
  auto task2 = parallel_f::make_task(func2, task1->result());
  auto task3 = parallel_f::make_task(func3, task2->result());

  tq.push(task1);
  tq.push(task2);
  tq.push(task3);

  j.add(tq.exec(true));

  // round 2 = initial test with three tasks
  auto task21 = parallel_f::make_task(func1);
  auto task22 = parallel_f::make_task(func3, task21->result());
  auto task23 = parallel_f::make_task(func3, task22->result());

  tq.push(task21);
  tq.push(task22);
  tq.push(task23);

  j.add(tq.exec(true));

  // round 3 = mixture of past and new tasks
  auto task31 = parallel_f::make_task(func1);
  auto task32 = parallel_f::make_task(func2, task31->result());
  auto task33 = parallel_f::make_task(func3, task32->result());

  tq.push(task31);

  parallel_f::core::task_queue queue2;

  queue2.push(task32);
  queue2.push(task33);

  //	tq.push(queue2);	<== like pushing another queue(2) instead of
  // tasks to our first queue
  auto queue2_task = parallel_f::make_task([&queue2]() {
    parallel_f::log_info("Special function running whole queue...\n");
    return queue2.exec();
  });
  tq.push(queue2_task); // TODO: move these (previous) four lines into
                        // task_queue::push(task_queue&)???

  j.add(tq.exec(true));

  j.join_all();

  parallel_f::stats::instance::get().show_stats();
}
