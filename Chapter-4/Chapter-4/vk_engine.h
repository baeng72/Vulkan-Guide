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

struct Material {
	VkPipeline			pipeline;
	VkPipelineLayout	pipelineLayout;
};

struct RenderObject {
	Mesh*			mesh;
	Material*		material;
	glm::mat4		tranformMatrix;
};

struct GPUObjectData {
	glm::mat4		modelMatrix;
};

struct FrameData {
	VkSemaphore	_presentSemaphore;
	VkSemaphore _renderSemaphore;
	VkFence		_renderFence;

	DeletionQueue	_frameDeletionQueue;

	VkCommandPool	_commandPool;
	VkCommandBuffer	_mainCommandBuffer;

	AllocatedBuffer	_cameraBuffer;
	VkDescriptorSet	_globalDescriptor;

	AllocatedBuffer	_objectBuffer;
	VkDescriptorSet	_objectDescriptor;
};

struct GPUCameraData {
	glm::mat4	view;
	glm::mat4	proj;
	glm::mat4	viewproj;
};

struct GPUSceneData {
	glm::vec4	fogColor;		//w is for exponent
	glm::vec4	fogDistances;	//x for min, y for max, zw unsued
	glm::vec4	ambientColor;	//
	glm::vec4	sunlightDirection;	//w for sun power
	glm::vec4	sunlightColor;
};

constexpr unsigned int FRAME_OVERLAP = 2;

class VulkanEngine {
	GLFWwindow*					_window;
	VkInstance					_instance;
	VkDebugUtilsMessengerEXT	_debug_messenger; // Vulkan debug output handle
	VkSurfaceKHR				_surface; // Vulkan window surface
	VkPhysicalDevice			_chosenGPU; // gpu chosen as the default device
	VkDevice					_device; // Vulkan device for commands
	VkPhysicalDeviceProperties	_gpuProperties;
	FrameData					_frames[FRAME_OVERLAP];
	VkQueue						_graphicsQueue;
	uint32_t					_graphicsQueueFamily;
	VkSwapchainKHR				_swapchain;
	VkFormat					_swapchainImageFormat;
	std::vector<VkImage>		_swapchainImages;
	std::vector<VkImageView>	_swapchainImageViews;
	VkRenderPass				_renderPass;
	std::vector<VkFramebuffer>	_framebuffers;
	//VkCommandPool				_commandPool;
	//VkCommandBuffer				_mainCommandBuffer;
	VkDescriptorPool			_descriptorPool;
	VkDescriptorSetLayout		_globalSetLayout;
	VkDescriptorSetLayout		_objectSetLayout;
	/*VkFence						_renderFence;
	VkSemaphore					_presentSemaphore;
	VkSemaphore					_renderSemaphore;*/

	//VkPipelineLayout			_trianglePipelineLayout;
	//VkPipeline					_trianglePipeline;
	//VkPipeline					_redTrianglePipeline;

	DeletionQueue				_mainDeletionQueue;

	//VkPipelineLayout			_meshPipelineLayout;
	//VkPipeline					_meshPipeline;
	//Mesh						_triangleMesh;
	//Mesh						_monkeyMesh;

	VmaAllocator				_allocator;

	std::vector<RenderObject>	_renderables;
	std::unordered_map<std::string, Material>	_materials;
	std::unordered_map<std::string, Mesh>		_meshes;

	GPUSceneData				_sceneParameters;
	AllocatedBuffer				_sceneParameterBuffer;



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

