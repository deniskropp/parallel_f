// === (C) 2020/2021 === parallel_f / testing utilities
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#pragma once

#define RUN(x)	\
	do {													\
		LOG_INFO("Running '%s'...\n", #x);					\
		x;													\
		parallel_f::stats::instance::get().show_stats();	\
		parallel_f::system::instance().flush();				\
	} while (0)
