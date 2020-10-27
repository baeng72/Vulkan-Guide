#pragma once
//based on https://vkguide.dev/ by Victor Blanco
#include "vk_types.h"
#include "vk_initializers.h"
#include "VkBootstrap.h"

//we want to immediately abort when there is an error. In normal engines this would give an error message to the user, or perform a dump of state.
using namespace std;
#define VK_CHECK(x)                                                 \
	do                                                              \
	{                                                               \
		VkResult err = x;                                           \
		if (err)                                                    \
		{                                                           \
			std::cout <<"Detected Vulkan error: " << err << std::endl; \
			abort();                                                \
		}                                                           \
	} while (0)

class VulkanEngine {
	GLFWwindow* _window;
	VkInstance	_instance;
	VkDebugUtilsMessengerEXT _debug_messenger; // Vulkan debug output handle
	VkSurfaceKHR _surface; // Vulkan window surface
	VkPhysicalDevice _chosenGPU; // gpu chosen as the default device
	VkDevice _device; // Vulkan device for commands
	VkQueue	_graphicsQueue;
	uint32_t _graphicsQueueFamily;
	VkSwapchainKHR	_swapchain;
	VkFormat		_swapchainImageFormat;
	std::vector<VkImage>	_swapchainImages;
	std::vector<VkImageView>	_swapchainImageViews;
	VkRenderPass				_renderPass;
	std::vector<VkFramebuffer>	_framebuffers;
	VkCommandPool				_commandPool;
	VkCommandBuffer				_mainCommandBuffer;
	VkFence						_renderFence;
	VkSemaphore					_presentSemaphore;
	VkSemaphore					_renderSemaphore;
	bool _isInitialized = false;
	int _frameNumber = 0;
	VkExtent2D _windowExtent = { 1700, 900 };
	
	bool framebufferResized = false;
	static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
		auto app = reinterpret_cast<VulkanEngine*>(glfwGetWindowUserPointer(window));
		app->framebufferResized = true;
	}
	void init_vulkan() {
		vkb::InstanceBuilder builder;
		//make the Vulkan instance, with basic debug features
		auto inst_ret = builder.set_app_name("Example Vulkan application")
			.request_validation_layers(true)
			.require_api_version(1, 1, 0)
			.use_default_debug_messenger()
			.build();

		vkb::Instance vkb_inst = inst_ret.value();
		
		//store the instance
		_instance = vkb_inst.instance;
		//store the debug messenger
		_debug_messenger = vkb_inst.debug_messenger;

		//get the surface of the window 
		glfwCreateWindowSurface(_instance, _window, nullptr, &_surface);

		//use vkbootstrap to select a gpu.
		//We want a gpu that can write to the GLFW surface and supports Vulkan 1.1
		vkb::PhysicalDeviceSelector selector{ vkb_inst };
		vkb::PhysicalDevice physicalDevice = selector
			.set_minimum_version(1, 1)
			.set_surface(_surface)
			.select()
			.value();

		//create the final vulkan device
		vkb::DeviceBuilder deviceBuilder{ physicalDevice };

		vkb::Device vkbDevice = deviceBuilder.build().value();

		//get the VKDevice handle used in the reset of the Vulkan application
		_device = vkbDevice.device;
		_chosenGPU = physicalDevice.physical_device;

		_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();

		_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();


	}
	void init_swapchain() {

		vkb::SwapchainBuilder swapchainBuilder{ _chosenGPU, _device, _surface };

		vkb::Swapchain vkbSwapchain = swapchainBuilder
			.use_default_format_selection()
			.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
			.set_desired_extent(_windowExtent.width, _windowExtent.height)
			.build()
			.value();

		//store swapchain and its related images
		_swapchain = vkbSwapchain.swapchain;
		_swapchainImages = vkbSwapchain.get_images().value();
		_swapchainImageViews = vkbSwapchain.get_image_views().value();
		_swapchainImageFormat = vkbSwapchain.image_format;

			

	}

	void init_default_renderpass() {
		//We define an attachment description for our main color image
		//the attachment is loaded as "clear" when renderpass starts
		//the attachment is stored when renderpass ends
		//the attachment layout starts as "undefined", and transitions to "Present" so it's possible to display it
		//we dont care about stencil, and dont use multisampling
		VkAttachmentDescription color_attachment{};
		color_attachment.format = _swapchainImageFormat;
		color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference color_attachment_ref{};
		color_attachment_ref.attachment = 0;
		color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		//we are going to create 1 subpass, which is the minimum you can do
		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_attachment_ref;

		//1 dependency, which is from "outside" into the subpass. And we can read or write color
		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo render_pass_info{};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		render_pass_info.attachmentCount = 1;
		render_pass_info.pAttachments = &color_attachment;
		render_pass_info.subpassCount = 1;
		render_pass_info.pSubpasses = &subpass;
		render_pass_info.dependencyCount = 1;
		render_pass_info.pDependencies = &dependency;

		VK_CHECK(vkCreateRenderPass(_device, &render_pass_info, nullptr, &_renderPass));
	}
	void init_framebuffers() {
		//create the framebuffers for the swapchain images. This will connect the render-pass to the images for rendering
		VkFramebufferCreateInfo fb_info = vkinit::framebuffer_create_info(_renderPass, _windowExtent);
		const uint32_t swapchain_imagecount = (uint32_t)_swapchainImages.size();
		_framebuffers = std::vector<VkFramebuffer>(swapchain_imagecount);

		for (uint32_t i = 0; i < swapchain_imagecount; i++) {
			fb_info.pAttachments = &_swapchainImageViews[i];
			VK_CHECK(vkCreateFramebuffer(_device, &fb_info, nullptr, &_framebuffers[i]));
		}
	}
	void init_commands() {
		//create a command pool for commands submitted to the graphics queue.
		//we also want the pool to allow for resetting of individual command buffers
		VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

		VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_commandPool));

		//allocate the default command buffer that we will use for rendering
		VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_commandPool, 1);

		VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_mainCommandBuffer));
	}

	void init_sync_structures() {
		//Create synchronization structures
		//one fence to control when the gpu has finished rendering the frame,
		//and 2 semaphores to synchronize rendering with swapchain
		//we want the fence to start signalled so we can wait on it on the first frame
		VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

		VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_renderFence));

		VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_presentSemaphore));
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_renderSemaphore));
	}
