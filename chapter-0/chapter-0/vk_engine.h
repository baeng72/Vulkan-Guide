#pragma once
#include "vk_types.h"

class VulkanEngine {
	bool _isInitialized = false;
	int _frameNumber = 0;
	VkExtent2D _windowExtent = { 1700, 900 };
	GLFWwindow* _window;
	bool framebufferResized = false;
	static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
		auto app = reinterpret_cast<VulkanEngine*>(glfwGetWindowUserPointer(window));
		app->framebufferResized = true;
	}
public:
	void init() {
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);//don't want opengl
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);//don't allow window resize
		_window = glfwCreateWindow(_windowExtent.width,_windowExtent.height, "Vulkan Engine", nullptr, nullptr);
		glfwSetWindowUserPointer(_window, this);
		glfwSetFramebufferSizeCallback(_window, framebufferResizeCallback);
		_isInitialized = true;
	}
	void cleanup() {
		if (_isInitialized) {
			glfwDestroyWindow(_window);
			glfwTerminate();
		}
	}
	void draw() {

	}
	void run() {
		bool bQuit = false;
		while (!bQuit) {
			//Handle events on queue
			bQuit = glfwWindowShouldClose(_window);
			if(!bQuit){
				glfwPollEvents();
				draw();
			}
		}
	}
};