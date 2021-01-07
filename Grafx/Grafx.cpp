// === (C) 2020 === parallel_f / test_flush_join (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#include <SFML/Graphics.hpp>

#include "parallel_f.hpp"
#include "task_cl.hpp"

static void test_grafx();


int main()
{
	parallel_f::setDebugLevel(0);
//	parallel_f::setDebugLevel("OCL_Device", 1);
//	parallel_f::setDebugLevel("task::", 1);
//	parallel_f::setDebugLevel("task_list::", 1);
//	parallel_f::setDebugLevel("task_queue::", 1);
//	parallel_f::setDebugLevel("task_node::", 1);
//	parallel_f::setDebugLevel("task_cl::", 1);

//	parallel_f::system::instance().setAutoFlush(parallel_f::system::AutoFlush::EndOfLine);
	parallel_f::system::instance().startFlushThread(1000);

	test_grafx();

	parallel_f::stats::instance::get().show_stats();
	parallel_f::system::instance().flush();
}



namespace lo {


struct vector2i
{
	int x;
	int y;
};

struct bounds {
	vector2i tl;
	vector2i br;
};

struct color {
	float r;
	float g;
	float b;
	float a;
};

enum class flags {
	none = 0x00,
	opaque = 0x01
};

struct element
{
	bounds bounds;
	flags flags;
};

struct rect : element
{
	color color;
};

struct triangle : element
{
	color color;

	struct {
		vector2i v1;
		vector2i v2;
		vector2i v3;
	} coords;
};


enum class type
{
	rect,
	triangle
};

struct instance
{
	type type;
	int index;
};

}


static void test_grafx()
{
	std::vector<lo::rect>	  rects(2);
	std::vector<lo::triangle> tris(0);
	std::vector<lo::instance> instances(2);

	std::vector<uint32_t>	  image(1000 * 1000);


	rects[0].bounds.tl.x = 0;
	rects[0].bounds.tl.y = 0;
	rects[0].bounds.br.x = 500;
	rects[0].bounds.br.y = 500;
	rects[0].color.r = 0.0f;
	rects[0].color.g = 0.0f;
	rects[0].color.b = 1.0f;
	rects[0].color.a = 1.0f;
	rects[0].flags = lo::flags::opaque;

	rects[1].bounds.tl.x = 400;
	rects[1].bounds.tl.y = 300;
	rects[1].bounds.br.x = 700;
	rects[1].bounds.br.y = 600;
	rects[1].color.r = 1.0f;
	rects[1].color.g = 0.0f;
	rects[1].color.b = 0.0f;
	rects[1].color.a = 1.0f;
	rects[1].flags = lo::flags::opaque;

	instances[0].type = lo::type::rect;
	instances[0].index = 0;

	instances[1].type = lo::type::rect;
	instances[1].index = 1;



	auto kernel = task_cl::make_kernel("grafx.cl", "render", image.size(), 100);

	auto args = std::make_shared<task_cl::kernel_args>(new task_cl::kernel_args::kernel_arg_mem(image.size() * sizeof(uint32_t), NULL, image.data()),
													   new task_cl::kernel_args::kernel_arg_mem(rects.size() * sizeof(lo::rect), rects.data(), NULL),
													   new task_cl::kernel_args::kernel_arg_mem(tris.size() * sizeof(lo::triangle), tris.data(), NULL),
													   new task_cl::kernel_args::kernel_arg_mem(instances.size() * sizeof(lo::instance), instances.data(), NULL),
													   new task_cl::kernel_args::kernel_arg_t<cl_int>((cl_int)instances.size()));


	sf::RenderWindow window(sf::VideoMode(1000, 1000), "Grafx");

	sf::Clock clock;
	unsigned int frames = 0;
	bool update = true;

	parallel_f::task_queue tq;

	while (window.isOpen()) {
		if (clock.getElapsedTime().asSeconds() >= 2) {
			std::cout << "FPS " << (frames / clock.restart().asSeconds()) << std::endl;
			frames = 0;
		}

		frames++;


		auto task = task_cl::make_task(kernel, args);

		tq.push(task);

		tq.exec();


		if (update) {
			sf::Image img;

			img.create(1000, 1000, (uint8_t*)image.data());

			sf::Texture tex;

			tex.loadFromImage(img);

			sf::Sprite sprite;

			sprite.setTexture(tex);


			window.clear();

			window.draw(sprite);

			window.display();


			update = false;
		}



		sf::Event event;
		while (window.pollEvent(event)) {
			switch (event.type) {
			case sf::Event::Closed:
				window.close();
				break;
			case sf::Event::MouseMoved:
				rects[0].bounds.tl.x = event.mouseMove.x;
				rects[0].bounds.tl.y = event.mouseMove.y;
				rects[0].bounds.br.x = rects[0].bounds.tl.x + 500;
				rects[0].bounds.br.y = rects[0].bounds.tl.y + 500;
				break;
			case sf::Event::MouseButtonPressed:
				update = true;
				break;
			}
		}
	}
}
