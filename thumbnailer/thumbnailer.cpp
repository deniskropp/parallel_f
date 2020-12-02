
#include <filesystem>
#include <iostream>
#include <sstream>

#include <SFML/Graphics.hpp>

#include <parallel_f.hpp>


static void
make_thumbnail(std::string filename)
{
	std::string thumbnail_filename;

	thumbnail_filename = filename.substr(0, filename.find_last_of(".")) + "_mini.png";

	std::stringstream ss;

	ss << "Thumbnailing " << filename << " -> " << thumbnail_filename << std::endl;

	std::cout << ss.str();

	sf::Image image;

	image.loadFromFile(filename);


	sf::RenderTexture thumb_render;

	thumb_render.create(image.getSize().x / 20, image.getSize().y / 20);

	sf::Texture tex;
	sf::Sprite sprite;

	tex.loadFromImage(image);

	sprite.setTexture(tex);
	sprite.setScale(.05f, .05f);

	thumb_render.draw(sprite);
	
	sf::Image thumb = thumb_render.getTexture().copyToImage();

	thumb.saveToFile(thumbnail_filename);
}

int main()
{
	auto func_make = [](std::string filename) {
		make_thumbnail(filename);

		return parallel_f::none;
	};

	sf::Clock clock;

	parallel_f::task_list task_list;

	for (auto& p : std::filesystem::recursive_directory_iterator(".")) {
		if (p.path().string().find(".jpg") != std::string::npos) {
			auto task = parallel_f::make_task(func_make, p.path().string());

			task_list.append(task);

			if (task_list.length() > 4)
				task_list.flush();
		}
	}

	task_list.finish();

	std::cout << "Operations took " << clock.getElapsedTime().asSeconds() << " seconds." << std::endl;
}
