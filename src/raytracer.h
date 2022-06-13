#pragma once

#include "rtweekend.h"

#include "hittable_list.h"
#include "sphere.h"
#include "camera.h"

#include "ThreadPool.h"

color ray_color(const ray& r, const hittable& world) {
	hit_record rec;
	if (world.hit(r, 0, infinity, rec)) {
		return 0.5 * (rec.normal + color(1, 1, 1));
	}
	vec3 unit_direction = unit_vector(r.direction());
	auto t = 0.5 * (unit_direction.y() + 1.0);
	return (1.0 - t) * color(1.0, 1.0, 1.0) + t * color(0.5, 0.7, 1.0);
}

int image_width = 400;
int image_height = 225;
int samples_per_pixel = 100;

// multi-threading
ThreadPool pool(std::thread::hardware_concurrency());
std::mutex tile_mutex;
int finishedTileCount = 0;
int totalTileCount = 0;
double startTime = 0;
int tileSize = 16;

class raytracer {
	hittable_list world;
	camera cam;
	uint8_t* pixels = nullptr;
public:
	void render(uint8_t* _pixels)
	{
		pixels = _pixels;
		startTime = glfwGetTime();

		// World
		world.clear();
		world.add(make_shared<sphere>(point3(0, 0, -1), 0.5));
		world.add(make_shared<sphere>(point3(0, -100.5, -1), 100));

		// Camera
		cam.init(image_width / (float)image_height);

		int xTiles = (image_width + tileSize - 1) / tileSize;
		int yTiles = (image_height + tileSize - 1) / tileSize;

		totalTileCount = xTiles * yTiles;
		finishedTileCount = 0;

		auto renderTile = [&](int xTile, int yTile) {
			int xStart = xTile * tileSize;
			int yStart = yTile * tileSize;
			for (int j = yStart; j < yStart + tileSize; j++)
			{
				for (int i = xStart; i < xStart + tileSize; i++)
				{
					// bounds check
					if (i >= image_width || j >= image_height)
						continue;

					color pixel_color(0, 0, 0);
					for (int s = 0; s < samples_per_pixel; s++) {
						auto u = (i + random_double()) / (image_width - 1);
						auto v = (j + random_double()) / (image_height - 1);
						ray r = cam.get_ray(u, v);
						pixel_color += ray_color(r, world);
					}

					auto r = pixel_color.x();
					auto g = pixel_color.y();
					auto b = pixel_color.z();

					// Divide the color by the number of samples.
					auto scale = 1.0 / samples_per_pixel;
					r *= scale;
					g *= scale;
					b *= scale;

					// writes color
					int index = i + j * image_width;
					pixels[index * 4] = static_cast<int>(256 * clamp(r, 0.0, 0.999));
					pixels[index * 4 + 1] = static_cast<int>(256 * clamp(g, 0.0, 0.999));
					pixels[index * 4 + 2] = static_cast<int>(256 * clamp(b, 0.0, 0.999));
					pixels[index * 4 + 3] = 255;
				}
			}
			{
				std::lock_guard<std::mutex> lock(tile_mutex);
				finishedTileCount++;
				if (finishedTileCount == totalTileCount)
				{
					std::cout << "render finished, spent " << glfwGetTime() - startTime << "s." << std::endl;
				}
			}
		};

		for (int i = 0; i < xTiles; i++)
		{
			for (int j = 0; j < yTiles; j++)
			{
				pool.enqueue(renderTile, i, j);
			}
		}
	}

	void render_sync(uint8_t* _pixels)
	{
		pixels = _pixels;
		startTime = glfwGetTime();

		// World
		world.clear();
		world.add(make_shared<sphere>(point3(0, 0, -1), 0.5));
		world.add(make_shared<sphere>(point3(0, -100.5, -1), 100));

		// Camera
		cam.init(image_width / (float)image_height);

		for (int j = 0; j < image_height; j++)
		{
			for (int i = 0; i < image_width; i++) // go horizontal line first
			{
				color pixel_color(0, 0, 0);
				for (int s = 0; s < samples_per_pixel; ++s) {
					auto u = (i + random_double()) / (image_width - 1);
					auto v = (j + random_double()) / (image_height - 1);
					ray r = cam.get_ray(u, v);
					pixel_color += ray_color(r, world);
				}

				auto r = pixel_color.x();
				auto g = pixel_color.y();
				auto b = pixel_color.z();

				// Divide the color by the number of samples.
				auto scale = 1.0 / samples_per_pixel;
				r *= scale;
				g *= scale;
				b *= scale;

				// writes color
				int index = i + j * image_width;
				pixels[index * 4] = static_cast<int>(256 * clamp(r, 0.0, 0.999));
				pixels[index * 4 + 1] = static_cast<int>(256 * clamp(g, 0.0, 0.999));
				pixels[index * 4 + 2] = static_cast<int>(256 * clamp(b, 0.0, 0.999));
				pixels[index * 4 + 3] = 255;
			}
		}
		std::cout << "render finished, spent " << glfwGetTime() - startTime << "s." << std::endl;
	}
};