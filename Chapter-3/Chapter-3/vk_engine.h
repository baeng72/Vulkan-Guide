#pragma once
//based on https://vkguide.dev/ by Victor Blanco

#include "vk_types.h"
#include "vk_initializers.h"
#include "vk_mesh.h"
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


struct PipelineBuilder {
	std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
	VkPipelineVertexInputStateCreateInfo _vertexInputInfo;
	VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
	VkViewport _viewport;
	VkRect2D _scissor;
	VkPipelineRasterizationStateCreateInfo _rasterizer;
	VkPipelineColorBlendAttachmentState _colorBlendAttachment;
	VkPipelineMultisampleStateCreateInfo _multiSampling;
	VkPipelineLayout _pipelineLayout;
	VkPipelineDepthStencilStateCreateInfo _depthStencil;

	VkPipeline build_pipeline(VkDevice device, VkRenderPass renderPass) {
		//make viewport state from our stored viewport and scissor.
		//at the moment we won't support multiple viewports and scissors
		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &_viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &_scissor;

		//Setup dummy color blending. We aren't using transparent objects yet.
		//The blending is just "no blend", but we do write to the color attachment.
		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &_colorBlendAttachment;

		//Build the actual pipeline.
		//We now use all of the info structs we have been writing into this one to create the pipeline.
		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = static_cast<uint32_t> (_shaderStages.size());
		pipelineInfo.pStages = _shaderStages.data();
		pipelineInfo.pVertexInputState = &_vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &_inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &_rasterizer;
		pipelineInfo.pMultisampleState = &_multiSampling;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.layout = _pipelineLayout;
		pipelineInfo.pDepthStencilState = &_depthStencil;
		pipelineInfo.renderPass = renderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

		//It's easy to error out on create graphics pipeline, so we handle it a bit better than the common VK_CHECK case.
		VkPipeline newPipeline = VK_NULL_HANDLE;
		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
			std::cout << "failed to create pipeline" << std::endl;
			return VK_NULL_HANDLE;
		}
		return newPipeline;
	}
};

struct DeletionQueue {
	std::deque < std::function<void()>> deletors;

	void push_function(std::function<void()>&& function){
		deletors.push_back(function);
	}

	void flush() {
		//reverse iterate the deletion queue to execute all the functions
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
			(*it)();//call functors
		}
	}
};

struct MeshPushConstants {
	glm::vec4	data;
	glm::mat4	render_matrix;
};

class VulkanEngine {
	GLFWwindow*					_window;
	VkInstance					_instance;
	VkDebugUtilsMessengerEXT	_debug_messenger; // Vulkan debug output handle
	VkSurfaceKHR				_surface; // Vulkan window surface
	VkPhysicalDevice			_chosenGPU; // gpu chosen as the default device
	VkDevice					_device; // Vulkan device for commands
	VkQueue						_graphicsQueue;
	uint32_t					_graphicsQueueFamily;
	VkSwapchainKHR				_swapchain;
	VkFormat					_swapchainImageFormat;
	std::vector<VkImage>		_swapchainImages;
	std::vector<VkImageView>	_swapchainImageViews;
	VkRenderPass				_renderPass;
	std::vector<VkFramebuffer>	_framebuffers;
	VkCommandPool				_commandPool;
	VkCommandBuffer				_mainCommandBuffer;
	VkFence						_renderFence;
	VkSemaphore					_presentSemaphore;
	VkSemaphore					_renderSemaphore;

	VkPipelineLayout			_trianglePipelineLayout;
	VkPipeline					_trianglePipeline;
	VkPipeline					_redTrianglePipeline;

	DeletionQueue				_mainDeletionQueue;

	VkPipelineLayout			_meshPipelineLayout;
	VkPipeline					_meshPipeline;
	Mesh						_triangleMesh;
	Mesh						_monkeyMesh;

	VmaAllocator				_allocator;

	//depth resources
	VkImageView					_depthImageView;
	AllocatedImage				_depthImage;

	//the format for the depth image
	VkFormat					_depthFormat;

	int							_selectedShader = 0;

	bool						_isInitialized = false;
	int							_frameNumber = 0;
	VkExtent2D					_windowExtent = { 1700, 900 };
	
