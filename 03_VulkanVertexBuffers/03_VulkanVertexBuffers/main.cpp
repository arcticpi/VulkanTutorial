#define NOMINMAX

#define GLFW_INCLUDE_VULKAN
#define VK_USE_PLATFORM_WIN32_KHR
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>

#include <string>
#include <vector>
#include <optional>
#include <set>
#include <algorithm>
#include <fstream>
#include <array>

#include <glm/glm.hpp>

#ifdef NDEBUG
const bool EnableValidationLayer = false;;
#else
const bool EnableValidationLayer = true;
#endif

struct QueueFamilyIndex
{
	std::optional<uint32_t> graphic;
	std::optional<uint32_t> present;

	bool IsComplete()
	{
		return graphic.has_value() && present.has_value();
	}
};

struct SwapChainSupport
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> modes;
};

struct Vertex
{
	glm::vec2 position;
	glm::vec3 color;

	static VkVertexInputBindingDescription GetBindingDescription()
	{
		VkVertexInputBindingDescription BindingDescription = {
			0,							// binding
			sizeof(Vertex),				// stride
			VK_VERTEX_INPUT_RATE_VERTEX	// inputRate
		};

		return BindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions()
	{
		VkVertexInputAttributeDescription PositionDescription = {
			0,							// location
			0,							// binding
			VK_FORMAT_R32G32_SFLOAT,	// format
			offsetof(Vertex, position)	// offset
		};

		VkVertexInputAttributeDescription ColorDescription = {
			1,							// location
			0,							// binding
			VK_FORMAT_R32G32B32_SFLOAT,	// format
			offsetof(Vertex, color)		// offset
		};

		std::array<VkVertexInputAttributeDescription, 2> AttributeDescriptions = { PositionDescription, ColorDescription };

		return AttributeDescriptions;
	}
};

class VulkanApplication
{
public:
	void run()
	{
		InitWindow();
		InitVulkan();
		MainLoop();
		cleanup();
	}

private:
	// glfw
	GLFWwindow* window;
	const int WindowWidth = 800;
	const int WindowHeight = 600;
	const char* WindowTitle = "Vulkan Triangle";

