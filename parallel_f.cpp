// === (C) 2020 === parallel_f (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#include <fstream>

#include "parallel_f.hpp"


static void test_extract_todos(std::string initial_file);


int main()
{
	parallel_f::setDebugLevel(0);

	test_extract_todos("parallel_f.cpp");
	parallel_f::stats::instance::get().show_stats();
}


// parallel_f :: task_list/queue == testing example that extracts all todos from sources

static void test_extract_todos(std::string initial_file)
{
	auto readfile_func = [](auto filename)
	{
		parallel_f::logInfo( "  <= ::: => reading source code (file path) %s\n", filename.c_str() );

		std::ifstream filestream(filename);
		std::string   filestring;

		while (!filestream.eof()) {
			char str[1024];

			filestream.getline(str, 1024);

			filestring.append(str);
			filestring.append("\n");
		}

		return filestring;
	};

	auto parsestring_func = [](auto t1)
	{
		std::string			   filestring = t1.get<std::string>();
		std::list<std::string> included_files;

		size_t pos;

		for (pos = filestring.find("#include \""); pos < filestring.size(); pos = filestring.find("#include \"", pos + 1)) {
			size_t inc_pos = pos + 10;
			size_t inc_end = filestring.find("\"", inc_pos);

			std::string inc_filename = filestring.substr(inc_pos, inc_end - inc_pos);

			parallel_f::logInfo( "     ->> local include at position (file offset) %d <- %s\n", pos, inc_filename.c_str() );

			included_files.push_back(inc_filename);
		}

		return included_files;
	};


	std::map<std::string,bool> files;

	auto loop_func = [&files](auto t1)
	{
		std::list<std::string> included_files = t1.get<std::list<std::string>>();

		for (auto& li : included_files) {
			auto it = files.find(li);

			if (it == files.end())
				files.insert(std::make_pair(li, false));
		}

		return std::any();
	};


	files.insert(std::make_pair(initial_file, false));

	while (true) {
		bool done = true;
		parallel_f::task_list tl;
		parallel_f::task_id loop_id = 0;

		for (auto it : files) {
			if (!it.second) {
				files[it.first] = true;

				done = false;


				parallel_f::task_id id;


				auto readtask = parallel_f::make_task(readfile_func, it.first);

				id = tl.append(readtask);


				auto parsetask = parallel_f::make_task(parsestring_func, readtask->result());

				id = tl.append(parsetask, id);


				auto looptask = parallel_f::make_task(loop_func, parsetask->result());

				loop_id = tl.append(looptask, id, loop_id);
			}
		}
	
		tl.finish();

		if (done)
			break;
	}
}
