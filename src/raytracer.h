#pragma once

#include "rtweekend.h"

#include "hittable_list.h"
#include "sphere.h"
#include "camera.h"
#include "material.h"
#include "moving_sphere.h"
#include "aarect.h"
#include "box.h"
#include "constant_medium.h"
#include "bvh.h"

#include "ThreadPool.h"

color ray_color(const ray& r, const color& background, const hittable& world, int depth) {
	hit_record rec;

	// If we've exceeded the ray bounce limit, no more light is gathered.
	if (depth <= 0)
		return color(0, 0, 0);

	// If the ray hits nothing, return the background color.
	if (!world.hit(r, 0.001, infinity, rec))
		return background;

	ray scattered;
	color attenuation;
	color emitted = rec.mat_ptr->emitted(rec.u, rec.v, rec.p);
	double pdf;
	color albedo;

	if (!rec.mat_ptr->scatter(r, rec, albedo, scattered, pdf))
		return emitted;

	return emitted
		+ albedo * rec.mat_ptr->scattering_pdf(r, rec, scattered)
		* ray_color(scattered, background, world, depth - 1) / pdf;
}

// screen
int image_width = 600;
int image_height = 600;
int samples_per_pixel = 100;
const int max_depth = 50;

// camera
point3 lookfrom(278, 278, -800);
point3 lookat(278, 278, 0);
vec3 vup(0, 1, 0);
float vfov = 40;
float dist_to_focus = 10.0f;
float aperture = 0.0f;
color background(0, 0, 0);