	void InitWindow()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // we don't need to create an OpenGL context
		// glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(WindowWidth, WindowHeight, WindowTitle, nullptr, nullptr);

		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, FramebufferResizeCallback);
	}

	static void FramebufferResizeCallback(GLFWwindow* window, int width, int height)
	{
		auto app = reinterpret_cast<VulkanApplication*>(glfwGetWindowUserPointer(window));
		app->FramebufferResized = true;
	}

	// vulkan
	const std::vector<const char*> layers = { "VK_LAYER_LUNARG_standard_validation" };
	VkInstance instance;
	VkSurfaceKHR surface;
	VkDebugUtilsMessengerEXT messenger;
	VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
	VkPhysicalDeviceFeatures features = {};

	const std::vector<const char*> extentions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	VkDevice device;
	VkQueue GraphicQueue;
	VkQueue PresentQueue;

	VkSwapchainKHR SwapChain;
	std::vector<VkImage> SwapChainImages;
	VkFormat SwapChainFormat;
	VkExtent2D SwapChainExtent;
	std::vector<VkImageView> SwapChainImageViews;

	VkRenderPass RenderPass;
	VkPipelineLayout PipelineLayout;
	VkPipeline GraphicsPipeline;

	std::vector<VkFramebuffer> SwapChainFramebuffers;

	VkCommandPool CommandPool;
	std::vector<VkCommandBuffer> CommandBuffers;

	const int MAX_FRAMES_IN_FLIGHT = 2;
	size_t CurrentFrame = 0;
	std::vector<VkSemaphore> ImageAvailableSemaphore;
	std::vector<VkSemaphore> RenderFinishedSemaphore;
	std::vector<VkFence> fences;

	bool FramebufferResized = false;

	const std::vector<Vertex> vertices = {
		{ {-0.5f, -0.5f}, {1.0f, 0.5f, 0.0f} },
		{ {-0.5f, +0.5f}, {1.0f, 0.0f, 1.0f} },
		{ {+0.5f, -0.5f}, {1.0f, 1.0f, 0.0f} },
		{ {+0.5f, +0.5f}, {0.0f, 1.0f, 1.0f} }
	};

	const std::vector<uint16_t> indices = { 0, 2, 1, 2, 3, 1 };

	VkBuffer VertexBuffer;
	VkDeviceMemory VertexBufferMemory;
	VkBuffer IndexBuffer;
	VkDeviceMemory IndexBufferMemory;

	void InitVulkan()
	{
		CreateInstance();
		SetupDebugMessenger();
		CreateSurface();
		PickPhysicalDevice();
		CreateLogicalDevice();
		CreateSwapchain();
		CreateImageViews();
		CreateRenderPass();
		CreateGraphicsPipeline();
		CreateFramebuffers();
		CreateCommandPool();
		CreateVertexBuffer();
		CreateIndexBuffer();
		CreateCommandBuffers();
		CreateSemaphoresAndFences();
	}

	void DisplayAvailableLayers()
	{
		uint32_t count = 0;
		vkEnumerateInstanceLayerProperties(&count, nullptr);

		std::vector<VkLayerProperties> properties(count);
		vkEnumerateInstanceLayerProperties(&count, properties.data());

		if (count != 0)
		{
			std::cout << "--- Instance Layer Properties ---" << std::endl;

			for (VkLayerProperties property : properties)
			{
				std::cout << property.layerName << std::endl;
			}
		}
	}

	void DisplayAvailableExtensions()
	{
		uint32_t count = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);

		std::vector<VkExtensionProperties> properties(count);
		vkEnumerateInstanceExtensionProperties(nullptr, &count, properties.data());

		if (count != 0)
		{
			std::cout << "--- Instance Extension Properties ---" << std::endl;

			for (VkExtensionProperties property : properties)
			{
				std::cout << property.extensionName << std::endl;
			}
		}
	}

	void CreateInstance()
	{
		VkResult result;

		// glfw extensions
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensionNames = glfwGetRequiredInstanceExtensions(&glfwExtensionCount); // VK_KHR_surface, VK_KHR_win32_surface

		std::vector<const char*> extensions(glfwExtensionNames, glfwExtensionNames + glfwExtensionCount);

		if (EnableValidationLayer)
		{
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		VkInstanceCreateInfo InstanceCreateInfo =
		{
			VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,				// sType
			nullptr,											// pNext
			0,													// flags
			nullptr,											// pApplicationInfo
			EnableValidationLayer ? layers.size() : 0,			// enabledLayerCount
			EnableValidationLayer ? layers.data() : nullptr,	// ppEnabledLayerNames
			static_cast<uint32_t>(extensions.size()),			// enabledExtensionCount
			extensions.data()									// ppEnabledExtensionNames
		};

		result = vkCreateInstance(&InstanceCreateInfo, nullptr, &instance);

		if (result != VK_SUCCESS)
		{
			std::string message;

			switch (result)
			{
			case VK_ERROR_EXTENSION_NOT_PRESENT:
				message = "VK_ERROR_EXTENSION_NOT_PRESENT";
				break;
			case VK_ERROR_LAYER_NOT_PRESENT:
				message = "VK_ERROR_LAYER_NOT_PRESENT";
				break;
			default:
				message = "failed to create instance";
				break;
			}

			throw std::runtime_error(message);
		}
	}

	VkResult vkCreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger)
	{
		PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		// PFN_vkCreateDebugUtilsMessengerEXT func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT> (vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));

		if (func != nullptr)
		{
			return func(instance, pCreateInfo, pAllocator, pMessenger);
		}
		else
		{
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	void vkDestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT pMessenger, const VkAllocationCallbacks* pAllocator)
	{
		PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

		if (func != nullptr)
		{
			func(instance, pMessenger, pAllocator);
		}
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
	{
		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
		return VK_FALSE;
	}

	void SetupDebugMessenger()
	{
		VkResult result;

		if (EnableValidationLayer)
		{
			VkDebugUtilsMessageSeverityFlagsEXT severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			// VkDebugUtilsMessageSeverityFlagsEXT severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			VkDebugUtilsMessageTypeFlagsEXT type = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

			VkDebugUtilsMessengerCreateInfoEXT DebugUtilsMessengerCreateInfo = {
				VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,	// sType
				nullptr,													// pNext
				0,															// flags
				severity,													// messageSeverity
				type,														// messageType
				DebugCallback,												// pfnUserCallback
				nullptr														// pUserData
			};

			result = vkCreateDebugUtilsMessengerEXT(instance, &DebugUtilsMessengerCreateInfo, nullptr, &messenger);

			if (result != VK_SUCCESS)
			{
				throw std::runtime_error("failed to set up debug messenger");
			}
		}
	}

	void PickPhysicalDevice()
	{
		uint32_t count = 0;
		vkEnumeratePhysicalDevices(instance, &count, nullptr);

		if (count != 0)
		{
			std::vector<VkPhysicalDevice> devices(count);
			vkEnumeratePhysicalDevices(instance, &count, devices.data());

			for (VkPhysicalDevice device : devices)
			{
				if (IsDeviceSuitable(device))
				{
					this->PhysicalDevice = device;
					break;
				}
			}

			if (this->PhysicalDevice == VK_NULL_HANDLE)
			{
				throw std::runtime_error("failed to find a suitable device");
			}
		}
		else
		{
			throw std::runtime_error("failed to find any physical device");
		}
	}

	bool IsDeviceSuitable(VkPhysicalDevice device)
	{
		QueueFamilyIndex index = FindQueueFamilyIndex(device);

		bool ExtensionSupport = CheckDeviceExtensionSupport(device);
		bool SwapChainAdequate = false;

		if (ExtensionSupport)
		{
			SwapChainSupport support = QuerySwapChainSupport(device);
			SwapChainAdequate = !support.formats.empty() && !support.modes.empty();
		}

		return index.IsComplete() && ExtensionSupport&& SwapChainAdequate;
	}

	QueueFamilyIndex FindQueueFamilyIndex(VkPhysicalDevice device)
	{
		QueueFamilyIndex index;

		uint32_t count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);

		if (count != 0)
		{
			std::vector<VkQueueFamilyProperties> families(count);
			vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

			for (uint32_t i = 0; i < count; i++)
			{
				const VkQueueFamilyProperties& family = families[i];

				if ((family.queueCount > 0) && (family.queueFlags & VK_QUEUE_GRAPHICS_BIT))
				{
					index.graphic = i;
				}

				VkBool32 flag = false;
				vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &flag);

				if ((family.queueCount > 0) && flag)
				{
					index.present = i;
				}

				if (index.IsComplete())
				{
					break;
				}
			}
		}

		return index;
	}

	bool CheckDeviceExtensionSupport(VkPhysicalDevice device)
	{
		uint32_t count;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);

		std::vector<VkExtensionProperties> available(count);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());

		std::set<std::string> required(extentions.begin(), extentions.end());

		for (const VkExtensionProperties& extension : available)
		{
			required.erase(extension.extensionName);
		}

		return required.empty();
	}

	void CreateLogicalDevice()
	{
		VkResult result;

		QueueFamilyIndex index = FindQueueFamilyIndex(PhysicalDevice);
		std::set<uint32_t> families = { index.graphic.value(), index.present.value() };

		std::vector<VkDeviceQueueCreateInfo> QueueCreateInfos;
		float QueuePriority = 1.0f;

		for (uint32_t family : families)
		{
			VkDeviceQueueCreateInfo QueueCreateInfo = {
				VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,	// sType
				nullptr,									// pNext
				0,											// flags
				family,										// queueFamilyIndex
				1,											// queueCount
				&QueuePriority								// pQueuePriorities
			};

			QueueCreateInfos.push_back(QueueCreateInfo);
		}

		VkDeviceCreateInfo DeviceCreateInfo = {
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,				// sType
			nullptr,											// pNext
			0,													// flags
			QueueCreateInfos.size(),							// queueCreateInfoCount
			QueueCreateInfos.data(),							// pQueueCreateInfos
			EnableValidationLayer ? layers.size() : 0,			// enabledLayerCount
			EnableValidationLayer ? layers.data() : nullptr,	// ppEnabledLayerNames
			extentions.size(),									// enabledExtensionCount
			extentions.data(),									// ppEnabledExtensionNames
			&features											// pEnabledFeatures
		};

		result = vkCreateDevice(PhysicalDevice, &DeviceCreateInfo, nullptr, &device);

		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create logical device");
		}

		vkGetDeviceQueue(device, index.graphic.value(), 0, &GraphicQueue);
		vkGetDeviceQueue(device, index.present.value(), 0, &PresentQueue);
	}

	void CreateSurface()
	{
		VkResult result;

		/*
		// Windows platform specific surface creation

		VkWin32SurfaceCreateInfoKHR SurfaceCreateInfo = {
			VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,	// sType
			nullptr,											// pNext
			0,													// flags
			GetModuleHandle(nullptr),							// hinstance
			glfwGetWin32Window(window)							// hwnd
		};

		result = vkCreateWin32SurfaceKHR(instance, &SurfaceCreateInfo, nullptr, &surface);
		*/

		// platform agnostic surface creation

		result = glfwCreateWindowSurface(instance, window, nullptr, &surface);

		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create window surface");
		}
	}

	SwapChainSupport QuerySwapChainSupport(VkPhysicalDevice device)
	{
		SwapChainSupport support;

		// capabilities
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &support.capabilities);

		// formats
		{
			uint32_t count;
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, nullptr);

			if (count != 0)
			{
				support.formats.resize(count);
				vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, support.formats.data());
			}
		}

		// present modes
		{
			uint32_t count;
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, nullptr);

			if (count != 0)
			{
				support.modes.resize(count);
				vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, support.modes.data());
			}
		}

		return support;
	}

	VkSurfaceFormatKHR ChooseSwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
	{
		if (formats.size() == 1 && formats.begin()->format == VK_FORMAT_UNDEFINED)
		{
			return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
		}

		for (VkSurfaceFormatKHR format : formats)
		{
			if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
			{
				return format;
			}
		}

		return *formats.begin();
	}

	VkPresentModeKHR ChooseSwapChainPresentMode(const std::vector<VkPresentModeKHR>& modes)
	{
		VkPresentModeKHR available = VK_PRESENT_MODE_FIFO_KHR;

		for (VkPresentModeKHR mode : modes)
		{
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				return mode;
			}

			if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
			{
				available = mode;
			}
		}

		return available;
	}

	VkExtent2D ChooseSwapChainExtent(const VkSurfaceCapabilitiesKHR& capabilities)
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}
		else
		{
			int width;
			int height;

			glfwGetFramebufferSize(window, &width, &height);
			VkExtent2D extent = { width, height };

			VkExtent2D min = capabilities.minImageExtent;
			VkExtent2D max = capabilities.maxImageExtent;

			extent.width = std::max(min.width, std::min(max.width, extent.width));
			extent.height = std::max(min.height, std::min(max.height, extent.height));

			return extent;
		}
	}

	void CreateSwapchain()
	{
		SwapChainSupport support = QuerySwapChainSupport(PhysicalDevice);
		VkSurfaceFormatKHR format = ChooseSwapChainSurfaceFormat(support.formats);
		VkPresentModeKHR mode = ChooseSwapChainPresentMode(support.modes);
		VkExtent2D extent = ChooseSwapChainExtent(support.capabilities);

		uint32_t count = support.capabilities.minImageCount + 1;

		if (support.capabilities.maxImageCount > 0 && count > support.capabilities.maxImageCount)
		{
			count = support.capabilities.maxImageCount;
		}

		QueueFamilyIndex index = FindQueueFamilyIndex(PhysicalDevice);
		bool flag = index.graphic == index.present;
		std::vector<uint32_t> indices = { index.graphic.value(), index.present.value() };

		VkSwapchainCreateInfoKHR SwapChainCreateInfo = {
			VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,					// sType
			nullptr,														// pNext
			0,																// flags
			surface,														// surface
			count,															// minImageCount
			format.format,													// imageFormat
			format.colorSpace,												// imageColorSpace
			extent,															// imageExtent
			1,																// imageArrayLayers
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,							// imageUsage
			flag ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,	// imageSharingMode
			flag ? indices.size() : 0,										// queueFamilyIndexCount
			flag ? indices.data() : nullptr,								// pQueueFamilyIndices
			support.capabilities.currentTransform,							// preTransform
			VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,								// compositeAlpha
			mode,															// presentMode
			VK_TRUE,														// clipped
			VK_NULL_HANDLE													// oldSwapchain
		};

		VkResult result = vkCreateSwapchainKHR(device, &SwapChainCreateInfo, nullptr, &SwapChain);

		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create swap chain");
		}

		uint32_t ImageCount;
		vkGetSwapchainImagesKHR(device, SwapChain, &ImageCount, nullptr);
		SwapChainImages.resize(ImageCount);
		vkGetSwapchainImagesKHR(device, SwapChain, &ImageCount, SwapChainImages.data());

		SwapChainFormat = format.format;
		SwapChainExtent = extent;
	}

	void RecreateSwapchain()
	{
		int width = 0;
		int height = 0;

		while (width == 0 || height == 0)
		{
			glfwGetFramebufferSize(window, &width, &height);
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(device);

		CleanupSwapchain();

		CreateSwapchain();
		CreateImageViews();
		CreateRenderPass();
		CreateGraphicsPipeline();
		CreateFramebuffers();
		CreateCommandBuffers();
	}

	void CreateImageViews()
	{
		// SwapChainImageViews.resize(SwapChainImages.size());
		SwapChainImageViews.clear();

		for (VkImage image : SwapChainImages)
		{
			VkComponentMapping components = {
				VK_COMPONENT_SWIZZLE_IDENTITY,	//	r
				VK_COMPONENT_SWIZZLE_IDENTITY,	//	g
				VK_COMPONENT_SWIZZLE_IDENTITY,	//	b
				VK_COMPONENT_SWIZZLE_IDENTITY	//	a
			};

			VkImageSubresourceRange SubresourceRange = {
				VK_IMAGE_ASPECT_COLOR_BIT,	//	aspectMask
				0,							//	baseMipLevel
				1,							//	levelCount
				0,							//	baseArrayLayer
				1							//	layerCount
			};

			VkImageViewCreateInfo ImageViewCreateInfo = {
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// sType
				nullptr,									// pNext
				0,											// flags
				image,										// image
				VK_IMAGE_VIEW_TYPE_2D,						// viewType
				SwapChainFormat,							// format
				components,									// components
				SubresourceRange							// subresourceRange
			};

			VkImageView view;
			VkResult result = vkCreateImageView(device, &ImageViewCreateInfo, nullptr, &view);

			if (result != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create image views");
			}

			SwapChainImageViews.push_back(view);
		}
	}

	static std::vector<char> ReadFile(const std::string& FileName)
	{
		std::ifstream file(FileName, std::ios::ate | std::ios::binary);

		if (!file.is_open())
		{
			throw std::runtime_error("failed to open file");
		}

		size_t size = file.tellg();
		std::vector<char> buffer(size);

		file.seekg(0);
		file.read(buffer.data(), size);

		file.close();

		return buffer;
	}

	VkShaderModule CreateShaderModule(const std::vector<char>& bytecode)
	{
		VkShaderModuleCreateInfo ShaderModuleCreateInfo = {
			VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,		// sType
			nullptr,											// pNext
			0,													// flags
			bytecode.size(),									// codeSize
			reinterpret_cast<const uint32_t*>(bytecode.data())	// pCode
		};

		VkShaderModule module;
		VkResult result = vkCreateShaderModule(device, &ShaderModuleCreateInfo, nullptr, &module);

		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create shader module");
		}

		// the buffer with the code can be freed immediately after creating the shader module

		return module;
	}

	void CreateRenderPass()
	{
		VkAttachmentDescription AttachmentDescription = {
			0,									// flags
			SwapChainFormat,					// format
			VK_SAMPLE_COUNT_1_BIT,				// samples
			VK_ATTACHMENT_LOAD_OP_CLEAR,		// loadOp
			VK_ATTACHMENT_STORE_OP_STORE,		// storeOp
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,	// stencilLoadOp
			VK_ATTACHMENT_STORE_OP_DONT_CARE,	// stencilStoreOp
			VK_IMAGE_LAYOUT_UNDEFINED,			// initialLayout
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR		// finalLayout
		};

		VkAttachmentReference AttachmentReference = {
			0,											// attachment
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL	// layout
		};

		VkSubpassDescription SubpassDescription = {
			0,									// flags
			VK_PIPELINE_BIND_POINT_GRAPHICS,	// pipelineBindPoint
			0,									// inputAttachmentCount
			nullptr,							// pInputAttachments
			1,									// colorAttachmentCount
			&AttachmentReference,				// pColorAttachments
			nullptr,							// pResolveAttachments
			nullptr,							// pDepthStencilAttachment
			0,									// preserveAttachmentCount
			nullptr								// pPreserveAttachments
		};

		VkAccessFlags flags = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkSubpassDependency dependecy = {
			VK_SUBPASS_EXTERNAL,							// srcSubpass
			0,												// dstSubpass
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,	// srcStageMask
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,	// dstStageMask
			0,												// srcAccessMask
			flags,											// dstAccessMask
			0												// dependencyFlags
		};

		VkRenderPassCreateInfo RenderPassCreateInfo = {
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// sType
			nullptr,									// pNext
			0,											// flags
			1,											// attachmentCount
			&AttachmentDescription,						// pAttachments
			1,											// subpassCount
			&SubpassDescription,						// pSubpasses
			1,											// dependencyCount
			&dependecy									// pDependencies
		};

		VkResult result = vkCreateRenderPass(device, &RenderPassCreateInfo, nullptr, &RenderPass);

		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create render pass");
		}
	}

	void CreateGraphicsPipeline()
	{
		VkResult result;

		std::vector<char> VertCode = ReadFile("shaders/vert.spv");
		std::vector<char> FragCode = ReadFile("shaders/frag.spv");

		VkShaderModule VertModule = CreateShaderModule(VertCode);
		VkShaderModule FragModule = CreateShaderModule(FragCode);

		// the buffer with the code can be freed immediately after creating the shader module

		VkPipelineShaderStageCreateInfo VertStageCreateInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// sType
			nullptr,												// pNext
			0,														// flags
			VK_SHADER_STAGE_VERTEX_BIT,								// stage
			VertModule,												// module
			"main",													// pName
			nullptr													// pSpecializationInfo
		};

		VkPipelineShaderStageCreateInfo FragStageCreateInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,	// sType
			nullptr,												// pNext
			0,														// flags
			VK_SHADER_STAGE_FRAGMENT_BIT,							// stage
			FragModule,												// module
			"main",													// pName
			nullptr													// pSpecializationInfo
		};

		std::vector<VkPipelineShaderStageCreateInfo> stages = { VertStageCreateInfo, FragStageCreateInfo };

		auto BindingDescription = Vertex::GetBindingDescription();
		auto AttributeDescriptions = Vertex::GetAttributeDescriptions();

		VkPipelineVertexInputStateCreateInfo VertexInputStateCreateInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// sType
			nullptr,													// pNext
			0,															// flags
			1,															// vertexBindingDescriptionCount
			&BindingDescription,										// pVertexBindingDescriptions
			AttributeDescriptions.size(),								// vertexAttributeDescriptionCount
			AttributeDescriptions.data()								// pVertexAttributeDescriptions
		};

		VkPipelineInputAssemblyStateCreateInfo InputAssemblyStateCreateInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// sType
			nullptr,														// pNext
			0,																// flags
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							// topology
			VK_FALSE														// primitiveRestartEnable
		};

		VkViewport viewport = {
			0.0f,					// x
			0.0f,					// y
			SwapChainExtent.width,	// width
			SwapChainExtent.height,	// height
			0.0f,					// minDepth
			1.0f					// maxDepth
		};

		VkRect2D scissor = {
			{0, 0},			// offset
			SwapChainExtent	// extent
		};

		VkPipelineViewportStateCreateInfo ViewportStateCreateInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,	// sType
			nullptr,												// pNext
			0,														// flags
			1,														// viewportCount
			&viewport,												// pViewports
			1,														// scissorCount
			&scissor												// pScissors
		};

		VkPipelineRasterizationStateCreateInfo RasterizationStateCreateInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,	// sType
			nullptr,													// pNext
			0,															// flags
			VK_FALSE,													// depthClampEnable
			VK_FALSE,													// rasterizerDiscardEnable
			VK_POLYGON_MODE_FILL,										// polygonMode
			VK_CULL_MODE_BACK_BIT,										// cullMode
			VK_FRONT_FACE_CLOCKWISE,									// frontFace
			VK_FALSE,													// depthBiasEnable
			0.0f,														// depthBiasConstantFactor
			0.0f,														// depthBiasClamp
			0.0f,														// depthBiasSlopeFactor
			1.0f														// lineWidth
		};

		VkPipelineMultisampleStateCreateInfo MultisampleStateCreateInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// sType
			nullptr,													// pNext
			0,															// flags
			VK_SAMPLE_COUNT_1_BIT,										// rasterizationSamples;
			VK_FALSE,													// sampleShadingEnable
			1.0f,														// minSampleShading
			nullptr,													// pSampleMask
			VK_FALSE,													// alphaToCoverageEnable
			VK_FALSE,													// alphaToOneEnable
		};

		// VkPipelineDepthStencilStateCreateInfo

		VkColorComponentFlags ColorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendAttachmentState ColorBlendAttachmentState = {
			VK_FALSE,				// blendEnable
			VK_BLEND_FACTOR_ONE,	// srcColorBlendFactor
			VK_BLEND_FACTOR_ZERO,	// dstColorBlendFactor
			VK_BLEND_OP_ADD,		// colorBlendOp
			VK_BLEND_FACTOR_ONE,	// srcAlphaBlendFactor
			VK_BLEND_FACTOR_ZERO,	// dstAlphaBlendFactor
			VK_BLEND_OP_ADD,		// alphaBlendOp
			ColorWriteMask			// colorWriteMask
		};

		// alpha blending : new color blended with old color based on its opacity
		/*
		VkPipelineColorBlendAttachmentState AlphaBlending = {
			VK_TRUE,								// blendEnable
			VK_BLEND_FACTOR_SRC_ALPHA,				// srcColorBlendFactor
			VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,	// dstColorBlendFactor
			VK_BLEND_OP_ADD,						// colorBlendOp
			VK_BLEND_FACTOR_ONE,					// srcAlphaBlendFactor
			VK_BLEND_FACTOR_ZERO,					// dstAlphaBlendFactor
			VK_BLEND_OP_ADD,						// alphaBlendOp
			ColorWriteMask							// colorWriteMask
		};
		*/

		VkPipelineColorBlendStateCreateInfo ColorBlendStateCreateInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// sType
			nullptr,													// pNext
			0,															// flags
			VK_FALSE,													// logicOpEnable
			VK_LOGIC_OP_COPY,											// logicOp
			1,															// attachmentCount
			&ColorBlendAttachmentState,									// pAttachments
			{0.0f, 0.0f, 0.0f, 0.0f}									// blendConstants
		};

		// VkDynamicState & VkPipelineDynamicStateCreateInfo

		VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,	// sType
			nullptr,										// pNext
			0,												// flags
			0,												// setLayoutCount
			nullptr,										// pSetLayouts
			0,												// pushConstantRangeCount
			nullptr											// pPushConstantRanges
		};

		result = vkCreatePipelineLayout(device, &PipelineLayoutCreateInfo, nullptr, &PipelineLayout);

		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create pipeline layout");
		}

		VkGraphicsPipelineCreateInfo GraphicsPipelineCreateInfo = {
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// sType
			nullptr,											// pNext
			0,													// flags
			2,													// stageCount
			stages.data(),										// pStages
			&VertexInputStateCreateInfo,						// pVertexInputState
			&InputAssemblyStateCreateInfo,						// pInputAssemblyState
			nullptr,											// pTessellationState
			&ViewportStateCreateInfo,							// pViewportState
			&RasterizationStateCreateInfo,						// pRasterizationState
			&MultisampleStateCreateInfo,						// pMultisampleState
			nullptr,											// pDepthStencilState
			&ColorBlendStateCreateInfo,							// pColorBlendState
			nullptr,											// pDynamicState
			PipelineLayout,										// layout
			RenderPass,											// renderPass
			0,													// subpass
			nullptr,											// basePipelineHandle
			-1													// basePipelineIndex
		};

		result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &GraphicsPipelineCreateInfo, nullptr, &GraphicsPipeline);

		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create graphics pipeline");
		}

		// bytecode compilation and linking happens during graphics pipeline creation
		// so we can destroy shader modules as soon as pipeline creation is finished

		vkDestroyShaderModule(device, VertModule, nullptr);
		vkDestroyShaderModule(device, FragModule, nullptr);
	}

	void CreateFramebuffers()
	{
		SwapChainFramebuffers.clear();

		for (VkImageView view : SwapChainImageViews)
		{
			VkFramebuffer framebuffer;
			std::vector<VkImageView> attachments = { view };

			VkFramebufferCreateInfo FramebufferCreateInfo = {
				VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// sType
				nullptr,									// pNext
				0,											// flags
				RenderPass,									// renderPass
				1,											// attachmentCount
				attachments.data(),							// pAttachments
				SwapChainExtent.width,						// width
				SwapChainExtent.height,						// height
				1											// layers
			};

			VkResult result = vkCreateFramebuffer(device, &FramebufferCreateInfo, nullptr, &framebuffer);

			if (result != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create framebuffer");
			}

			SwapChainFramebuffers.push_back(framebuffer);
		}
	}

	void CreateCommandPool()
	{
		QueueFamilyIndex index = FindQueueFamilyIndex(PhysicalDevice);

		VkCommandPoolCreateInfo CommandPoolCreateInfo = {
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,	// sType
			nullptr,									// pNext
			0,											// flags
			index.graphic.value()						// queueFamilyIndex
		};

		VkResult result = vkCreateCommandPool(device, &CommandPoolCreateInfo, nullptr, &CommandPool);

		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create command pool");
		}
	}

	void CreateVertexBuffer()
	{
		VkBuffer StagingBuffer;
		VkDeviceMemory StagingBufferMemory;
		VkDeviceSize size = sizeof(Vertex) * vertices.size();
		VkBufferUsageFlags usage;
		VkMemoryPropertyFlags properties;

		usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		CreateBuffer(size, usage, properties, StagingBuffer, StagingBufferMemory);

		void* data;
		vkMapMemory(device, StagingBufferMemory, 0, size, 0, &data);
		memcpy(data, vertices.data(), size);
		vkUnmapMemory(device, StagingBufferMemory);

		usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		CreateBuffer(size, usage, properties, VertexBuffer, VertexBufferMemory);

		CopyBuffer(StagingBuffer, VertexBuffer, size);

		vkDestroyBuffer(device, StagingBuffer, nullptr);
		vkFreeMemory(device, StagingBufferMemory, nullptr);
	}

	void CreateIndexBuffer()
	{
		VkBuffer StagingBuffer;
		VkDeviceMemory StagingBufferMemory;
		// VkDeviceSize size = sizeof(uint16_t) * indices.size();
		VkDeviceSize size = sizeof(indices.at(0)) * indices.size();
		VkBufferUsageFlags usage;
		VkMemoryPropertyFlags properties;

		usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		CreateBuffer(size, usage, properties, StagingBuffer, StagingBufferMemory);

		void* data;
		vkMapMemory(device, StagingBufferMemory, 0, size, 0, &data);
		memcpy(data, indices.data(), size);
		vkUnmapMemory(device, StagingBufferMemory);

		usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		CreateBuffer(size, usage, properties, IndexBuffer, IndexBufferMemory);

		CopyBuffer(StagingBuffer, IndexBuffer, size);

		vkDestroyBuffer(device, StagingBuffer, nullptr);
		vkFreeMemory(device, StagingBufferMemory, nullptr);
	}

	void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory)
	{
		VkResult result;

		VkBufferCreateInfo BufferCreateInfo = {
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,	// sType
			nullptr,								// pNext
			0,										// flags
			size,									// size
			usage,									// usage
			VK_SHARING_MODE_EXCLUSIVE,				// sharingMode
			0,										// queueFamilyIndexCount
			nullptr									// pQueueFamilyIndices
		};

		result = vkCreateBuffer(device, &BufferCreateInfo, nullptr, &buffer);

		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create buffer");
		}

		VkMemoryRequirements MemoryRequirements;
		vkGetBufferMemoryRequirements(device, buffer, &MemoryRequirements);

		uint32_t index = FindMemoryType(MemoryRequirements.memoryTypeBits, properties);

		VkMemoryAllocateInfo MemoryAllocateInfo = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,	// sType
			nullptr,								// pNext
			MemoryRequirements.size,				// allocationSize
			index									// memoryTypeIndex
		};

		result = vkAllocateMemory(device, &MemoryAllocateInfo, nullptr, &memory);

		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate buffer memory");
		}

		vkBindBufferMemory(device, buffer, memory, 0);
	}

	void CopyBuffer(VkBuffer SrcBuffer, VkBuffer DstBuffer, VkDeviceSize size)
	{
		VkResult result;
		VkCommandBuffer CommandBuffer;

		VkCommandBufferAllocateInfo CommandBufferAllocateInfo = {
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// sType
			nullptr,										// pNext
			CommandPool,									// commandPool
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// level
			1												// commandBufferCount
		};

		result = vkAllocateCommandBuffers(device, &CommandBufferAllocateInfo, &CommandBuffer);

		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate command buffers");
		}

		VkCommandBufferBeginInfo CommandBufferBeginInfo = {
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// sType
			nullptr,										// pNext
			VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,	// flags
			nullptr											// pInheritanceInfo
		};

		result = vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo);

		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to begin recording command buffer");
		}

		VkBufferCopy region = {
			0,		// srcOffset
			0,		// dstOffset
			size	// size
		};

		vkCmdCopyBuffer(CommandBuffer, SrcBuffer, DstBuffer, 1, &region);
		vkEndCommandBuffer(CommandBuffer);

		VkSubmitInfo SubmitInfo = {
			VK_STRUCTURE_TYPE_SUBMIT_INFO,	// sType
			nullptr,						// pNext
			0,								// waitSemaphoreCount
			nullptr,						// pWaitSemaphores
			nullptr,						// pWaitDstStageMask
			1,								// commandBufferCount
			&CommandBuffer,					// pCommandBuffers
			0,								// signalSemaphoreCount
			nullptr,						// pSignalSemaphores
		};

		result = vkQueueSubmit(GraphicQueue, 1, &SubmitInfo, VK_NULL_HANDLE);

		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to submit copy command buffer");
		}

		vkQueueWaitIdle(GraphicQueue);

		vkFreeCommandBuffers(device, CommandPool, 1, &CommandBuffer);
	}

	uint32_t FindMemoryType(uint32_t TypeFilter, VkMemoryPropertyFlags properties)
	{
		VkPhysicalDeviceMemoryProperties MemoryProperties;
		vkGetPhysicalDeviceMemoryProperties(PhysicalDevice, &MemoryProperties);

		for (uint32_t i = 0; i < MemoryProperties.memoryTypeCount; i++)
		{
			bool suitable = (MemoryProperties.memoryTypes[i].propertyFlags & properties) == properties;

			if (TypeFilter & (1 << i) && suitable)
			{
				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type");
	}

	void CreateCommandBuffers()
	{
		VkResult result;

		CommandBuffers.resize(SwapChainImageViews.size());

		VkCommandBufferAllocateInfo CommandBufferAllocateInfo = {
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// sType
			nullptr,										// pNext
			CommandPool,									// commandPool
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// level
			CommandBuffers.size()							// commandBufferCount
		};

		result = vkAllocateCommandBuffers(device, &CommandBufferAllocateInfo, CommandBuffers.data());

		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate command buffers");
		}

		/*
		for (VkCommandBuffer& CommandBuffer : CommandBuffers)
		{
			VkCommandBufferBeginInfo CommandBufferBeginInfo = {
				VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// sType
				nullptr,										// pNext
				VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,	// flags
				nullptr											// pInheritanceInfo
			};

			result = vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo);

			if (result != VK_SUCCESS)
			{
				throw std::runtime_error("failed to begin recording command buffer");
			}
		}
		*/

		for (size_t i = 0; i < CommandBuffers.size(); i++)
		{
			VkCommandBufferBeginInfo CommandBufferBeginInfo = {
				VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// sType
				nullptr,										// pNext
				VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,	// flags
				nullptr											// pInheritanceInfo
			};

			result = vkBeginCommandBuffer(CommandBuffers[i], &CommandBufferBeginInfo);

			if (result != VK_SUCCESS)
			{
				throw std::runtime_error("failed to begin recording command buffer");
			}

			VkRect2D area = {
				{0, 0},			// offset
				SwapChainExtent	// extent		
			};

			VkClearValue ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };

			VkRenderPassBeginInfo RenderPassBeginInfo = {
				VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,	// sType
				nullptr,									// pNext
				RenderPass,									// renderPass
				SwapChainFramebuffers[i],					// framebuffer
				area,										// renderArea
				1,											// clearValueCount
				&ClearColor									// pClearValues
			};

			vkCmdBeginRenderPass(CommandBuffers[i], &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(CommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, GraphicsPipeline);

			std::vector<VkBuffer> VertexBuffers = { VertexBuffer };
			std::vector<VkDeviceSize> offsets = { 0 };
			vkCmdBindVertexBuffers(CommandBuffers[i], 0, 1, VertexBuffers.data(), offsets.data());

			VkIndexType IndexType = sizeof(indices.at(0)) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
			vkCmdBindIndexBuffer(CommandBuffers[i], IndexBuffer, 0, IndexType);

			// vkCmdDraw(CommandBuffers[i], vertices.size(), 1, 0, 0);
			vkCmdDrawIndexed(CommandBuffers[i], indices.size(), 1, 0, 0, 0);

			vkCmdEndRenderPass(CommandBuffers[i]);

			result = vkEndCommandBuffer(CommandBuffers[i]);

			if (result != VK_SUCCESS)
			{
				throw std::runtime_error("failed to record command buffer");
			}
		}
	}

	void DrawFrame()
	{
		vkWaitForFences(device, 1, &fences.at(CurrentFrame), VK_TRUE, std::numeric_limits<uint64_t>::max());

		VkResult result;

		uint32_t index;
		result = vkAcquireNextImageKHR(device, SwapChain, std::numeric_limits<uint64_t>::max(), ImageAvailableSemaphore.at(CurrentFrame), VK_NULL_HANDLE, &index);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			RecreateSwapchain();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("failed to acquire swap chain image");
		}

		std::vector<VkSemaphore> WaitSemaphores = { ImageAvailableSemaphore.at(CurrentFrame) };
		std::vector<VkPipelineStageFlags> stages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		std::vector<VkSemaphore> SignalSemaphores = { RenderFinishedSemaphore.at(CurrentFrame) };

		VkSubmitInfo SubmitInfo = {
			VK_STRUCTURE_TYPE_SUBMIT_INFO,	// sType
			nullptr,						// pNext
			1,								// waitSemaphoreCount
			WaitSemaphores.data(),			// pWaitSemaphores
			stages.data(),					// pWaitDstStageMask
			1,								// commandBufferCount
			&CommandBuffers.at(index),		// pCommandBuffers
			1,								// signalSemaphoreCount
			SignalSemaphores.data()			// pSignalSemaphores
		};

		vkResetFences(device, 1, &fences.at(CurrentFrame));

		result = vkQueueSubmit(GraphicQueue, 1, &SubmitInfo, fences.at(CurrentFrame));

		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to submit draw command buffer");
		}

		std::vector<VkSwapchainKHR> swapchains = { SwapChain };

		VkPresentInfoKHR PresentInfo = {
			VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,	// sType
			nullptr,							// pNext
			1,									// waitSemaphoreCount
			SignalSemaphores.data(),			// pWaitSemaphores
			1,									// swapchainCount
			swapchains.data(),					// pSwapchains
			&index,								// pImageIndices
			nullptr								// pResults
		};

		result = vkQueuePresentKHR(PresentQueue, &PresentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || FramebufferResized)
		{
			FramebufferResized = false;
			RecreateSwapchain();
		}
		else if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to present swap chain image");
		}

		CurrentFrame = (CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	void CreateSemaphoresAndFences()
	{
		ImageAvailableSemaphore.resize(MAX_FRAMES_IN_FLIGHT);
		RenderFinishedSemaphore.resize(MAX_FRAMES_IN_FLIGHT);
		fences.resize(MAX_FRAMES_IN_FLIGHT);

		VkSemaphoreCreateInfo SemaphoreCreateInfo = {
			VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,	// sType
			nullptr,									// pNext
			0											// flags
		};

		VkFenceCreateInfo FenceCreateInfo = {
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	// sType
			nullptr,								// pNext
			VK_FENCE_CREATE_SIGNALED_BIT			// flags
		};

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			VkResult ImageAvailableResult = vkCreateSemaphore(device, &SemaphoreCreateInfo, nullptr, &ImageAvailableSemaphore.at(i));
			VkResult RenderFinishedResult = vkCreateSemaphore(device, &SemaphoreCreateInfo, nullptr, &RenderFinishedSemaphore.at(i));
			VkResult FenceResult = vkCreateFence(device, &FenceCreateInfo, nullptr, &fences.at(i));

			if (ImageAvailableResult || RenderFinishedResult || FenceResult)
			{
				throw std::runtime_error("failed to create synchronization objects for a frame");
			}
		}
	}

	void MainLoop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
			DrawFrame();
		}

		vkDeviceWaitIdle(device);
	}

	void cleanup()
	{
		CleanupSwapchain();

		vkDestroyBuffer(device, IndexBuffer, nullptr);
		vkFreeMemory(device, IndexBufferMemory, nullptr);

		vkDestroyBuffer(device, VertexBuffer, nullptr);
		vkFreeMemory(device, VertexBufferMemory, nullptr);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vkDestroyFence(device, fences.at(i), nullptr);
			vkDestroySemaphore(device, RenderFinishedSemaphore.at(i), nullptr);
			vkDestroySemaphore(device, ImageAvailableSemaphore.at(i), nullptr);
		}

		vkDestroyCommandPool(device, CommandPool, nullptr);

		// device VkQueue are implicitly cleaned up when the VkDevice is destroyed

		vkDestroyDevice(device, nullptr);

		// VkPhysicalDevice are implicitly destroyed when VkInstance is destroyed

		if (EnableValidationLayer)
		{
			vkDestroyDebugUtilsMessengerEXT(instance, messenger, nullptr);
		}

		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);

		glfwDestroyWindow(window);
		glfwTerminate();
	}

	void CleanupSwapchain()
	{
		for (VkFramebuffer framebuffer : SwapChainFramebuffers)
		{
			vkDestroyFramebuffer(device, framebuffer, nullptr);
		}

		// command buffers are automatically freed when their command pool is destroyed

		// we clean up the existing command buffers and reuse the existing pool to allocate the new command buffers
		vkFreeCommandBuffers(device, CommandPool, CommandBuffers.size(), CommandBuffers.data());

		vkDestroyPipeline(device, GraphicsPipeline, nullptr);
		vkDestroyPipelineLayout(device, PipelineLayout, nullptr);
		vkDestroyRenderPass(device, RenderPass, nullptr);

		for (VkImageView view : SwapChainImageViews)
		{
			vkDestroyImageView(device, view, nullptr);
		}

		// swap chain images are automatically cleaned up once the swap chain is destroyed

		vkDestroySwapchainKHR(device, SwapChain, nullptr);
	}
};

int main()
{
	VulkanApplication app;

	try
	{
		app.run();
	}
	catch (const std::exception & exc)
	{
		std::cerr << exc.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}