public:
	void init() {
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);//don't want opengl
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);//don't allow window resize
		_window = glfwCreateWindow(_windowExtent.width,_windowExtent.height, "Vulkan Engine", nullptr, nullptr);
		glfwSetWindowUserPointer(_window, this);
		glfwSetFramebufferSizeCallback(_window, framebufferResizeCallback);

		init_vulkan();

		init_swapchain();

		init_default_renderpass();

		init_framebuffers();

		init_commands();

		init_sync_structures();

		_isInitialized = true;
	}
	void cleanup() {
		if (_isInitialized) {
			//make sure the gpu has stopped doing its thing
			vkWaitForFences(_device, 1, &_renderFence, true, 1000000000);
			//destroy sync objects
			vkDestroySemaphore(_device, _renderSemaphore, nullptr);
			vkDestroySemaphore(_device, _presentSemaphore, nullptr);
			vkDestroyFence(_device, _renderFence, nullptr);

			vkDestroyCommandPool(_device, _commandPool, nullptr);

			for (size_t i = 0; i < _framebuffers.size(); i++) {
				vkDestroyFramebuffer(_device, _framebuffers[i], nullptr);
			}
			vkDestroyRenderPass(_device, _renderPass, nullptr);

			for (size_t i = 0; i < _swapchainImageViews.size(); i++) {
				vkDestroyImageView(_device, _swapchainImageViews[i],nullptr);
			}
			vkDestroySwapchainKHR(_device, _swapchain, nullptr);
			vkDestroyDevice(_device, nullptr);
			vkDestroySurfaceKHR(_instance, _surface, nullptr);	//destroy surface before instance
			vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
			vkDestroyInstance(_instance, nullptr);
			glfwDestroyWindow(_window);
			glfwTerminate();
		}
	}
	void draw() {
		//wait until the gpu has finished rendering the last frame. Timeout of 1 sec.
		VK_CHECK(vkWaitForFences(_device, 1, &_renderFence, true, 1000000000));
		VK_CHECK(vkResetFences(_device, 1, &_renderFence));

		//now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
		uint32_t swapchainImageIndex;
		VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, 0, _presentSemaphore, nullptr, &swapchainImageIndex));

		//naming it cmd for shorter writing
		VkCommandBuffer cmd = _mainCommandBuffer;

		//begin the command buffer recording. We will use this command buffer exacly once, se we want to let vulkan know that
		VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

		VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

		//make a clear-color from frame number. This wil flash with a 120 frame period
		VkClearValue clearValue;
		float flash = abs(sin(_frameNumber / 120.f));
		clearValue.color = { {0.0f,0.0f,flash,1.0f} };

		//start the main render pass
		//We will use the clear color from above, and the framebuffer of the index the swapchain gave us
		VkRenderPassBeginInfo rpInfo = vkinit::renderpass_begin_info(_renderPass, _windowExtent, _framebuffers[swapchainImageIndex]);

		//connect clear values
		rpInfo.clearValueCount = 1;
		rpInfo.pClearValues = &clearValue;

		vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

		//once we start adding rendering commands, they will go here

		//finalize the render pass
		vkCmdEndRenderPass(cmd);
		//finalize the command buffer (we can no longer add commands, but it can now be executed)
		VK_CHECK(vkEndCommandBuffer(cmd));

		//prepare the submission to the queue
		//we want to wait fon the _presentSemaphore, as that semaphore is signaled when the swapchain is read
		//we will signal the _renderSemaphore, to signal that rendering has finished

		VkSubmitInfo submit = vkinit::submit_info(&cmd);
		VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		submit.pWaitDstStageMask = &waitStage;
		submit.waitSemaphoreCount = 1;
		submit.pWaitSemaphores =&_presentSemaphore;
		submit.signalSemaphoreCount = 1;
		submit.pSignalSemaphores = &_renderSemaphore;

		//submit command buffer to the queue and execute it.
		//_renderFence will now block until the graphic commands finish execution
		VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submit, _renderFence));

		//prepare present
		//this will put the image we just rendered into the visible window.
		//we want to wait on the _renderSemaphore for that,
		//as it's necessary that drawing commands have finished before the image is displayed ot the user
		VkPresentInfoKHR presentInfo = vkinit::present_info();

		presentInfo.pSwapchains = &_swapchain;
		presentInfo.swapchainCount = 1;

		presentInfo.pWaitSemaphores = &_renderSemaphore;
		presentInfo.waitSemaphoreCount = 1;

		presentInfo.pImageIndices = &swapchainImageIndex;

		VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

		//increate the number of frames draw
		_frameNumber++;
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