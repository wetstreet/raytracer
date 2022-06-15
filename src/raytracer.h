#pragma once

#include "rtweekend.h"

#include "hittable_list.h"
#include "sphere.h"
#include "camera.h"
#include "material.h"

#include "ThreadPool.h"

color ray_color(const ray& r, const hittable& world, int depth) {
	hit_record rec;

	// If we've exceeded the ray bounce limit, no more light is gathered.
	if (depth <= 0)
		return color(0, 0, 0);

	if (world.hit(r, 0.001, infinity, rec)) {
		ray scattered;
		color attenuation;
		if (rec.mat_ptr->scatter(r, rec, attenuation, scattered))
			return attenuation * ray_color(scattered, world, depth - 1);
		return color(0, 0, 0);
	}
	vec3 unit_direction = unit_vector(r.direction());
	auto t = 0.5 * (unit_direction.y() + 1.0);
	return (1.0 - t) * color(1.0, 1.0, 1.0) + t * color(0.5, 0.7, 1.0);
}

// screen
int image_width = 400;
int image_height = 225;
int samples_per_pixel = 100;
const int max_depth = 50;

// camera
point3 lookfrom(-2, 2, 1);
point3 lookat(0, 0, -1);
vec3 vup(0, 1, 0);
float vfov = 90;

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
	void write_color(color pixel_color, int i, int j)
	{
		auto r = pixel_color.x();
		auto g = pixel_color.y();
		auto b = pixel_color.z();

		// Divide the color by the number of samples and gamma-correct for gamma=2.0.
		auto scale = 1.0 / samples_per_pixel;
		r = sqrt(scale * r);
		g = sqrt(scale * g);
		b = sqrt(scale * b);

		// Write the translated [0,255] value of each color component.
		int index = i + j * image_width;
		pixels[index * 4] = static_cast<int>(256 * clamp(r, 0.0, 0.999));
		pixels[index * 4 + 1] = static_cast<int>(256 * clamp(g, 0.0, 0.999));
		pixels[index * 4 + 2] = static_cast<int>(256 * clamp(b, 0.0, 0.999));
		pixels[index * 4 + 3] = 255;
	}

	void init_render()
	{
		// World
		auto R = cos(pi / 4);

		auto material_ground = make_shared<lambertian>(color(0.8, 0.8, 0.0));
		auto material_center = make_shared<lambertian>(color(0.1, 0.2, 0.5));
		auto material_left = make_shared<dielectric>(1.5);
		auto material_right = make_shared<metal>(color(0.8, 0.6, 0.2), 0.0);

		world.add(make_shared<sphere>(point3(0.0, -100.5, -1.0), 100.0, material_ground));
		world.add(make_shared<sphere>(point3(0.0, 0.0, -1.0), 0.5, material_center));
		world.add(make_shared<sphere>(point3(-1.0, 0.0, -1.0), 0.5, material_left));
		world.add(make_shared<sphere>(point3(-1.0, 0.0, -1.0), -0.45, material_left));
		world.add(make_shared<sphere>(point3(1.0, 0.0, -1.0), 0.5, material_right));

		// Camera
		cam.init(lookfrom, lookat, vup, vfov, image_width / (float)image_height);

	}

	void render(uint8_t* _pixels)
	{
		pixels = _pixels;
		startTime = glfwGetTime();

		// world and camera
		init_render();

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
						pixel_color += ray_color(r, world, max_depth);
					}

					write_color(pixel_color, i, j);
				}
			}
			{
				std::lock_guard<std::mutex> lock(tile_mutex);
				finishedTileCount++;
				if (finishedTileCount == totalTileCount)
				{
					std::cout << "render async finished, spent " << glfwGetTime() - startTime << "s." << std::endl;
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

		// world and camera
		init_render();

		for (int j = 0; j < image_height; j++)
		{
			for (int i = 0; i < image_width; i++) // go horizontal line first
			{
				color pixel_color(0, 0, 0);
				for (int s = 0; s < samples_per_pixel; ++s) {
					auto u = (i + random_double()) / (image_width - 1);
					auto v = (j + random_double()) / (image_height - 1);
					ray r = cam.get_ray(u, v);
					pixel_color += ray_color(r, world, max_depth);
				}

				write_color(pixel_color, i, j);
			}
		}
		std::cout << "render sync finished, spent " << glfwGetTime() - startTime << "s." << std::endl;
	}
};