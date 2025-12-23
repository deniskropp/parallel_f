// === (C) 2020/2021 === parallel_f / test_flush_join (tasks, queues, lists in
// parallel threads) Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#include <SFML/Graphics.hpp>

#include "parallel_f.hpp"
#include "task_cl.hpp"

static void test_grafx();

class Grafx {
private:
  sf::RenderWindow window;

  std::vector<uint32_t> image;

  std::shared_ptr<task_cl::kernel> kernel;
  std::shared_ptr<task_cl::kernel_args> args;

  sf::Clock clock;
  unsigned int frames;
  unsigned int frames_first;
  bool update;

  parallel_f::core::task_list tl;
  parallel_f::core::task_list tl2;
  parallel_f::joinables joinables;

public:
  Grafx();
  ~Grafx();

  bool loop(unsigned int iterations);
};

int main() {
  parallel_f::set_debug_level(0);
  //	parallel_f::set_debug_level("vthread::", 1);
  //	parallel_f::set_debug_level("vthread::run", 1);
  //	parallel_f::set_debug_level("vthread::join", 1);
  //	parallel_f::set_debug_level("vthread::manager::", 1);
  //	parallel_f::set_debug_level("task::", 1);
  //	parallel_f::set_debug_level("task_base::", 1);
  //	parallel_f::set_debug_level("task_list::", 1);
  //	parallel_f::set_debug_level("task_queue::", 1);
  //	parallel_f::set_debug_level("task_node::", 1);
  //	parallel_f::set_debug_level("task_cl::", 1);
  //	parallel_f::set_debug_level("task_cl::kernel_exec::kernel_exec", 1);
  //	parallel_f::set_debug_level("OCL_Device", 1);
  //	parallel_f::set_debug_level("Grafx::main", 1);

  parallel_f::system::instance().set_auto_flush(
      parallel_f::system::AutoFlush::EndOfLine);
  //	parallel_f::system::instance().startFlushThread(1000);

  Grafx grafx;

  while (grafx.loop(500)) {
    parallel_f::stats::instance::get().show_stats();
    parallel_f::system::instance().flush();
  }
}

namespace lo {

struct vector2i {
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

enum class flags { none = 0x00, opaque = 0x01 };

struct element {
  bounds bounds;
  flags flags;
};

struct rect : element {
  color color;
};

struct triangle : element {
  color color;

  struct {
    vector2i v1;
    vector2i v2;
    vector2i v3;
  } coords;
};

enum class type { rect, triangle };

struct instance {
  type type;
  int index;
};

} // namespace lo

Grafx::Grafx()
    : window(sf::VideoMode(1000, 700), "Grafx"), frames(0), frames_first(0),
      update(true) //,
//	joinables(2)
{
  image.resize(1000 * 700);

  std::vector<lo::rect> rects(2);
  std::vector<lo::triangle> tris(0);
  std::vector<lo::instance> instances(2);

  for (int i = 0; i < rects.size(); i++) {
    rects[i].bounds.tl.x = rand() % 200;
    rects[i].bounds.tl.y = rand() % 200;
    rects[i].bounds.br.x = rects[i].bounds.tl.x + 800;
    rects[i].bounds.br.y = rects[i].bounds.tl.y + 500;
    rects[i].color.r = rand() % 1001 / 1000.0f;
    rects[i].color.g = rand() % 1001 / 1000.0f;
    rects[i].color.b = rand() % 1001 / 1000.0f;
    rects[i].color.a = rand() % 501 / 500.0f + 0.5f;
    rects[i].flags = lo::flags::none;
  }

  rects[0].bounds.tl.x = 200;
  rects[0].bounds.tl.y = 200;
  rects[0].bounds.br.x = 800;
  rects[0].bounds.br.y = 500;
  rects[0].color.r = 1.0f;
  rects[0].color.g = 0.0f;
  rects[0].color.b = 0.0f;
  rects[0].color.a = 0.7f;

  rects[1].bounds.tl.x = 400;
  rects[1].bounds.tl.y = 400;
  rects[1].bounds.br.x = 900;
  rects[1].bounds.br.y = 700;
  rects[1].color.r = 0.0f;
  rects[1].color.g = 0.0f;
  rects[1].color.b = 1.0f;
  rects[1].color.a = 0.7f;

  for (int i = 0; i < rects.size(); i++) {
    instances[i].type = lo::type::rect;
    instances[i].index = i;
  }

  kernel = task_cl::make_kernel("grafx.cl", "render", image.size(), 100);

  args = task_cl::make_args(
      new task_cl::kernel_args::kernel_arg_mem(image.size() * sizeof(uint32_t),
                                               NULL, image.data()),
      new task_cl::kernel_args::kernel_arg_mem(rects.size() * sizeof(lo::rect),
                                               rects.data(), NULL),
      new task_cl::kernel_args::kernel_arg_mem(
          tris.size() * sizeof(lo::triangle), tris.data(), NULL),
      new task_cl::kernel_args::kernel_arg_mem(
          instances.size() * sizeof(lo::instance), instances.data(), NULL),
      new task_cl::kernel_args::kernel_arg_t<cl_int>((cl_int)instances.size()));

  tq.push(task_cl::kernel_pre::make_task(args));
  tq.push(task_cl::kernel_exec::make_task(args, kernel));
  tq.push(task_cl::kernel_post::make_task(args));

  // joinable = tq.exec(true);
  tq.exec();

  clock.restart();
}

Grafx::~Grafx() {}

bool Grafx::loop(unsigned int iterations) {
  while (iterations-- && window.isOpen()) {
    LOG_DEBUG("Grafx::main loop <- frames %u\n", frames);

    if (clock.getElapsedTime().asSeconds() >= 1.0f) {
      parallel_f::log_line_f("FPS ", (frames - frames_first) /
                                         clock.restart().asSeconds());
      frames_first = frames;

      update = true;
    }

    frames++;

    tq.push(task_cl::kernel_exec::make_task(args, kernel));

    if (update)
      tq.push(task_cl::kernel_post::make_task(args));

    joinables.join_all();
    //		joinable = tq.exec(true);
    joinables.add(tq.exec(true));

    if (update) {
      sf::Image img;

      img.create(1000, 700, (uint8_t *)image.data());

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
      }
    }
  }

  return window.isOpen();
}
