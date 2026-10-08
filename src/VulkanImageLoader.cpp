#define GLFW_INCLUDE_VULKAN
#define STB_IMAGE_IMPLEMENTATION

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <stb_image.h>

#include <iostream>
#include <vector>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <set>
#include <stdexcept>
#include <string>

using namespace std;

struct swapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	vector<VkSurfaceFormatKHR> formats;
	vector<VkPresentModeKHR> presentModes;
};

const char* imageName = "";

class Application {
public:
	void run() {
		initVulkan_();
		mainLoop_();
		clearup_();
	}

	void createWindow(const int width, const int height, const char* name) {
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		width_ = width;
		height_ = height;

		window_ = glfwCreateWindow(width, height, name, nullptr, nullptr);
	}
private:
	GLFWwindow* window_ = nullptr;

	VkInstance instance_;
	VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
	VkDevice device_ ;
	VkQueue graphicsQueue_;
	VkQueue presentQueue_;
	uint32_t graphicsFamily_;
	uint32_t presentFamily_;
	VkRenderPass renderPass_;
	VkDescriptorSetLayout descriptorSetLayout_;
	VkPipelineLayout pipelineLayout_;
	VkPipeline graphicsPipeline_;
	VkCommandPool commandPool_;
	VkCommandBuffer commandBuffer_;
	VkImage image_;
	VkDeviceMemory imageMemory_;
	VkImageView imageView_;
	VkSampler sampler_;
	VkDescriptorPool descriptorPool_;
	VkDescriptorSet descriptorSet_;

	VkSurfaceKHR surface_;
	VkSwapchainKHR swapChain_;
	VkFormat swapChainImageFormat_;
	VkExtent2D swapChainExtent_;

	vector<VkImageView> swapChainImageViews_;
	vector<VkFramebuffer> swapChainFramebuffers_;

	VkSemaphore imageAvailableSemaphore_;
	vector<VkSemaphore> renderFinishedSemaphores_;
	VkFence inFlightFence_;

	int width_ = 0;
	int height_ = 0;

	void initVulkan_() {
		createInstance_();
		createWindowSurface_();
		pickPhysicalDevice_();
		createLogicalDevice_();
		createSwapChain_();
		createImageViews_();
		createRenderPass_();
		createDescriptorSetLayout_();
		createGraphicsPipeline_();
		createFramebuffers_();
		createCommandPool_();
		createCommandBuffer_();
		createImage_(imageName);
		createImageView_();
		createSampler_();
		createDescriptorPool_();
		createDescriptorSet_();
		createSyncObjects_();
	}

	void clearup_() {
		vkDeviceWaitIdle(device_);

		vkDestroySemaphore(device_, imageAvailableSemaphore_, nullptr);
		for (auto semaphore : renderFinishedSemaphores_) {
			vkDestroySemaphore(device_, semaphore, nullptr);
		}
		vkDestroyFence(device_, inFlightFence_, nullptr);

		vkDestroyDescriptorPool(device_, descriptorPool_, nullptr); // also frees descriptorSet_
		vkDestroySampler(device_, sampler_, nullptr);
		vkDestroyImageView(device_, imageView_, nullptr);
		vkDestroyImage(device_, image_, nullptr);
		vkFreeMemory(device_, imageMemory_, nullptr);

		vkDestroyCommandPool(device_, commandPool_, nullptr); // also frees commandBuffer_
		for (auto framebuffer : swapChainFramebuffers_) {
			vkDestroyFramebuffer(device_, framebuffer, nullptr);
		}
		vkDestroyPipeline(device_, graphicsPipeline_, nullptr);
		vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
		vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
		vkDestroyRenderPass(device_, renderPass_, nullptr);
		for (auto imageView : swapChainImageViews_) {
			vkDestroyImageView(device_, imageView, nullptr);
		}
		vkDestroySwapchainKHR(device_, swapChain_, nullptr);
		vkDestroyDevice(device_, nullptr);
		vkDestroySurfaceKHR(instance_, surface_, nullptr);
		vkDestroyInstance(instance_, nullptr);

		glfwDestroyWindow(window_);
		glfwTerminate();
	}

