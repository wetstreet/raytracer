#pragma once

#include "ray.h"
#include "vec3.h"

color ray_color(const ray& r) {
	vec3 unit_direction = unit_vector(r.direction());
	auto t = 0.5 * (unit_direction.y() + 1.0);
	return (1.0 - t) * color(1.0, 1.0, 1.0) + t * color(0.5, 0.7, 1.0);
}

void Render(uint8_t* pixels, int image_width, int image_height)
{
	// Camera
	const auto aspect_ratio = image_width / (float)image_height;
	auto viewport_height = 2.0;
	auto viewport_width = aspect_ratio * viewport_height;
	auto focal_length = 1.0;

	auto origin = point3(0, 0, 0);
	auto horizontal = vec3(viewport_width, 0, 0);
	auto vertical = vec3(0, viewport_height, 0);
	auto lower_left_corner = origin - horizontal / 2 - vertical / 2 - vec3(0, 0, focal_length);

	for (int j = 0; j < image_height; j++)
	{
		for (int i = 0; i < image_width; i++) // go horizontal line first
		{
			auto u = double(i) / (image_width - 1);
			auto v = double(j) / (image_height - 1);
			ray r(origin, lower_left_corner + u * horizontal + v * vertical - origin);
			color pixel_color = ray_color(r);

			// writes color
			int index = i + j * image_width;
			pixels[index * 4] = static_cast<int>(255.999 * pixel_color.x());
			pixels[index * 4 + 1] = static_cast<int>(255.999 * pixel_color.y());
			pixels[index * 4 + 2] = static_cast<int>(255.999 * pixel_color.z());
			pixels[index * 4 + 3] = 255;
		}
	}
}