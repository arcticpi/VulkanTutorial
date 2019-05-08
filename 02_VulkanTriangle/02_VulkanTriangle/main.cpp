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
	const char* WindowTitle = "Vulkan Window";

	void InitWindow()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // we don't need to create an OpenGL context
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(WindowWidth, WindowHeight, WindowTitle, nullptr, nullptr);
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

	void InitVulkan()
	{
		CreateInstance();
		SetupDebugMessenger();
		CreateSurface();
		PickPhysicalDevice();
		CreateLogicalDevice();
		CreateSwapChain();
		CreateImageViews();
		CreateRenderPass();
		CreateGraphicsPipeline();
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
			VkExtent2D extent = { WindowWidth, WindowHeight };

			VkExtent2D min = capabilities.minImageExtent;
			VkExtent2D max = capabilities.maxImageExtent;

			extent.width = std::max(min.width, std::min(max.width, extent.width));
			extent.height = std::max(min.height, std::min(max.height, extent.height));

			return extent;
		}
	}

	void CreateSwapChain()
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

		VkResult result;

		result = vkCreateSwapchainKHR(device, &SwapChainCreateInfo, nullptr, &SwapChain);

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

	void CreateImageViews()
	{
		// SwapChainImageViews.resize(SwapChainImages.size());

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

		VkRenderPassCreateInfo RenderPassCreateInfo = {
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,	// sType
			nullptr,									// pNext
			0,											// flags
			1,											// attachmentCount
			&AttachmentDescription,						// pAttachments
			1,											// subpassCount
			&SubpassDescription,						// pSubpasses
			0,											// dependencyCount
			nullptr										// pDependencies
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

		VkPipelineVertexInputStateCreateInfo VertexInputStateCreateInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,	// sType
			nullptr,													// pNext
			0,															// flags
			0,															// vertexBindingDescriptionCount
			nullptr,													// pVertexBindingDescriptions
			0,															// vertexAttributeDescriptionCount
			nullptr														// pVertexAttributeDescriptions
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
			0,														// scissorCount
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

		// bytecode compilation and linking happens during graphics pipeline creation
		// so we can destroy shader modules as soon as pipeline creation is finished

		vkDestroyShaderModule(device, VertModule, nullptr);
		vkDestroyShaderModule(device, FragModule, nullptr);
	}

	void MainLoop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
		}
	}

	void cleanup()
	{
		vkDestroyPipelineLayout(device, PipelineLayout, nullptr);
		vkDestroyRenderPass(device, RenderPass, nullptr);

		for (VkImageView view : SwapChainImageViews)
		{
			vkDestroyImageView(device, view, nullptr);
		}

		// swap chain images are automatically cleaned up once the swap chain is destroyed

		vkDestroySwapchainKHR(device, SwapChain, nullptr);

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