
#include <filesystem>
#include <iostream>
#include <sstream>

#include <SFML/Graphics.hpp>

#include <parallel_f.hpp>


int main()
{
	auto func_load = [](std::string filename) {
		std::shared_ptr<sf::Image> image = std::make_shared<sf::Image>();

		parallel_f::logInfoF("Load %s...\n", filename.c_str());

		image->loadFromFile(filename);

		return image;
	};

	auto func_scale = [](std::string filename, auto img) {
		std::shared_ptr<sf::Image> image = img.template get<std::shared_ptr<sf::Image>>();

		parallel_f::logInfoF("Scale %s... (%ux%u->%ux%u)\n", filename.c_str(), image->getSize().x, image->getSize().y, image->getSize().x/20, image->getSize().y/20);

		sf::RenderTexture thumb_render;

		thumb_render.create({image->getSize().x / 20, image->getSize().y / 20});

		sf::Texture tex;
		sf::Sprite sprite;

		tex.loadFromImage(*image);

		sprite.setTexture(tex);
		sprite.setScale(sf::Vector2f(.05f, .05f));

		thumb_render.draw(sprite);

		std::shared_ptr<sf::Image> thumb = std::make_shared<sf::Image>(thumb_render.getTexture().copyToImage());

		return thumb;
	};

	auto func_store = [](auto img, std::string filename) {
		std::shared_ptr<sf::Image> image = img.template get<std::shared_ptr<sf::Image>>();

		parallel_f::logInfoF("Store %s...\n", filename.c_str());

		image->saveToFile(filename);
	};


	sf::Clock clock;

	parallel_f::task_list task_list;

	for (auto& p : std::filesystem::recursive_directory_iterator(".")) {
		if (p.path().string().find(".jpg") != std::string::npos) {
			std::string filename = p.path().string();

			auto task_load = parallel_f::make_task(func_load, filename);
			auto task_scale = parallel_f::make_task(func_scale, filename, task_load->result());
			auto task_store = parallel_f::make_task(func_store, task_scale->result(), filename.substr(0, filename.find_last_of(".")) + "_mini.png");

			auto id_load = task_list.append(task_load);
			auto id_scale = task_list.append(task_scale, id_load);
			auto id_store = task_list.append(task_store, id_scale);
		}
	}

	task_list.finish();

	std::cout << "Operations took " << clock.getElapsedTime().asSeconds() << " seconds." << std::endl;

	parallel_f::stats::instance::get().show_stats();
}