	Material* create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name) {
		Material mat;
		mat.pipeline = pipeline;
		mat.pipelineLayout = layout;
		_materials[name] = mat;
		return &_materials[name];
	}

	Material* get_material(const std::string& name) {
		//search for object, and return nullptr if not found
		auto it = _materials.find(name);
		if (it == _materials.end()) {
			return nullptr;
		}
		return &(*it).second;
	}

	Mesh* get_mesh(const std::string& name) {
		auto it = _meshes.find(name);
		if (it == _meshes.end()) {
			return nullptr;
		}
		return &(*it).second;
	}

	void draw_objects(VkCommandBuffer cmd, RenderObject* first, int count) {
		auto& frame = get_current_frame();
		//make a model view matrix for rendering the object
		//camera view
		glm::vec3 camPos = { 0.0f,-6.0f,-10.0f };

		glm::mat4 view = glm::translate(glm::mat4(1.0f), camPos);
		//camera projection
		glm::mat4 projection = glm::perspective(glm::radians(70.0f), 1700.0f / 900.0f, 0.1f, 200.0f);
		projection[1][1] *= -1;

		GPUCameraData camData;
		camData.proj = projection;
		camData.view = view;
		camData.viewproj = projection * view;

		void* data;
		vmaMapMemory(_allocator, frame._cameraBuffer._allocation, &data);
		memcpy(data, &camData, sizeof(GPUCameraData));
		vmaUnmapMemory(_allocator, frame._cameraBuffer._allocation);

		float framed = (_frameNumber / 120.0f);

		_sceneParameters.ambientColor = { sin(framed),0,cos(framed),1 };

		char* sceneData;
		vmaMapMemory(_allocator, _sceneParameterBuffer._allocation, (void**)&sceneData);
		int frameIndex = _frameNumber % FRAME_OVERLAP;
		sceneData += pad_uniform_buffer_size(sizeof(GPUSceneData)) * frameIndex;
		memcpy(sceneData, &_sceneParameters, sizeof(GPUSceneData));
		vmaUnmapMemory(_allocator, _sceneParameterBuffer._allocation);

		void* objectData;
		vmaMapMemory(_allocator, frame._objectBuffer._allocation, &objectData);
		GPUObjectData* objectSSBO = (GPUObjectData*)objectData;

		for (int i = 0; i < count; i++) {
			RenderObject& object = first[i];
			objectSSBO[i].modelMatrix = object.tranformMatrix;
		}

		vmaUnmapMemory(_allocator, frame._objectBuffer._allocation);

		Mesh* lastMesh = nullptr;
		Material* lastMaterial = nullptr;
		for (int i = 0; i < count; i++) {
			RenderObject& object = first[i];

			//only bind the pipeline if it doesn't match with the one already bound
			if (object.material != lastMaterial) {
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline);
				lastMaterial = object.material;

				uint32_t uniform_offset = (uint32_t)pad_uniform_buffer_size(sizeof(GPUSceneData)) * frameIndex;
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 0, 1, &frame._globalDescriptor, 1, &uniform_offset);

				//object data descriptor
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 1, 1, &frame._objectDescriptor, 0, nullptr);
			}

			glm::mat4 model = object.tranformMatrix;
			//final render matrix, that we are calculating on the cpu
			glm::mat4 mesh_matrix = model;

			MeshPushConstants constants;
			constants.render_matrix = mesh_matrix;

			//updload the mesh to the gpu via pushconstants
			vkCmdPushConstants(cmd, object.material->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &constants);

			//only bind the mesh if it's a different on from the last bind
			if (object.mesh != lastMesh) {
				//bind the mesh vertex buffer with offset 0
				VkDeviceSize offset = 0;
				vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->_vertexBuffer._buffer, &offset);
				lastMesh = object.mesh;
			}

			//we can now draw
			vkCmdDraw(cmd, (uint32_t)object.mesh->_vertices.size(), 1, 0, i);
		}
	}

	void init_scene() {
		RenderObject monkey;
		monkey.mesh = get_mesh("monkey");
		monkey.material = get_material("defaultMesh");
		monkey.tranformMatrix = glm::translate(glm::mat4{ 1.0f },glm::vec3(0,1,0));

		_renderables.push_back(monkey);

		for (int x = -20; x <= 20; x++) {
			for (int y = -20; y <= 20; y++) {
				RenderObject tri;
				tri.mesh = get_mesh("triangle");
				tri.material = get_material("defaultMesh");
				glm::mat4 translation = glm::translate(glm::mat4{ 1.0 }, glm::vec3(x, 0, y));
				glm::mat4 scale = glm::scale(glm::mat4{ 1.0 }, glm::vec3(0.2, 0.2, 0.2));
				tri.tranformMatrix = translation * scale;
				_renderables.push_back(tri);
			}
		}
	}



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




		_mainDeletionQueue.push_function([=]() {
			vmaDestroyAllocator(_allocator);
			});


		vkGetPhysicalDeviceProperties(_chosenGPU, &_gpuProperties);

		std::cout << "The gpu has a minimum buffer alignment of " << _gpuProperties.limits.minUniformBufferOffsetAlignment << std::endl;
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

		VkAttachmentDescription depth_attachment{};
		//Depth attachment
		depth_attachment.flags = 0;
		depth_attachment.format = _depthFormat;
		depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depth_attachment_ref{};
		depth_attachment_ref.attachment = 1;
		depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		//we are going to create 1 subpass, which is the minimum you can do
		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_attachment_ref;
		
		subpass.pDepthStencilAttachment = &depth_attachment_ref;


		//1 dependency, which is from "outside" into the subpass. And we can read or write color
		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkAttachmentDescription attachments[2] = { color_attachment,depth_attachment };

		VkRenderPassCreateInfo render_pass_info{};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		render_pass_info.attachmentCount = 2;
		render_pass_info.pAttachments = attachments;
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
			VkImageView attachments[2];
			attachments[0] = _swapchainImageViews[i];
			attachments[1] = _depthImageView;
			//fb_info.pAttachments = &_swapchainImageViews[i];
			fb_info.attachmentCount = 2;
			fb_info.pAttachments = attachments;
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


		for (int i = 0; i < FRAME_OVERLAP; i++) {

			VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

			//allocate the default command buffer that we will use for rendering
			VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

			VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));

			_mainDeletionQueue.push_function([=]() {
				vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);
				});
		}
	}

	void init_sync_structures() {
		//Create synchronization structures
		//one fence to control when the gpu has finished rendering the frame,
		//and 2 semaphores to synchronize rendering with swapchain
		//we want the fence to start signalled so we can wait on it on the first frame
		VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
		VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();
		for (int i = 0; i < FRAME_OVERLAP; i++) {
			VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

			_mainDeletionQueue.push_function([=]() {
				vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
				});



			VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._presentSemaphore));
			VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore));

			_mainDeletionQueue.push_function([=]() {
				vkDestroySemaphore(_device, _frames[i]._presentSemaphore, nullptr);
				vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
				});
		}
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
	void init_descriptors() {
		//create a descriptor pool that will hold 10 uniform buffers
		std::vector<VkDescriptorPoolSize> sizes = {
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,10},
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,10},
			{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,10}
		};

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.flags = 0;
		poolInfo.maxSets = 10;
		poolInfo.poolSizeCount = (uint32_t)sizes.size();
		poolInfo.pPoolSizes = sizes.data();

		vkCreateDescriptorPool(_device, &poolInfo, nullptr, &_descriptorPool);

		_mainDeletionQueue.push_function([=]() {
			vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
			});

		VkDescriptorSetLayoutBinding cameraBind = vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
		VkDescriptorSetLayoutBinding sceneBind = vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1);

		VkDescriptorSetLayoutBinding bindings[] = { cameraBind, sceneBind };

		VkDescriptorSetLayoutCreateInfo setInfo{};
		setInfo.bindingCount = 2;
		setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		setInfo.pBindings = bindings;

		vkCreateDescriptorSetLayout(_device, &setInfo, nullptr, &_globalSetLayout);


		_mainDeletionQueue.push_function([=] {
			vkDestroyDescriptorSetLayout(_device, _globalSetLayout, nullptr);
			});
		VkDescriptorSetLayoutBinding objectBind = vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);

		VkDescriptorSetLayoutCreateInfo setInfo2{};
		setInfo2.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		setInfo2.bindingCount = 1;
		setInfo2.pBindings = &objectBind;

		vkCreateDescriptorSetLayout(_device, &setInfo2, nullptr, &_objectSetLayout);


		_mainDeletionQueue.push_function([=] {
			vkDestroyDescriptorSetLayout(_device, _objectSetLayout, nullptr);
			});

		const size_t sceneParamBufferSize = FRAME_OVERLAP * pad_uniform_buffer_size(sizeof(GPUSceneData));

		_sceneParameterBuffer = create_buffer(sceneParamBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
		_mainDeletionQueue.push_function([=]() {
			vmaDestroyBuffer(_allocator, _sceneParameterBuffer._buffer, _sceneParameterBuffer._allocation);
			});

		for (int i = 0; i < FRAME_OVERLAP; i++)
		{
			_frames[i]._cameraBuffer = create_buffer(sizeof(GPUCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
			_frames[i]._frameDeletionQueue.push_function([=]() {
				vmaDestroyBuffer(_allocator, _frames[i]._cameraBuffer._buffer, _frames[i]._cameraBuffer._allocation);
				});


			const int MAX_OBJECTS = 10000;

			_frames[i]._objectBuffer = create_buffer(sizeof(GPUObjectData) * MAX_OBJECTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
			_frames[i]._frameDeletionQueue.push_function([=]() {
				vmaDestroyBuffer(_allocator, _frames[i]._objectBuffer._buffer, _frames[i]._objectBuffer._allocation);
				});
			VkDescriptorSetAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = _descriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &_globalSetLayout;

			vkAllocateDescriptorSets(_device, &allocInfo, &_frames[i]._globalDescriptor);

			VkDescriptorSetAllocateInfo objectAlloc{};
			objectAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			objectAlloc.descriptorPool = _descriptorPool;
			objectAlloc.descriptorSetCount = 1;
			objectAlloc.pSetLayouts = &_objectSetLayout;

			vkAllocateDescriptorSets(_device, &objectAlloc, &_frames[i]._objectDescriptor);

			VkDescriptorBufferInfo cameraInfo{};
			cameraInfo.buffer = _frames[i]._cameraBuffer._buffer;
			cameraInfo.offset = 0;
			cameraInfo.range = sizeof(GPUCameraData);

			VkDescriptorBufferInfo sceneInfo{};
			sceneInfo.buffer = _sceneParameterBuffer._buffer;
			sceneInfo.offset = 0;
			sceneInfo.range = sizeof(GPUSceneData);

			VkDescriptorBufferInfo objectBufferInfo{};
			objectBufferInfo.buffer = _frames[i]._objectBuffer._buffer;
			objectBufferInfo.offset = 0;
			objectBufferInfo.range = sizeof(GPUObjectData) * MAX_OBJECTS;

			VkWriteDescriptorSet cameraWrite = vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, _frames[i]._globalDescriptor, &cameraInfo, 0);

			VkWriteDescriptorSet sceneWrite = vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, _frames[i]._globalDescriptor, &sceneInfo, 1);

			VkWriteDescriptorSet objectWrite = vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, _frames[i]._objectDescriptor, &objectBufferInfo, 0);

			VkWriteDescriptorSet setWrites[] = { cameraWrite, sceneWrite, objectWrite };

			vkUpdateDescriptorSets(_device, 3, setWrites, 0, nullptr);


		}

	}
	void init_pipelines() {

		VkShaderModule	colorMeshShader;
		if (!load_shader_module("Shaders/default_lit.frag.spv", &colorMeshShader)) {
			std::cout << "Error when building the default lit shader." << std::endl;
		}
		
		VkShaderModule meshVertShader;
		if (!load_shader_module("Shaders/tri_mesh_ssbo.vert.spv", &meshVertShader)) {
			std::cout << "Error when building push constant vertex shader module" << std::endl;
		}
		else {
			std::cout << "Push constant shader successfully loaded" << std::endl;
		}

		PipelineBuilder pipelineBuilder;

		//add the other shaders
		pipelineBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, meshVertShader));
		
		pipelineBuilder._shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, colorMeshShader));

		//start from default empty pipeline layout
		VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();

		//setup push constants
		VkPushConstantRange push_constant;
		push_constant.offset = 0;
		push_constant.size = sizeof(MeshPushConstants);
		push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;//for vertex shader
		mesh_pipeline_layout_info.pPushConstantRanges = &push_constant;
		mesh_pipeline_layout_info.pushConstantRangeCount = 1;

		VkDescriptorSetLayout setLayouts[] = { _globalSetLayout,_objectSetLayout };

		mesh_pipeline_layout_info.setLayoutCount = 2;
		mesh_pipeline_layout_info.pSetLayouts = setLayouts;

		VkPipelineLayout meshPipelineLayout;

		VK_CHECK(vkCreatePipelineLayout(_device, &mesh_pipeline_layout_info, nullptr, &meshPipelineLayout));

		//hook the pushconstants layout
		pipelineBuilder._pipelineLayout = meshPipelineLayout;
		//build the mesh triangle pipeline
		//_meshPipeline = pipelineBuilder.build_pipeline(_device, _renderPass);

		//vertex input controls how to read vertices from vertex buffers.
		pipelineBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();

		//input assembly is the configuration for drawing triangle lists, strips or points.
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

		//default depth testing 
		pipelineBuilder._depthStencil = vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

		//build the mesh pipeline
		VertexInputDescription vertexDescription = Vertex::get_vertex_description();

		pipelineBuilder._vertexInputInfo.pVertexAttributeDescriptions = vertexDescription.attributes.data();
		pipelineBuilder._vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexDescription.attributes.size());

		pipelineBuilder._vertexInputInfo.pVertexBindingDescriptions = vertexDescription.bindings.data();
		pipelineBuilder._vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexDescription.bindings.size());

		VkPipeline meshPipeline = pipelineBuilder.build_pipeline(_device, _renderPass);

		create_material(meshPipeline, meshPipelineLayout, "defaultMesh");






		vkDestroyShaderModule(_device, meshVertShader, nullptr);
		vkDestroyShaderModule(_device, colorMeshShader, nullptr);
		

		_mainDeletionQueue.push_function([=]() {
			vkDestroyPipeline(_device, meshPipeline, nullptr);
					
			vkDestroyPipelineLayout(_device, meshPipelineLayout, nullptr);
			
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
		Mesh triMesh{};
		//make the array 3 vertices long
		triMesh._vertices.resize(3);
		//vertex positions
		triMesh._vertices[0].position = { 1.0f,1.0f,0.0f };
		triMesh._vertices[1].position = { -1.0f,1.0f,0.0f };
		triMesh._vertices[2].position = { 0.0f,-1.0f,0.f };
		//vertex colors, all green
		triMesh._vertices[0].color = { 0.0f,1.0f,0.0f };//pure green
		triMesh._vertices[1].color = { 0.0f,1.0f,0.0f };
		triMesh._vertices[2].color = { 0.0f,1.0f,0.0f };

		////make the array 3 vertices long
		//_triangleMesh._vertices.resize(3);
		////vertex positions
		//_triangleMesh._vertices[0].position = { 1.0f,1.0f,0.0f };
		//_triangleMesh._vertices[1].position = { -1.0f,1.0f,0.0f };
		//_triangleMesh._vertices[2].position = { 0.0f,-1.0f,0.f };

		////vertex colors, all green
		//_triangleMesh._vertices[0].color = { 0.0f,1.0f,0.0f };//pure green
		//_triangleMesh._vertices[1].color = { 0.0f,1.0f,0.0f };
		//_triangleMesh._vertices[2].color = { 0.0f,1.0f,0.0f };

		//load the monkey
		Mesh monkeyMesh{};
		monkeyMesh.load_from_obj("assets/monkey_smooth.obj");


		//load the monkey
		//_monkeyMesh.load_from_obj("assets/monkey_smooth.obj");
		//upload_mesh(_triangleMesh);
		//upload_mesh(_monkeyMesh);
		upload_mesh(triMesh);
		upload_mesh(monkeyMesh);
		
		_meshes["monkey"] = monkeyMesh;
		_meshes["triangle"] = triMesh;
	}
	FrameData& get_current_frame() {
		return _frames[_frameNumber % FRAME_OVERLAP];
	}
	FrameData& get_last_frame() {
		return _frames[(_frameNumber - 1) % FRAME_OVERLAP];
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

		init_descriptors();

		init_pipelines();

		load_meshes();

		init_scene();

		_isInitialized = true;
	}
	void cleanup() {
		if (_isInitialized) {
			//make sure the gpu has stopped doing its thing
			vkWaitForFences(_device, 1, &get_last_frame()._renderFence, true, 1000000000);
			vkWaitForFences(_device, 1, &get_current_frame()._renderFence, true, 1000000000);
			
			for (size_t i = 0; i < FRAME_OVERLAP; i++) {
				_frames[i]._frameDeletionQueue.flush();
			}
			//vkDestroyDescriptorSetLayout(_device, _globalSetLayout, nullptr);
			//vkDestroyDescriptorSetLayout(_device, _objectSetLayout, nullptr);
			//vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
			_mainDeletionQueue.flush();
			

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
		auto frame = get_current_frame();
		VK_CHECK(vkWaitForFences(_device, 1, &frame._renderFence, true, 1000000000));
		VK_CHECK(vkResetFences(_device, 1, &frame._renderFence));

		//now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
		VK_CHECK(vkResetCommandBuffer(frame._mainCommandBuffer, 0));

		//request image from the swapchain
		uint32_t swapchainImageIndex;
		VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, 0, frame._presentSemaphore, nullptr, &swapchainImageIndex));

		//naming it cmd for shorter writing
		VkCommandBuffer cmd = frame._mainCommandBuffer;

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

		draw_objects(cmd, _renderables.data(),(int) _renderables.size());
		

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
		submit.pWaitSemaphores =&frame._presentSemaphore;
		submit.signalSemaphoreCount = 1;
		submit.pSignalSemaphores = &frame._renderSemaphore;

		//submit command buffer to the queue and execute it.
		//_renderFence will now block until the graphic commands finish execution
		VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submit, frame._renderFence));

		//prepare present
		//this will put the image we just rendered into the visible window.
		//we want to wait on the _renderSemaphore for that,
		//as it's necessary that drawing commands have finished before the image is displayed ot the user
		VkPresentInfoKHR presentInfo = vkinit::present_info();

		presentInfo.pSwapchains = &_swapchain;
		presentInfo.swapchainCount = 1;

		presentInfo.pWaitSemaphores = &frame._renderSemaphore;
		presentInfo.waitSemaphoreCount = 1;

		presentInfo.pImageIndices = &swapchainImageIndex;

		VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

		//increate the number of frames draw
		_frameNumber++;
	}
	AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) {
		//allocate vertex buffer
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = allocSize;
		bufferInfo.usage = usage;

		//let VMA library know that this data should be writeable by CPU, but also readable by GPU
		VmaAllocationCreateInfo vmaAllocInfo{};
		vmaAllocInfo.usage = memoryUsage;

		AllocatedBuffer newBuffer;
		//allocate the buffer
		VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaAllocInfo, &newBuffer._buffer, &newBuffer._allocation, nullptr));

		return newBuffer;
	}

	size_t pad_uniform_buffer_size(size_t originalSize) {
		//calculate required alignment based on minimum device offset alignment
		size_t minUboAlignment = _gpuProperties.limits.minUniformBufferOffsetAlignment;
		size_t alignedSize = originalSize;
		if (minUboAlignment > 0) {
			alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
		}
		return alignedSize;
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