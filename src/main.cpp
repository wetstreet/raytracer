#include <iostream>

#include "imgui/imgui.h"
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include "raytracer.h"

using namespace std;

int main(int argc, char* argv[]) {
	glfwInit();

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(350, 250, "raytracer", NULL, NULL);
	if (window == NULL) {
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	gladLoadGL();

	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows

	ImGui::StyleColorsDark();

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330");

	GLuint renderTexture;
	glGenTextures(1, &renderTexture);
	glBindTexture(GL_TEXTURE_2D, renderTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderTexture, 0);

	bool showResult = false;

	int inputSize[2]{ image_width, image_height };

	uint8_t* pixels = nullptr;
	raytracer rt;

	auto InputDouble3 = [] (const char* label, double* v)
	{
		static float temp[3];
		for (int i = 0; i < 3; i++) temp[i] = (float)v[i];
		ImGui::InputFloat3(label, temp);
		for (int i = 0; i < 3; i++) v[i] = temp[i];
	};

	while (!glfwWindowShouldClose(window))
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

		ImGui::Begin("control");

		ImGui::InputInt2("size", inputSize);
		ImGui::InputInt("samples", &samples_per_pixel);
		ImGui::Separator();
		InputDouble3("lookfrom", (double*)&lookfrom);
		InputDouble3("lookat", (double*)&lookat);
		ImGui::InputFloat("vfov", &vfov);
		ImGui::InputFloat("aperture", &aperture);
		ImGui::InputFloat("focus distance", &dist_to_focus);
		if (ImGui::Button("render"))
		{
			image_width = inputSize[0];
			image_height = inputSize[1];

			if (pixels != nullptr)
				delete[] pixels;
			pixels = new uint8_t[image_width * image_height * 4];

			rt.render(pixels);

			showResult = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("render sync"))
		{
			image_width = inputSize[0];
			image_height = inputSize[1];

			if (pixels != nullptr)
				delete[] pixels;
			pixels = new uint8_t[image_width * image_height * 4];

			rt.render_sync(pixels);

			showResult = true;
		}
		ImGui::End();

		if (showResult)
		{
			ImGui::SetNextWindowSize(ImVec2(20 + (float)image_width, 35 + (float)image_height));
			ImGui::Begin("result", &showResult);

			glBindTexture(GL_TEXTURE_2D, renderTexture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

			ImGui::Image((ImTextureID)(intptr_t)renderTexture, ImVec2((float)image_width, (float)image_height), ImVec2(0, 1), ImVec2(1, 0));
			ImGui::End();
		}

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			GLFWwindow* backup_current_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}