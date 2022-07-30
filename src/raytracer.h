#pragma once

#include "rtweekend.h"

#include "hittable_list.h"
#include "sphere.h"
#include "camera.h"
#include "material.h"
#include "moving_sphere.h"
#include "aarect.h"
#include "box.h"
//#include "constant_medium.h"
#include "bvh.h"
#include "pdf.h"

#include "ThreadPool.h"

color ray_color(
	const ray& r, const color& background, const hittable& world,
	shared_ptr<hittable>& lights, int depth
) {
	hit_record rec;

	// If we've exceeded the ray bounce limit, no more light is gathered.
	if (depth <= 0)
		return color(0, 0, 0);

	// If the ray hits nothing, return the background color.
	if (!world.hit(r, 0.001, infinity, rec))
		return background;

	ray scattered;
	color attenuation;
	color emitted = rec.mat_ptr->emitted(r, rec, rec.u, rec.v, rec.p);
	double pdf_val;
	color albedo;
	if (!rec.mat_ptr->scatter(r, rec, albedo, scattered, pdf_val))
		return emitted;
	
    auto p0 = make_shared<hittable_pdf>(lights, rec.p);
    auto p1 = make_shared<cosine_pdf>(rec.normal);
    mixture_pdf mixed_pdf(p0, p1);

    scattered = ray(rec.p, mixed_pdf.generate(), r.time());
    pdf_val = mixed_pdf.value(scattered.direction());

	return emitted
		+ albedo * rec.mat_ptr->scattering_pdf(r, rec, scattered)
		* ray_color(scattered, background, world, lights, depth - 1) / pdf_val;
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
	shared_ptr<hittable> lights;
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

	void init_cornell_box()
	{
		lights = make_shared<xz_rect>(213, 343, 227, 332, 554, shared_ptr<material>());

		auto red = make_shared<lambertian>(color(.65, .05, .05));
		auto white = make_shared<lambertian>(color(.73, .73, .73));
		auto green = make_shared<lambertian>(color(.12, .45, .15));
		auto light = make_shared<diffuse_light>(color(15, 15, 15));

		world.add(make_shared<yz_rect>(0, 555, 0, 555, 555, green));
		world.add(make_shared<yz_rect>(0, 555, 0, 555, 0, red));
		world.add(make_shared<flip_face>(make_shared<xz_rect>(213, 343, 227, 332, 554, light)));
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
						pixel_color += ray_color(r, background, world, lights, max_depth);
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
					pixel_color += ray_color(r, background, world, lights, max_depth);
				}

				write_color(pixel_color, i, j);
			}
		}
		std::cout << "render sync finished, spent " << glfwGetTime() - startTime << "s." << std::endl;
	}
};