// multi-threading
ThreadPool pool(std::thread::hardware_concurrency() - 1);
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

	void init_two_spheres()
	{
		auto checker = make_shared<checker_texture>(color(0.2, 0.3, 0.1), color(0.9, 0.9, 0.9));

		world.add(make_shared<sphere>(point3(0, -10, 0), 10, make_shared<lambertian>(checker)));
		world.add(make_shared<sphere>(point3(0, 10, 0), 10, make_shared<lambertian>(checker)));
	}

	void init_two_perlin_spheres()
	{
		auto pertext = make_shared<noise_texture>(4);

		world.add(make_shared<sphere>(point3(0, -1000, 0), 1000, make_shared<lambertian>(pertext)));
		world.add(make_shared<sphere>(point3(0, 2, 0), 2, make_shared<lambertian>(pertext)));
	}

	void init_earth()
	{
		auto earth_texture = make_shared<image_texture>("res/earthmap.jpg");
		auto earth_surface = make_shared<lambertian>(earth_texture);
		auto globe = make_shared<sphere>(point3(0, 0, 0), 2, earth_surface);

		world.add(globe);
	}

	void init_simple_light()
	{
		auto pertext = make_shared<noise_texture>(4);
		world.add(make_shared<sphere>(point3(0, -1000, 0), 1000, make_shared<lambertian>(pertext)));
		world.add(make_shared<sphere>(point3(0, 2, 0), 2, make_shared<lambertian>(pertext)));

		auto difflight = make_shared<diffuse_light>(color(4, 4, 4));
		world.add(make_shared<xy_rect>(3, 5, 1, 3, -2, difflight));

		background = color(0, 0, 0);
	}

	void init_cornell_box()
	{
		auto red = make_shared<lambertian>(color(.65, .05, .05));
		auto white = make_shared<lambertian>(color(.73, .73, .73));
		auto green = make_shared<lambertian>(color(.12, .45, .15));
		auto light = make_shared<diffuse_light>(color(15, 15, 15));

		world.add(make_shared<yz_rect>(0, 555, 0, 555, 555, green));
		world.add(make_shared<yz_rect>(0, 555, 0, 555, 0, red));
		world.add(make_shared<xz_rect>(213, 343, 227, 332, 554, light));
		world.add(make_shared<xz_rect>(0, 555, 0, 555, 0, white));
		world.add(make_shared<xz_rect>(0, 555, 0, 555, 555, white));
		world.add(make_shared<xy_rect>(0, 555, 0, 555, 555, white));

		shared_ptr<hittable> box1 = make_shared<box>(point3(0, 0, 0), point3(165, 330, 165), white);
		box1 = make_shared<rotate_y>(box1, 15);
		box1 = make_shared<translate>(box1, vec3(265, 0, 295));
		world.add(box1);

		shared_ptr<hittable> box2 = make_shared<box>(point3(0, 0, 0), point3(165, 165, 165), white);
		box2 = make_shared<rotate_y>(box2, -18);
		box2 = make_shared<translate>(box2, vec3(130, 0, 65));
		world.add(box2);
	}

	void init_cornell_smoke()
	{
		auto red = make_shared<lambertian>(color(.65, .05, .05));
		auto white = make_shared<lambertian>(color(.73, .73, .73));
		auto green = make_shared<lambertian>(color(.12, .45, .15));
		auto light = make_shared<diffuse_light>(color(7, 7, 7));

		world.add(make_shared<yz_rect>(0, 555, 0, 555, 555, green));
		world.add(make_shared<yz_rect>(0, 555, 0, 555, 0, red));
		world.add(make_shared<xz_rect>(113, 443, 127, 432, 554, light));
		world.add(make_shared<xz_rect>(0, 555, 0, 555, 0, white));
		world.add(make_shared<xz_rect>(0, 555, 0, 555, 555, white));
		world.add(make_shared<xy_rect>(0, 555, 0, 555, 555, white));

		shared_ptr<hittable> box1 = make_shared<box>(point3(0, 0, 0), point3(165, 330, 165), white);
		box1 = make_shared<rotate_y>(box1, 15);
		box1 = make_shared<translate>(box1, vec3(265, 0, 295));

		shared_ptr<hittable> box2 = make_shared<box>(point3(0, 0, 0), point3(165, 165, 165), white);
		box2 = make_shared<rotate_y>(box2, -18);
		box2 = make_shared<translate>(box2, vec3(130, 0, 65));

		world.add(make_shared<constant_medium>(box1, 0.01, color(0, 0, 0)));
		world.add(make_shared<constant_medium>(box2, 0.01, color(1, 1, 1)));
	}

	void init_random_scene()
	{
		auto checker = make_shared<checker_texture>(color(0.2, 0.3, 0.1), color(0.9, 0.9, 0.9));
		world.add(make_shared<sphere>(point3(0, -1000, 0), 1000, make_shared<lambertian>(checker)));

		for (int a = -11; a < 11; a++) {
			for (int b = -11; b < 11; b++) {
				auto choose_mat = random_double();
				point3 center(a + 0.9 * random_double(), 0.2, b + 0.9 * random_double());

				if ((center - point3(4, 0.2, 0)).length() > 0.9) {
					shared_ptr<material> sphere_material;

					if (choose_mat < 0.8) {
						// diffuse
						auto albedo = color::random() * color::random();
						sphere_material = make_shared<lambertian>(albedo);
						auto center2 = center + vec3(0, random_double(0, .5), 0);
						world.add(make_shared<moving_sphere>(
							center, center2, 0.0, 1.0, 0.2, sphere_material));
					}
					else if (choose_mat < 0.95) {
						// metal
						auto albedo = color::random(0.5, 1);
						auto fuzz = random_double(0, 0.5);
						sphere_material = make_shared<metal>(albedo, fuzz);
						world.add(make_shared<sphere>(center, 0.2, sphere_material));
					}
					else {
						// glass
						sphere_material = make_shared<dielectric>(1.5);
						world.add(make_shared<sphere>(center, 0.2, sphere_material));
					}
				}
			}
		}

		auto material1 = make_shared<dielectric>(1.5);
		world.add(make_shared<sphere>(point3(0, 1, 0), 1.0, material1));

		auto material2 = make_shared<lambertian>(color(0.4, 0.2, 0.1));
		world.add(make_shared<sphere>(point3(-4, 1, 0), 1.0, material2));

		auto material3 = make_shared<metal>(color(0.7, 0.6, 0.5), 0.0);
		world.add(make_shared<sphere>(point3(4, 1, 0), 1.0, material3));
	}

	void init_final_scene()
	{
		hittable_list boxes1;
		auto ground = make_shared<lambertian>(color(0.48, 0.83, 0.53));

		const int boxes_per_side = 20;
		for (int i = 0; i < boxes_per_side; i++) {
			for (int j = 0; j < boxes_per_side; j++) {
				auto w = 100.0;
				auto x0 = -1000.0 + i * w;
				auto z0 = -1000.0 + j * w;
				auto y0 = 0.0;
				auto x1 = x0 + w;
				auto y1 = random_double(1, 101);
				auto z1 = z0 + w;

				boxes1.add(make_shared<box>(point3(x0, y0, z0), point3(x1, y1, z1), ground));
			}
		}

		world.add(make_shared<bvh_node>(boxes1, 0, 1));

		auto light = make_shared<diffuse_light>(color(7, 7, 7));
		world.add(make_shared<xz_rect>(123, 423, 147, 412, 554, light));

		auto center1 = point3(400, 400, 200);
		auto center2 = center1 + vec3(30, 0, 0);
		auto moving_sphere_material = make_shared<lambertian>(color(0.7, 0.3, 0.1));
		world.add(make_shared<moving_sphere>(center1, center2, 0, 1, 50, moving_sphere_material));

		world.add(make_shared<sphere>(point3(260, 150, 45), 50, make_shared<dielectric>(1.5)));
		world.add(make_shared<sphere>(
			point3(0, 150, 145), 50, make_shared<metal>(color(0.8, 0.8, 0.9), 1.0)
			));

		auto boundary = make_shared<sphere>(point3(360, 150, 145), 70, make_shared<dielectric>(1.5));
		world.add(boundary);
		world.add(make_shared<constant_medium>(boundary, 0.2, color(0.2, 0.4, 0.9)));
		boundary = make_shared<sphere>(point3(0, 0, 0), 5000, make_shared<dielectric>(1.5));
		world.add(make_shared<constant_medium>(boundary, .0001, color(1, 1, 1)));

		auto emat = make_shared<lambertian>(make_shared<image_texture>("res/earthmap.jpg"));
		world.add(make_shared<sphere>(point3(400, 200, 400), 100, emat));
		auto pertext = make_shared<noise_texture>(0.1);
		world.add(make_shared<sphere>(point3(220, 280, 300), 80, make_shared<lambertian>(pertext)));

		hittable_list boxes2;
		auto white = make_shared<lambertian>(color(.73, .73, .73));
		int ns = 1000;
		for (int j = 0; j < ns; j++) {
			boxes2.add(make_shared<sphere>(point3::random(0, 165), 10, white));
		}

		world.add(make_shared<translate>(
			make_shared<rotate_y>(
				make_shared<bvh_node>(boxes2, 0.0, 1.0), 15),
			vec3(-100, 270, 395)
			)
		);
	}

	void render(uint8_t* _pixels)
	{
		pixels = _pixels;
		startTime = glfwGetTime();

		// world
		init_cornell_box();

		// Camera
		cam.init(lookfrom, lookat, vup, vfov, image_width / (float)image_height, aperture, dist_to_focus, 0.0, 1.0);

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
						pixel_color += ray_color(r, background, world, max_depth);
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

		// world
		init_cornell_box();

		// Camera
		cam.init(lookfrom, lookat, vup, vfov, image_width / (float)image_height, aperture, dist_to_focus, 0.0, 1.0);

		for (int j = 0; j < image_height; j++)
		{
			for (int i = 0; i < image_width; i++) // go horizontal line first
			{
				color pixel_color(0, 0, 0);
				for (int s = 0; s < samples_per_pixel; ++s) {
					auto u = (i + random_double()) / (image_width - 1);
					auto v = (j + random_double()) / (image_height - 1);
					ray r = cam.get_ray(u, v);
					pixel_color += ray_color(r, background, world, max_depth);
				}

				write_color(pixel_color, i, j);
			}
		}
		std::cout << "render sync finished, spent " << glfwGetTime() - startTime << "s." << std::endl;
	}
};