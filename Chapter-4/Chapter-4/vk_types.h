#pragma once
//based on https://vkguide.dev/ by Victor Blanco
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <vector>
#include <fstream>
#include <iostream>
#include <functional>
#include <deque>
#include <unordered_map>

struct AllocatedBuffer {
	VkBuffer		_buffer;
	VmaAllocation	_allocation;
};

struct AllocatedImage {
	VkImage			_image;
	VmaAllocation	_allocation;
};