	void createInstance_() {
		VkApplicationInfo appInfo {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Application Name";
		appInfo.apiVersion = VK_API_VERSION_1_4;
		// add engine name if wanted.
		// add application version if wanted.

		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		VkInstanceCreateInfo createInfo {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = glfwExtensionCount;
		createInfo.ppEnabledExtensionNames = glfwExtensions;

		if(vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS) {
			cerr << "Failed to create Vulkan Instatnce!";
		}
	}

	void pickPhysicalDevice_() {
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);

		if (deviceCount == 0) {
			cerr << "Failed to find GPU with Vulkan support!";
			return;
		}

		vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

		physicalDevice_ = devices[0];
		for (auto *pDev : devices)  {
			VkPhysicalDeviceProperties deviceProperties {};
			vkGetPhysicalDeviceProperties(pDev, &deviceProperties);
			if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
				physicalDevice_ = pDev;
			}
		}
	}

	void findQueueFamilies_() {
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount, nullptr);
		vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount, queueFamilies.data());

		bool foundGraphics = false;
		bool foundPresent = false;

		for (uint32_t i = 0; i < queueFamilyCount; i++) {
			VkBool32 presentSupport = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice_, i, surface_, &presentSupport);
			bool graphicsSupport = (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;

			if (graphicsSupport && presentSupport) {
				graphicsFamily_ = presentFamily_ = i;
				return;
			}
			if (graphicsSupport && !foundGraphics) { graphicsFamily_ = i; foundGraphics = true; }
			if (presentSupport && !foundPresent) { presentFamily_ = i; foundPresent = true; }
		}

		if (!foundGraphics || !foundPresent) {
			throw runtime_error("Failed to find suitable queue families!");
		}
	}

	void createLogicalDevice_() {
		findQueueFamilies_();

		set<uint32_t> uniqueFamilies = { graphicsFamily_, presentFamily_ };
		vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		float queuePriority = 1.0f;

		for (uint32_t family : uniqueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo {};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = family;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

		VkDeviceCreateInfo createInfo {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.enabledExtensionCount = 1;
		createInfo.ppEnabledExtensionNames = deviceExtensions;

		if (vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_) != VK_SUCCESS) {
			throw runtime_error("Failed to create logical device!");
		}

		vkGetDeviceQueue(device_, graphicsFamily_, 0, &graphicsQueue_);
		vkGetDeviceQueue(device_, presentFamily_, 0, &presentQueue_);
	}

	void createWindowSurface_() {
		if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS) {
			cerr << "Failed to create window surface!";
		}
	}

	 swapChainSupportDetails querySwapChainSupport_(VkPhysicalDevice device) {
		 swapChainSupportDetails details;
		 vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &details.capabilities);
		 uint32_t formatCount;
		 vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);

		 if (formatCount != 0) {
			 details.formats.resize(formatCount);
			 vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, details.formats.data());
		 }

		 uint32_t presentModeCount;
		 vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, nullptr);

		 if (presentModeCount != 0) {
			 details.presentModes.resize(presentModeCount);
			 vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, details.presentModes.data());
		 }

		 return details;
	 }

	 VkSurfaceFormatKHR chooseSwapSurfaceFormat_(const vector<VkSurfaceFormatKHR>& availableFormats) {
		 for (const auto& availableFormat : availableFormats) {
			 if(availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				 return availableFormat;
			 }
		 }
		 return availableFormats[0];
	 }

	 VkPresentModeKHR chooseSwapPresentMode_(const vector<VkPresentModeKHR>& availablePresentModes) {
		 for (const auto& availablePresentMode : availablePresentModes) {
			 if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				 return availablePresentMode;
			 }
		 }
		 return availablePresentModes[0];
	 }

	 VkExtent2D chooseSwapExtent_(const VkSurfaceCapabilitiesKHR& capabilities) {
		 if (capabilities.currentExtent.width != UINT32_MAX) {
			 return capabilities.currentExtent;
		 } else {
			 int width, height;
			 glfwGetFramebufferSize(window_, &width, &height);

			 VkExtent2D actualExtent = {
					 static_cast<uint32_t>(width),
					 static_cast<uint32_t>(height)
			 };

			 actualExtent.width = max(capabilities.minImageExtent.width, min(capabilities.maxImageExtent.width, actualExtent.width));
			 actualExtent.height = max(capabilities.minImageExtent.height, min(capabilities.maxImageExtent.height, actualExtent.height));

			 return actualExtent;
		 }
	 }

	 void createSwapChain_() {
		 swapChainSupportDetails swapChainSupport = querySwapChainSupport_(physicalDevice_);
		 VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat_(swapChainSupport.formats);
		 VkPresentModeKHR presentMode = chooseSwapPresentMode_(swapChainSupport.presentModes);
		 VkExtent2D extent = chooseSwapExtent_(swapChainSupport.capabilities);

		 uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		 if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
			 imageCount = swapChainSupport.capabilities.maxImageCount;
		 }

		 VkSwapchainCreateInfoKHR createInfo {};
		 createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		 createInfo.surface = surface_;
		 createInfo.minImageCount = imageCount;
		 createInfo.imageFormat = surfaceFormat.format;
		 createInfo.imageColorSpace = surfaceFormat.colorSpace;
		 createInfo.imageExtent = extent;
		 createInfo.imageArrayLayers = 1;
		 createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		 uint32_t queueFamilyIndices[] = { graphicsFamily_, presentFamily_ };
		 if (graphicsFamily_ != presentFamily_) {
			 createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			 createInfo.queueFamilyIndexCount = 2;
			 createInfo.pQueueFamilyIndices = queueFamilyIndices;
		 } else {
			 createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		 }

		 createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		 createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		 createInfo.presentMode = presentMode;
		 createInfo.clipped = VK_TRUE;
		 createInfo.oldSwapchain = VK_NULL_HANDLE;

		 if (vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapChain_) != VK_SUCCESS) {
			 throw runtime_error("Failed to create swap chain!");
		 }

		 swapChainImageFormat_ = surfaceFormat.format;
		 swapChainExtent_ = extent;
	 }

	 void createImageViews_() {
		 uint32_t imageCount = 0;
		 vkGetSwapchainImagesKHR(device_, swapChain_, &imageCount, nullptr);
		 vector<VkImage> swapChainImages(imageCount);
		 vkGetSwapchainImagesKHR(device_, swapChain_, &imageCount, swapChainImages.data());
		 swapChainImageViews_.resize(swapChainImages.size());

		 for (size_t i = 0; i < swapChainImages.size(); i++) {
			 VkImageViewCreateInfo createInfo {};
			 createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			 createInfo.image = swapChainImages[i];
			 createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			 createInfo.format = swapChainImageFormat_;

			 createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			 createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			 createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			 createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

			 if (vkCreateImageView(device_, &createInfo, nullptr, &swapChainImageViews_[i]) != VK_SUCCESS) {
				 cerr << "Failed to create Image View!";
			 }
		 }
	 }

	 void createRenderPass_() {
		 VkAttachmentDescription colorAttachment {};
		 colorAttachment.format = swapChainImageFormat_;
		 colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		 colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		 colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		 colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		 colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		 colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		 colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		 VkAttachmentReference colorAttachmentRef {};
		 colorAttachmentRef.attachment = 0;
		 colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		 VkSubpassDescription subpass {};
		 subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		 subpass.colorAttachmentCount = 1;
		 subpass.pColorAttachments = &colorAttachmentRef;

		 VkSubpassDependency dependency {};
		 dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		 dependency.dstSubpass = 0;
		 dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		 dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		 dependency.srcAccessMask = 0;
		 dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		 VkRenderPassCreateInfo renderPassInfo {};
		 renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		 renderPassInfo.attachmentCount = 1;
		 renderPassInfo.pAttachments = &colorAttachment;
		 renderPassInfo.subpassCount = 1;
		 renderPassInfo.pSubpasses = &subpass;
		 renderPassInfo.dependencyCount = 1;
		 renderPassInfo.pDependencies = &dependency;

		 if (vkCreateRenderPass(device_, &renderPassInfo, nullptr, &renderPass_) != VK_SUCCESS) {
			 cerr << "Failed to create render pass!";
		 }
	 }

	 vector<char> readFile_(const string& filename) {
		 ifstream file(filename, ios::ate | ios::binary);
		 if (!file.is_open()) {
			 cerr << "Failed to open file!";
			 return {};
		 }
		 size_t fileSize = (size_t)file.tellg();
		 vector<char> buffer(fileSize);
		 file.seekg(0);
		 file.read(buffer.data(), fileSize);
		 file.close();
		 return buffer;
	 }

	 VkShaderModule createShaderModule_(const vector<char>& code) {
		 VkShaderModuleCreateInfo createInfo {};
		 createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		 createInfo.codeSize = code.size();
		 createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		 VkShaderModule shaderModule;
		 if (vkCreateShaderModule(device_, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
			 cerr << "Failed to create shader module!";
		 }
		 return shaderModule;
	 }

	 void createGraphicsPipeline_() {
		 auto vertShaderCode = readFile_("src/vert.spv");
		 auto fragShaderCode = readFile_("src/frag.spv");
		 VkShaderModule vertShaderModule = createShaderModule_(vertShaderCode);
		 VkShaderModule fragShaderModule = createShaderModule_(fragShaderCode);

		 VkPipelineShaderStageCreateInfo vertShaderStageInfo {};
		 vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		 vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		 vertShaderStageInfo.module = vertShaderModule;
		 vertShaderStageInfo.pName = "main";

		 VkPipelineShaderStageCreateInfo fragShaderStageInfo {};
		 fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		 fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		 fragShaderStageInfo.module = fragShaderModule;
		 fragShaderStageInfo.pName = "main";

		 VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

		 VkPipelineVertexInputStateCreateInfo vertexInputInfo {};
		 vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		 vertexInputInfo.vertexBindingDescriptionCount = 0;
		 vertexInputInfo.vertexAttributeDescriptionCount = 0;

		 VkPipelineInputAssemblyStateCreateInfo inputAssembly {};
		 inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		 inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		 inputAssembly.primitiveRestartEnable = VK_FALSE;

		 VkExtent2D swapChainExtent = swapChainExtent_;

		 VkViewport viewport {};
		 viewport.x = 0.0f;
		 viewport.y = 0.0f;
		 viewport.width = (float)swapChainExtent.width;
		 viewport.height = (float)swapChainExtent.height;
		 viewport.minDepth = 0.0f;
		 viewport.maxDepth = 1.0f;

		 VkRect2D scissor {};
		 scissor.offset = {0, 0};
		 scissor.extent = swapChainExtent;

		 VkPipelineViewportStateCreateInfo viewportState {};
		 viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		 viewportState.viewportCount = 1;
		 viewportState.pViewports = &viewport;
		 viewportState.scissorCount = 1;
		 viewportState.pScissors = &scissor;

		 VkPipelineRasterizationStateCreateInfo rasterizer {};
		 rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		 rasterizer.depthClampEnable = VK_FALSE;
		 rasterizer.rasterizerDiscardEnable = VK_FALSE;
		 rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		 rasterizer.lineWidth = 1.0f;
		 rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		 rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		 rasterizer.depthBiasEnable = VK_FALSE;

		 VkPipelineMultisampleStateCreateInfo multisampling {};
		 multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		 multisampling.sampleShadingEnable = VK_FALSE;
		 multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		 VkPipelineColorBlendAttachmentState colorBlendAttachment {};
		 colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		 colorBlendAttachment.blendEnable = VK_FALSE;

		 VkPipelineColorBlendStateCreateInfo colorBlending {};
		 colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		 colorBlending.logicOpEnable = VK_FALSE;
		 colorBlending.attachmentCount = 1;
		 colorBlending.pAttachments = &colorBlendAttachment;

		 VkPipelineLayoutCreateInfo pipelineLayoutInfo {};
		 pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		 pipelineLayoutInfo.setLayoutCount = 1;
		 pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout_;
		 pipelineLayoutInfo.pushConstantRangeCount = 0;

		 if (vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
			 cerr << "Failed to create pipeline layout!";
		 }

		 VkGraphicsPipelineCreateInfo pipelineInfo {};
		 pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		 pipelineInfo.stageCount = 2;
		 pipelineInfo.pStages = shaderStages;
		 pipelineInfo.pVertexInputState = &vertexInputInfo;
		 pipelineInfo.pInputAssemblyState = &inputAssembly;
		 pipelineInfo.pViewportState = &viewportState;
		 pipelineInfo.pRasterizationState = &rasterizer;
		 pipelineInfo.pMultisampleState = &multisampling;
		 pipelineInfo.pColorBlendState = &colorBlending;
		 pipelineInfo.layout = pipelineLayout_;
		 pipelineInfo.renderPass = renderPass_;
		 pipelineInfo.subpass = 0;
		 pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

		 if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline_) != VK_SUCCESS) {
			 cerr << "Failed to create graphics pipeline!";
		 }

		 vkDestroyShaderModule(device_, vertShaderModule, nullptr);
		 vkDestroyShaderModule(device_, fragShaderModule, nullptr);
	 }

	 void createFramebuffers_() {
		 swapChainFramebuffers_.resize(swapChainImageViews_.size());

		 for (size_t i = 0; i < swapChainImageViews_.size(); i++) {
			 VkImageView attachments[] = {
				 swapChainImageViews_[i]
			 };

			 VkFramebufferCreateInfo framebufferInfo {};
			 framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			 framebufferInfo.renderPass = renderPass_;
			 framebufferInfo.attachmentCount = 1;
			 framebufferInfo.pAttachments = attachments;

			 VkExtent2D swapChainExtent = swapChainExtent_;
			 framebufferInfo.width = swapChainExtent.width;
			 framebufferInfo.height = swapChainExtent.height;
			 framebufferInfo.layers = 1;

			 if (vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &swapChainFramebuffers_[i]) != VK_SUCCESS) {
				 cerr << "Failed to create framebuffer!";
			 }
		}
	 }

	void createCommandPool_() {
		 VkCommandPoolCreateInfo poolInfo {};
		 poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		 poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		 poolInfo.queueFamilyIndex = graphicsFamily_;

		 if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) != VK_SUCCESS) {
			 cerr << "Failed to create command pool!";
		 }
	 }

	 void createCommandBuffer_() {
		 VkCommandBufferAllocateInfo allocInfo {};
		 allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		 allocInfo.commandPool = commandPool_;
		 allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		 allocInfo.commandBufferCount = 1;

		 if (vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer_) != VK_SUCCESS) {
			 cerr << "Failed to allocate command buffers!";
		 }
	 }

	 uint32_t findMemoryType_(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	         VkPhysicalDeviceMemoryProperties memProperties;
	         vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProperties);

	         for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
	             if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
	                 return i;
	        }
	    }
	    cerr << "Failed to find suitable memory type!" << endl;
	    return 0;
	 }

	 void createBuffer_(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
	         VkBufferCreateInfo bufferInfo {};
	         bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	         bufferInfo.size = size;
	         bufferInfo.usage = usage;
	         bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	         if (vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
	             cerr << "Failed to create buffer!" << endl;
	         }

	         VkMemoryRequirements memRequirements;
	         vkGetBufferMemoryRequirements(device_, buffer, &memRequirements);

	         VkMemoryAllocateInfo allocInfo {};
	         allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	         allocInfo.allocationSize = memRequirements.size;
	         allocInfo.memoryTypeIndex = findMemoryType_(memRequirements.memoryTypeBits, properties);

	         if (vkAllocateMemory(device_, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
	             cerr << "Failed to allocate buffer memory!" << endl;
	         }

	         vkBindBufferMemory(device_, buffer, bufferMemory, 0);
	     }

	 void createImage_(const char* fileName) {
	 	    int texWidth = 0, texHeight = 0, texChannels = 0;
	 	    stbi_uc* pixels = stbi_load(fileName, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	 	    if (!pixels) {
	 	        throw runtime_error("Failed to load texture image file! (looking for texture.png in the working directory)");
	 	    }

	 	    VkDeviceSize imageSize = static_cast<VkDeviceSize>(texWidth) * texHeight * 4;

	 	    VkBuffer stagingBuffer;
	 	    VkDeviceMemory stagingBufferMemory;
	 	    createBuffer_(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	 	                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	 	                  stagingBuffer, stagingBufferMemory);

	 	    void* data;
	 	    vkMapMemory(device_, stagingBufferMemory, 0, imageSize, 0, &data);
	 	    memcpy(data, pixels, static_cast<size_t>(imageSize));
	 	    vkUnmapMemory(device_, stagingBufferMemory);

	 	    stbi_image_free(pixels);

	 	    VkImageCreateInfo imageInfo {};
	 	    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	 	    imageInfo.imageType = VK_IMAGE_TYPE_2D;
	 	    imageInfo.extent.width = static_cast<uint32_t>(texWidth);
	 	    imageInfo.extent.height = static_cast<uint32_t>(texHeight);
	 	    imageInfo.extent.depth = 1;
	 	    imageInfo.mipLevels = 1;
	 	    imageInfo.arrayLayers = 1;
	 	    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	 	    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	 	    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	 	    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	 	    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	 	    if (vkCreateImage(device_, &imageInfo, nullptr, &image_) != VK_SUCCESS) {
	 	        cerr << "Failed to create texture image!" << endl;
	 	    }

	 	    VkMemoryRequirements memRequirements;
	 	    vkGetImageMemoryRequirements(device_, image_, &memRequirements);

	 	    VkMemoryAllocateInfo allocInfo {};
	 	    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	 	    allocInfo.allocationSize = memRequirements.size;
	 	    allocInfo.memoryTypeIndex = findMemoryType_(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	 	    if (vkAllocateMemory(device_, &allocInfo, nullptr, &imageMemory_) != VK_SUCCESS) {
	 	        cerr << "Failed to allocate texture image memory!" << endl;
	 	    }

	 	    vkBindImageMemory(device_, image_, imageMemory_, 0);

	 	    transitionImageLayout_(image_, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	 	    copyBufferToImage_(stagingBuffer, image_, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
	 	    transitionImageLayout_(image_, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	 	    vkDestroyBuffer(device_, stagingBuffer, nullptr);
	 	    vkFreeMemory(device_, stagingBufferMemory, nullptr);
	 	}

	 VkCommandBuffer beginSingleTimeCommands_() {
	         VkCommandBufferAllocateInfo allocInfo {};
	         allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	         allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	         allocInfo.commandPool = commandPool_;
	         allocInfo.commandBufferCount = 1;

	         VkCommandBuffer commandBuffer;
	         vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer);

	         VkCommandBufferBeginInfo beginInfo {};
	         beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	         beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	         vkBeginCommandBuffer(commandBuffer, &beginInfo);
	         return commandBuffer;
	     }

	     void endSingleTimeCommands_(VkCommandBuffer commandBuffer) {
	         vkEndCommandBuffer(commandBuffer);

	         VkSubmitInfo submitInfo {};
	         submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	         submitInfo.commandBufferCount = 1;
	         submitInfo.pCommandBuffers = &commandBuffer;

	         vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
	         vkQueueWaitIdle(graphicsQueue_);

	         vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
	     }

	     void transitionImageLayout_(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
	         VkCommandBuffer commandBuffer = beginSingleTimeCommands_();

	         VkImageMemoryBarrier barrier {};
	         barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	         barrier.oldLayout = oldLayout;
	         barrier.newLayout = newLayout;
	         barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	         barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	         barrier.image = image;
	         barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	         barrier.subresourceRange.baseMipLevel = 0;
	         barrier.subresourceRange.levelCount = 1;
	         barrier.subresourceRange.baseArrayLayer = 0;
	         barrier.subresourceRange.layerCount = 1;

	         VkPipelineStageFlags sourceStage;
	         VkPipelineStageFlags destinationStage;

	         if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
	             barrier.srcAccessMask = 0;
	             barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	             sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	             destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	         } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
	             barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	             barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	             sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	             destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	         } else {
	             cerr << "Unsupported layout transition!" << endl;
	             return;
	         }

	         vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	         endSingleTimeCommands_(commandBuffer);
	     }

	     void copyBufferToImage_(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
	         VkCommandBuffer commandBuffer = beginSingleTimeCommands_();

	         VkBufferImageCopy region {};
	         region.bufferOffset = 0;
	         region.bufferRowLength = 0;
	         region.bufferImageHeight = 0;
	         region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	         region.imageSubresource.mipLevel = 0;
	         region.imageSubresource.baseArrayLayer = 0;
	         region.imageSubresource.layerCount = 1;
	         region.imageOffset = {0, 0, 0};
	         region.imageExtent = {width, height, 1};

	         vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	         endSingleTimeCommands_(commandBuffer);
	     }

	     void createImageView_() {
	             VkImageViewCreateInfo viewInfo {};
	             viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	             viewInfo.image = image_;
	             viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	             viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	             viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	             viewInfo.subresourceRange.baseMipLevel = 0;
	             viewInfo.subresourceRange.levelCount = 1;
	             viewInfo.subresourceRange.baseArrayLayer = 0;
	             viewInfo.subresourceRange.layerCount = 1;

	             if (vkCreateImageView(device_, &viewInfo, nullptr, &imageView_) != VK_SUCCESS) {
	                 cerr << "Failed to create texture image view!" << endl;
	             }
	         }

	         void createSampler_() {
	             VkSamplerCreateInfo samplerInfo {};
	             samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	             samplerInfo.magFilter = VK_FILTER_LINEAR;
	             samplerInfo.minFilter = VK_FILTER_LINEAR;
	             samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	             samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	             samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	             samplerInfo.anisotropyEnable = VK_FALSE;
	             samplerInfo.maxAnisotropy = 1.0f;
	             samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	             samplerInfo.unnormalizedCoordinates = VK_FALSE;
	             samplerInfo.compareEnable = VK_FALSE;
	             samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	             samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

	             if (vkCreateSampler(device_, &samplerInfo, nullptr, &sampler_) != VK_SUCCESS) {
	                 cerr << "Failed to create texture sampler!" << endl;
	             }
	         }

	 void createDescriptorSetLayout_() {
		 VkDescriptorSetLayoutBinding samplerBinding {};
		 samplerBinding.binding = 0;
		 samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		 samplerBinding.descriptorCount = 1;
		 samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		 VkDescriptorSetLayoutCreateInfo layoutInfo {};
		 layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		 layoutInfo.bindingCount = 1;
		 layoutInfo.pBindings = &samplerBinding;

		 if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &descriptorSetLayout_) != VK_SUCCESS) {
			 cerr << "Failed to create descriptor set layout!" << endl;
		 }
	 }

	 void createDescriptorPool_() {
		 VkDescriptorPoolSize poolSize {};
		 poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		 poolSize.descriptorCount = 1;

		 VkDescriptorPoolCreateInfo poolInfo {};
		 poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		 poolInfo.poolSizeCount = 1;
		 poolInfo.pPoolSizes = &poolSize;
		 poolInfo.maxSets = 1;

		 if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_) != VK_SUCCESS) {
			 cerr << "Failed to create descriptor pool!" << endl;
		 }
	 }

	 void createDescriptorSet_() {
		 VkDescriptorSetAllocateInfo allocInfo {};
		 allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		 allocInfo.descriptorPool = descriptorPool_;
		 allocInfo.descriptorSetCount = 1;
		 allocInfo.pSetLayouts = &descriptorSetLayout_;

		 if (vkAllocateDescriptorSets(device_, &allocInfo, &descriptorSet_) != VK_SUCCESS) {
			 cerr << "Failed to allocate descriptor set!" << endl;
		 }

		 VkDescriptorImageInfo imageInfo {};
		 imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		 imageInfo.imageView = imageView_;
		 imageInfo.sampler = sampler_;

		 VkWriteDescriptorSet descriptorWrite {};
		 descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		 descriptorWrite.dstSet = descriptorSet_;
		 descriptorWrite.dstBinding = 0;
		 descriptorWrite.dstArrayElement = 0;
		 descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		 descriptorWrite.descriptorCount = 1;
		 descriptorWrite.pImageInfo = &imageInfo;

		 vkUpdateDescriptorSets(device_, 1, &descriptorWrite, 0, nullptr);
	 }

	 void createSyncObjects_() {
		 VkSemaphoreCreateInfo semaphoreInfo {};
		 semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		 VkFenceCreateInfo fenceInfo {};
		 fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		 fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		 if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAvailableSemaphore_) != VK_SUCCESS ||
			 vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFence_) != VK_SUCCESS) {
			 cerr << "Failed to create sync objects!" << endl;
		 }

		 renderFinishedSemaphores_.resize(swapChainImageViews_.size());
		 for (auto& semaphore : renderFinishedSemaphores_) {
			 if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS) {
				 cerr << "Failed to create render-finished semaphore!" << endl;
			 }
		 }
	 }

	 void recordCommandBuffer_(uint32_t imageIndex) {
		 VkCommandBufferBeginInfo beginInfo {};
		 beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		 vkBeginCommandBuffer(commandBuffer_, &beginInfo);

		 VkRenderPassBeginInfo renderPassInfo {};
		 renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		 renderPassInfo.renderPass = renderPass_;
		 renderPassInfo.framebuffer = swapChainFramebuffers_[imageIndex];
		 renderPassInfo.renderArea.offset = {0, 0};
		 renderPassInfo.renderArea.extent = swapChainExtent_;

		 VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
		 renderPassInfo.clearValueCount = 1;
		 renderPassInfo.pClearValues = &clearColor;

		 vkCmdBeginRenderPass(commandBuffer_, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		 vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline_);
		 vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &descriptorSet_, 0, nullptr);
		 vkCmdDraw(commandBuffer_, 6, 1, 0, 0);
		 vkCmdEndRenderPass(commandBuffer_);

		 if (vkEndCommandBuffer(commandBuffer_) != VK_SUCCESS) {
			 cerr << "Failed to record command buffer!" << endl;
		 }
	 }

	 void drawFrame_() {
		 vkWaitForFences(device_, 1, &inFlightFence_, VK_TRUE, UINT64_MAX);

		 uint32_t imageIndex;
		 VkResult result = vkAcquireNextImageKHR(device_, swapChain_, UINT64_MAX, imageAvailableSemaphore_, VK_NULL_HANDLE, &imageIndex);
		 if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			 cerr << "Failed to acquire swap chain image!" << endl;
			 return;
		 }
		 vkResetFences(device_, 1, &inFlightFence_);

		 vkResetCommandBuffer(commandBuffer_, 0);
		 recordCommandBuffer_(imageIndex);

		 VkSemaphore waitSemaphores[] = { imageAvailableSemaphore_ };
		 VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		 VkSemaphore signalSemaphores[] = { renderFinishedSemaphores_[imageIndex] };

		 VkSubmitInfo submitInfo {};
		 submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		 submitInfo.waitSemaphoreCount = 1;
		 submitInfo.pWaitSemaphores = waitSemaphores;
		 submitInfo.pWaitDstStageMask = waitStages;
		 submitInfo.commandBufferCount = 1;
		 submitInfo.pCommandBuffers = &commandBuffer_;
		 submitInfo.signalSemaphoreCount = 1;
		 submitInfo.pSignalSemaphores = signalSemaphores;

		 if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFence_) != VK_SUCCESS) {
			 cerr << "Failed to submit draw command buffer!" << endl;
		 }

		 VkPresentInfoKHR presentInfo {};
		 presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		 presentInfo.waitSemaphoreCount = 1;
		 presentInfo.pWaitSemaphores = signalSemaphores;
		 presentInfo.swapchainCount = 1;
		 presentInfo.pSwapchains = &swapChain_;
		 presentInfo.pImageIndices = &imageIndex;

		 vkQueuePresentKHR(presentQueue_, &presentInfo);
	 }

	 void mainLoop_() {
		 while (!glfwWindowShouldClose(window_)) {
			 glfwPollEvents();
			 drawFrame_();
		 }
		 vkDeviceWaitIdle(device_);
	 }
};

int main() {
	try {
		Application app;
		app.createWindow(800, 600, "2d Image Loader");
		imageName = "image.png"; // set image file here
		app.run();
	} catch (const exception& e) {
		cerr << e.what() << endl;
	}
}
