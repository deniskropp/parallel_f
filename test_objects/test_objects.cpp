// === (C) 2020 === parallel_f / test_objects (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#include "parallel_f.hpp"


namespace object {

using id = unsigned long long;

class entity
{
private:
	static id next_id()
	{
		static object::id ids;

		return ++ids;
	}

public:
	id id;
	int x;
	int y;
	int seq;

	entity()
		:
		id(next_id()),
		x(0),
		y(0),
		seq(0)
	{
	}
};


class funcs
{
public:
	static constexpr auto run = [](entity* o) {
		parallel_f::logInfo("object::funcs::run(%llu)...\n", o->id);

		std::this_thread::sleep_for(std::chrono::milliseconds(10));

		o->seq++;

		parallel_f::logInfo("object::funcs::run(%llu) done.\n", o->id);

		return parallel_f::none;
	};

	static constexpr auto show = [](entity* o) {
		parallel_f::logInfo("object id %llu, x %d, y %d, seq %d\n", o->id, o->x, o->y, o->seq);

		return parallel_f::none;
	};
};

}


// parallel_f :: task_list == testing example

int main()
{
	parallel_f::setDebugLevel(0);


	std::vector<object::entity> objects(8);

	parallel_f::task_list tl;
	parallel_f::task_id flush_id = 0;

	for (int i = 0; i < 4; i++) {
		for (int i = 0; i < objects.size(); i++) {
			auto o = &objects[i];

			auto run_id = tl.append(parallel_f::make_task(object::funcs::run, o), flush_id);

			if (i == 0 || i == objects.size()-1)
				tl.append(parallel_f::make_task(object::funcs::show, o), run_id);
		}

		flush_id = tl.flush();
	}

	tl.finish();

	parallel_f::stats::instance::get().show_stats();
}