	bool						framebufferResized = false;

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
		auto app = reinterpret_cast<VulkanEngine*>(glfwGetWindowUserPointer(window));
		app->framebufferResized = true;
	}
	static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		auto app = reinterpret_cast<VulkanEngine*>(glfwGetWindowUserPointer(window));
		if (action == GLFW_PRESS)
			
			app->_selectedShader = (++app->_selectedShader) % 2;
				
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


		//initalize the memory allocator
		VmaAllocatorCreateInfo allocatorInfo{};
		allocatorInfo.physicalDevice = _chosenGPU;
		allocatorInfo.device = _device;
		allocatorInfo.instance = _instance;
		vmaCreateAllocator(&allocatorInfo, &_allocator);

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



		_mainDeletionQueue.push_function([=]() {
			vkDestroySwapchainKHR(_device, _swapchain, nullptr);
			});


		//depth image size will match the window
		VkExtent3D depthImageExtent = {
			_windowExtent.width,_windowExtent.height,1
		};

		//hardcoding the depth format to 32 bit float
		_depthFormat = VK_FORMAT_D32_SFLOAT;

		//the depth image will be an image with the format we selected and Depth attachment usage flag
		VkImageCreateInfo dimg_info = vkinit::image_create_info(_depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImageExtent);

		//for the depth image, we want to allocate it from the gpu local memory
		VmaAllocationCreateInfo dimg_allocinfo{};
		dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		dimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		//allocate and create the image
		vmaCreateImage(_allocator, &dimg_info, &dimg_allocinfo, &_depthImage._image, &_depthImage._allocation, nullptr);

		//build the image-view for the depth image to use for rendering
		VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(_depthFormat, _depthImage._image, VK_IMAGE_ASPECT_DEPTH_BIT);

		VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depthImageView));

		//add to deletion queues
		_mainDeletionQueue.push_function([=]() {
			vkDestroyImageView(_device, _depthImageView, nullptr);
			vmaDestroyImage(_allocator, _depthImage._image, _depthImage._allocation);
			});

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

		_mainDeletionQueue.push_function([=]() {
			vkDestroyRenderPass(_device, _renderPass, nullptr);
			});
	}
	void init_framebuffers() {
		//create the framebuffers for the swapchain images. This will connect the render-pass to the images for rendering
		VkFramebufferCreateInfo fb_info = vkinit::framebuffer_create_info(_renderPass, _windowExtent);
		const uint32_t swapchain_imagecount = (uint32_t)_swapchainImages.size();
		_framebuffers = std::vector<VkFramebuffer>(swapchain_imagecount);

		for (uint32_t i = 0; i < swapchain_imagecount; i++) {
			fb_info.pAttachments = &_swapchainImageViews[i];
			VK_CHECK(vkCreateFramebuffer(_device, &fb_info, nullptr, &_framebuffers[i]));

			_mainDeletionQueue.push_function([=]() {
				vkDestroyFramebuffer(_device, _framebuffers[i], nullptr);
				});
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

		_mainDeletionQueue.push_function([=]() {
			vkDestroyCommandPool(_device, _commandPool, nullptr);
			});
	}

	void init_sync_structures() {
		//Create synchronization structures
		//one fence to control when the gpu has finished rendering the frame,
		//and 2 semaphores to synchronize rendering with swapchain
		//we want the fence to start signalled so we can wait on it on the first frame
		VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

		VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_renderFence));

		_mainDeletionQueue.push_function([=]() {
			vkDestroyFence(_device, _renderFence, nullptr);
			});

		VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_presentSemaphore));
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_renderSemaphore));

		_mainDeletionQueue.push_function([=]() {
			vkDestroySemaphore(_device, _presentSemaphore, nullptr);
			vkDestroySemaphore(_device, _renderSemaphore, nullptr);
			});
	}

	//loads a shader module from a spir-v file. Returns false if it errors.
	bool load_shader_module(const char* filePath, VkShaderModule* outShaderModule) {
		//open the file, with cursor at end.
		std::ifstream file(filePath, std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			return false;
		}

		//Find what the size of the file is by looking up the location of the cursor.
		//File opened with cursor at end, so cursor position = file length.
		size_t fileSize = (size_t)file.tellg();

		//spirv expects the buffer to be on uint32_t boundary, so make sure to reserive a buffer big enough for the entire file
		std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

		//put file cursor at start
		file.seekg(0);

		//load the entire file into buffer
		file.read((char*)buffer.data(), fileSize);

		//close file after loading data
		file.close();

		//create a new shader module, using the buffer we loaded
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		//codeSize has to be in bytes, so multiply the ints in the buffer by sizeof into to get real size
		createInfo.codeSize = buffer.size() * sizeof(uint32_t);
		createInfo.pCode = buffer.data();

		//check that create succeeds.
		VkShaderModule shaderModule;
		if (vkCreateShaderModule(_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
			return false;
		}
		*outShaderModule = shaderModule;
		return true;
	}
	void init_pipelines() {
		VkShaderModule triangleFragShader;
		if (!load_shader_module("Shaders/colored_triangle.frag.spv", &triangleFragShader)) {
			std::cout << "Error when building the triangle fragment shader mdoule." << std::endl;
		}
		else {
			std::cout << "Triangle fragment shader successfully loaded." << std::endl;
		}

		VkShaderModule triangleVertShader;
		if (!load_shader_module("Shaders/colored_triangle.vert.spv", &triangleVertShader)) {
			
			std::cout << "Error when building the triangle vertex shader mdoule." << std::endl;
		}
		else {
			std::cout << "Triangle vertex shader successfully loaded." << std::endl;
		}

		VkShaderModule redTriangleFragShader;
		if (!load_shader_module("Shaders/triangle.frag.spv", &redTriangleFragShader)) {
			std::cout << "Error when building the triangle fragment shader mdoule." << std::endl;
		}
		else {
			std::cout << "Red triangle fragment shader successfully loaded." << std::endl;
		}

		VkShaderModule redTriangleVertShader;
		if (!load_shader_module("Shaders/triangle.vert.spv", &redTriangleVertShader)) {

			std::cout << "Error when building the triangle vertex shader mdoule." << std::endl;
		}
		else {
			std::cout << "Red triangle vertex shader successfully loaded." << std::endl;
		}


		//build the pipeline layout that controls the inputs/outputs of the shader
		VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();

		VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_trianglePipelineLayout));


		//Builds the stage-create-info for both vertex and fragment stages. 
		//This lets the pipeline know the shader modules per stage

		PipelineBuilder pipelineBuilder;

		pipelineBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, triangleVertShader));

		pipelineBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, triangleFragShader));

		//vertex input controls how to read vertices from vertex buffers, We aren't using it yet
		pipelineBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();

		//Input assembly is the configuration for drawing triangle lists, strips, or individual points.
		pipelineBuilder._inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

		//build viewport and scissor from swapchain extents
		pipelineBuilder._viewport.x = 0.0f;
		pipelineBuilder._viewport.y = 0.0f;
		pipelineBuilder._viewport.width = (float)_windowExtent.width;
		pipelineBuilder._viewport.height = (float)_windowExtent.height;
		pipelineBuilder._viewport.minDepth = 0.0f;
		pipelineBuilder._viewport.maxDepth = 1.0f;

		pipelineBuilder._scissor.offset = { 0,0 };
		pipelineBuilder._scissor.extent = _windowExtent;

		//configure the rasterizer to draw filled triangles
		pipelineBuilder._rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);

		//we don't use multisampling, so just the default 
		pipelineBuilder._multiSampling = vkinit::multisampling_state_create_info();

		//a single blend attachment with no blending and writing to RGBA
		pipelineBuilder._colorBlendAttachment = vkinit::color_blend_attachment_state();

		//use the triangle layout we create
		pipelineBuilder._pipelineLayout = _trianglePipelineLayout;

		//default depth testing 
		pipelineBuilder._depthStencil = vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

		//finally build the pipeline
		_trianglePipeline = pipelineBuilder.build_pipeline(_device, _renderPass);

		//clear the shader stages for the builder
		pipelineBuilder._shaderStages.clear();

		//add the other shaders
		pipelineBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, redTriangleVertShader));

		pipelineBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, redTriangleFragShader));

		//build the red triangle pipeline
		

		_redTrianglePipeline = pipelineBuilder.build_pipeline(_device, _renderPass);


		//build the mesh pipeline
		VertexInputDescription vertexDescription = Vertex::get_vertex_description();

		pipelineBuilder._vertexInputInfo.pVertexAttributeDescriptions = vertexDescription.attributes.data();
		pipelineBuilder._vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexDescription.attributes.size());

		pipelineBuilder._vertexInputInfo.pVertexBindingDescriptions = vertexDescription.bindings.data();
		pipelineBuilder._vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexDescription.bindings.size());

		//clear the shader stages for the builder
		pipelineBuilder._shaderStages.clear();
		
		VkShaderModule meshVertShader;
		if (!load_shader_module("Shaders/tri_mesh_pushconstants.vert.spv", &meshVertShader)) {
			std::cout << "Error when building push constant vertex shader module" << std::endl;
		}
		else {
			std::cout << "Push constant shader successfully loaded" << std::endl;
		}

		//add the other shaders
		pipelineBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, meshVertShader));
		pipelineBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, triangleFragShader));

		//start from default empty pipeline layout
		VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();

		//setup push constants
		VkPushConstantRange push_constant;
		push_constant.offset = 0;
		push_constant.size = sizeof(MeshPushConstants);
		push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;//for vertex shader
		mesh_pipeline_layout_info.pPushConstantRanges = &push_constant;
		mesh_pipeline_layout_info.pushConstantRangeCount = 1;

		VK_CHECK(vkCreatePipelineLayout(_device, &mesh_pipeline_layout_info, nullptr, &_meshPipelineLayout));

		//hook the pushconstants layout
		pipelineBuilder._pipelineLayout = _meshPipelineLayout;
		//build the mesh triangle pipeline
		_meshPipeline = pipelineBuilder.build_pipeline(_device, _renderPass);




		vkDestroyShaderModule(_device, meshVertShader, nullptr);
		vkDestroyShaderModule(_device, redTriangleFragShader, nullptr);
		vkDestroyShaderModule(_device, redTriangleVertShader, nullptr);
		vkDestroyShaderModule(_device, triangleFragShader, nullptr);
		vkDestroyShaderModule(_device, triangleVertShader, nullptr);

		_mainDeletionQueue.push_function([=]() {
			vkDestroyPipeline(_device, _meshPipeline, nullptr);
			vkDestroyPipeline(_device, _redTrianglePipeline, nullptr);
			vkDestroyPipeline(_device, _trianglePipeline, nullptr);			
			vkDestroyPipelineLayout(_device, _meshPipelineLayout, nullptr);
			vkDestroyPipelineLayout(_device, _trianglePipelineLayout, nullptr);
			});
			
		

	}
	void upload_mesh(Mesh& mesh) {
		//allocate vertex buffer
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = mesh._vertices.size() * sizeof(Vertex);
		bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

		//let the VMA library know that this date should be writeable by CPU, but also readable by GPU
		VmaAllocationCreateInfo vmaallocInfo{};
		vmaallocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
		//allocate the buffer
		VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaallocInfo, &mesh._vertexBuffer._buffer, &mesh._vertexBuffer._allocation, nullptr));

		//add the descrution of triangle mesh buffer to the deletion queue
		_mainDeletionQueue.push_function([=]() {
			vmaDestroyBuffer(_allocator, mesh._vertexBuffer._buffer, mesh._vertexBuffer._allocation);
			});

		//copy vertex data
		void* data;
		vmaMapMemory(_allocator, mesh._vertexBuffer._allocation, &data);
		memcpy(data, mesh._vertices.data(), mesh._vertices.size() * sizeof(Vertex));
		vmaUnmapMemory(_allocator, mesh._vertexBuffer._allocation);
	}
	void load_meshes() {
		//make the array 3 vertices long
		_triangleMesh._vertices.resize(3);
		//vertex positions
		_triangleMesh._vertices[0].position = { 1.0f,1.0f,0.0f };
		_triangleMesh._vertices[1].position = { -1.0f,1.0f,0.0f };
		_triangleMesh._vertices[2].position = { 0.0f,-1.0f,0.f };

		//vertex colors, all green
		_triangleMesh._vertices[0].color = { 0.0f,1.0f,0.0f };//pure green
		_triangleMesh._vertices[1].color = { 0.0f,1.0f,0.0f };
		_triangleMesh._vertices[2].color = { 0.0f,1.0f,0.0f };

		//load the monkey
		_monkeyMesh.load_from_obj("assets/monkey_smooth.obj");
		upload_mesh(_triangleMesh);
		upload_mesh(_monkeyMesh);
	}
