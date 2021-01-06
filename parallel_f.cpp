// === (C) 2020 === parallel_f (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#include "parallel_f.hpp"


thread_local parallel_f::vthread* parallel_f::vthread::manager::current;