public:
	void init() {
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);//don't want opengl
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);//don't allow window resize
		_window = glfwCreateWindow(_windowExtent.width,_windowExtent.height, "Vulkan Engine", nullptr, nullptr);
		glfwSetWindowUserPointer(_window, this);
		glfwSetFramebufferSizeCallback(_window, framebufferResizeCallback);
		glfwSetKeyCallback(_window, keyCallback);

		init_vulkan();

		init_swapchain();

		init_default_renderpass();

		init_framebuffers();

		init_commands();

		init_sync_structures();

		init_pipelines();

		load_meshes();

		_isInitialized = true;
	}
	void cleanup() {
		if (_isInitialized) {
			//make sure the gpu has stopped doing its thing
			vkWaitForFences(_device, 1, &_renderFence, true, 1000000000);
			_mainDeletionQueue.flush();
			//destroy sync objects
			//vkDestroySemaphore(_device, _renderSemaphore, nullptr);
			//vkDestroySemaphore(_device, _presentSemaphore, nullptr);
			//vkDestroyFence(_device, _renderFence, nullptr);

			//vkDestroyCommandPool(_device, _commandPool, nullptr);

			//for (size_t i = 0; i < _framebuffers.size(); i++) {
		//		vkDestroyFramebuffer(_device, _framebuffers[i], nullptr);
			//}
			//vkDestroyRenderPass(_device, _renderPass, nullptr);

			for (size_t i = 0; i < _swapchainImageViews.size(); i++) {
				vkDestroyImageView(_device, _swapchainImageViews[i],nullptr);
			}
			//vkDestroySwapchainKHR(_device, _swapchain, nullptr);
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
		VK_CHECK(vkResetCommandBuffer(_mainCommandBuffer, 0));

		//request image from the swapchain
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

		//clear depth at 1
		VkClearValue depthClear;
		depthClear.depthStencil.depth = 1.0f;

		//start the main render pass
		//We will use the clear color from above, and the framebuffer of the index the swapchain gave us
		VkRenderPassBeginInfo rpInfo = vkinit::renderpass_begin_info(_renderPass, _windowExtent, _framebuffers[swapchainImageIndex]);

		//connect clear values
		rpInfo.clearValueCount = 2;

		VkClearValue clearValues[] = { clearValue,depthClear };
		rpInfo.pClearValues = clearValues;

		

		vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _meshPipeline);

		//bind the mesh vertex buffer with offset 0
		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(cmd, 0, 1, &_monkeyMesh._vertexBuffer._buffer, &offset);

		//make a model view matrix for rendering the object
		//camera position
		glm::vec3 camPos = { 0.0f,0.0f,-2.0f };

		glm::mat4 view = glm::translate(glm::mat4(1.0f), camPos);
		//camera projection
		glm::mat4 projection = glm::perspective(glm::radians(70.0f), 1700.0f / 900.0f, 0.1f, 200.0f);
		projection[1][1] *= -1;
		//model rotation
		glm::mat4 model = glm::rotate(glm::mat4{ 1.0f }, glm::radians(_frameNumber * 0.4f), glm::vec3(0, 1, 0));

		//calculate final mesh matrix
		glm::mat4 mesh_matrix = projection * view * model;

		MeshPushConstants constants;
		constants.render_matrix = mesh_matrix;

		//upload the matrix to the gpu via push constants
		vkCmdPushConstants(cmd, _meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &constants);

		//we can now draw the mesh
		vkCmdDraw(cmd, static_cast<uint32_t>(_monkeyMesh._vertices.size()), 1, 0, 0);

		/*if (_selectedShader == 0) {
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _trianglePipeline);
		}
		else {
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _redTrianglePipeline);
		}
		vkCmdDraw(cmd, 3, 1, 0, 0);*/

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