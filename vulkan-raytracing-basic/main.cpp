#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <filesystem>
#include <tuple>
#include <bitset>
#include <span>
#include <unordered_map>
#include "shader_module.h"

#define TINYOBJLOADER_IMPLEMENTATION // define this in only *one* .cc
// Optional. define TINYOBJLOADER_USE_MAPBOX_EARCUT gives robust triangulation. Requires C++11
//#define TINYOBJLOADER_USE_MAPBOX_EARCUT
#include "tiny_obj_loader.h"

typedef unsigned int uint;

const uint32_t WIDTH = 1200;
const uint32_t HEIGHT = 800;
const uint32_t SHADER_GROUP_HANDLE_SIZE = 32;

#ifdef NDEBUG
    const bool ON_DEBUG = false;
#else
    const bool ON_DEBUG = true;
#endif


struct Global {
    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
	PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;

	VkPhysicalDeviceRayTracingPipelinePropertiesKHR  rtProperties = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
    
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;

    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice;
    VkDevice device;

    VkQueue graphicsQueue; // assume allowing graphics and present
    uint queueFamilyIndex;

    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    // std::vector<VkImageView> swapChainImageViews;
    const VkFormat swapChainImageFormat = VK_FORMAT_B8G8R8A8_SRGB;    // intentionally chosen to match a specific format
    const VkExtent2D swapChainImageExtent = { .width = WIDTH, .height = HEIGHT };

    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;

    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence fence0;

    VkBuffer blasBuffer;
    VkDeviceMemory blasBufferMem;
    VkAccelerationStructureKHR blas;
    VkDeviceAddress blasAddress;

    VkBuffer tlasBuffer;
    VkDeviceMemory tlasBufferMem;
    VkAccelerationStructureKHR tlas;

    VkImage outImage;
    VkDeviceMemory outImageMem;
    VkImageView outImageView;

    VkBuffer uniformBuffer;
    VkDeviceMemory uniformBufferMem;

    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;

    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;

    VkBuffer sbtBuffer;
    VkDeviceMemory sbtBufferMem;
    VkStridedDeviceAddressRegionKHR rgenSbt{};
    VkStridedDeviceAddressRegionKHR missSbt{};
    VkStridedDeviceAddressRegionKHR hitgSbt{};
    
    ~Global() {
        vkDestroyBuffer(device, tlasBuffer, nullptr);
        vkFreeMemory(device, tlasBufferMem, nullptr);
        vkDestroyAccelerationStructureKHR(device, tlas, nullptr);

        vkDestroyBuffer(device, blasBuffer, nullptr);
        vkFreeMemory(device, blasBufferMem, nullptr);
        vkDestroyAccelerationStructureKHR(device, blas, nullptr);

        vkDestroyImageView(device, outImageView, nullptr);
        vkDestroyImage(device, outImage, nullptr);
        vkFreeMemory(device, outImageMem, nullptr);

        vkDestroyBuffer(device, uniformBuffer, nullptr);
        vkFreeMemory(device, uniformBufferMem, nullptr);

        vkDestroyBuffer(device, sbtBuffer, nullptr);
        vkFreeMemory(device, sbtBufferMem, nullptr);

        vkDestroyPipeline(device, pipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);


        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroyFence(device, fence0, nullptr);

        vkDestroyCommandPool(device, commandPool, nullptr);

        // for (auto imageView : swapChainImageViews) {
        //     vkDestroyImageView(device, imageView, nullptr);
        // }
        vkDestroySwapchainKHR(device, swapChain, nullptr);
        vkDestroyDevice(device, nullptr);
        if (ON_DEBUG) {
            ((PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vk.instance, "vkDestroyDebugUtilsMessengerEXT"))
                (vk.instance, vk.debugMessenger, nullptr);
        }
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
    }
} vk;

void loadDeviceExtensionFunctions(VkDevice device)
{
    vk.vkGetBufferDeviceAddressKHR = (PFN_vkGetBufferDeviceAddressKHR)(vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR"));
    vk.vkGetAccelerationStructureDeviceAddressKHR = (PFN_vkGetAccelerationStructureDeviceAddressKHR)(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR"));
    vk.vkCreateAccelerationStructureKHR = (PFN_vkCreateAccelerationStructureKHR)(vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
    vk.vkDestroyAccelerationStructureKHR = (PFN_vkDestroyAccelerationStructureKHR)(vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));
    vk.vkGetAccelerationStructureBuildSizesKHR = (PFN_vkGetAccelerationStructureBuildSizesKHR)(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR"));
    vk.vkCmdBuildAccelerationStructuresKHR = (PFN_vkCmdBuildAccelerationStructuresKHR)(vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR"));
	vk.vkCreateRayTracingPipelinesKHR = (PFN_vkCreateRayTracingPipelinesKHR)(vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR"));
	vk.vkGetRayTracingShaderGroupHandlesKHR = (PFN_vkGetRayTracingShaderGroupHandlesKHR)(vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR"));
    vk.vkCmdTraceRaysKHR = (PFN_vkCmdTraceRaysKHR)(vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR"));

    VkPhysicalDeviceProperties2 deviceProperties2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &vk.rtProperties,
    };
	vkGetPhysicalDeviceProperties2(vk.physicalDevice, &deviceProperties2);

    if (vk.rtProperties.shaderGroupHandleSize != SHADER_GROUP_HANDLE_SIZE) {
        throw std::runtime_error("shaderGroupHandleSize must be 32 mentioned in the vulakn spec (Table 69. Required Limits)!");
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    const char* severity;
    switch (messageSeverity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: severity = "[Verbose]"; break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: severity = "[Warning]"; break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: severity = "[Error]"; break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: severity = "[Info]"; break;
    default: severity = "[Unknown]";
    }

    const char* types;
    switch (messageType) {
    case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT: types = "[General]"; break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT: types = "[Performance]"; break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT: types = "[Validation]"; break;
    default: types = "[Unknown]";
    }

    std::cout << "[Debug]" << severity << types << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

bool checkValidationLayerSupport(std::vector<const char*>& reqestNames) 
{
    uint32_t count;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> availables(count);
    vkEnumerateInstanceLayerProperties(&count, availables.data());

    for (const char* reqestName : reqestNames) {
        bool found = false;

        for (const auto& available : availables) {
            if (strcmp(reqestName, available.layerName) == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            return false;
        }
    }

    return true;
}

bool checkDeviceExtensionSupport(VkPhysicalDevice device, std::vector<const char*>& reqestNames) 
{
    uint32_t count;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> availables(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, availables.data());

    for (const char* reqestName : reqestNames) {
        bool found = false;

        for (const auto& available : availables) {
            if (strcmp(reqestName, available.extensionName) == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            return false;
        }
    }

    return true;
}

// Define a hash function for std::tuple<int, int>
struct tuple_hash {
    template <typename T1, typename T2>
    std::size_t operator()(const std::tuple<T1, T2>& t) const {
        return std::hash<T1>()(std::get<0>(t)) ^ (std::hash<T2>()(std::get<1>(t)) << 1);
    }
};

struct Geometry
{
    tinyobj::ObjReaderConfig reader_config;
    tinyobj::ObjReader reader;

    std::vector<float> vertData;
    std::vector<uint32_t> idxData;

    void readfile(const std::string& inputfile) {
        if (!reader.ParseFromFile(inputfile, reader_config)) {
            if (!reader.Error().empty()) {
                std::cerr << "TinyObjReader: " << reader.Error();
            }
            exit(1);
        }

        if (!reader.Warning().empty()) {
            std::cout << "TinyObjReader: " << reader.Warning();
        }

        const auto& attrib = reader.GetAttrib();
        const auto& shapes = reader.GetShapes();

        vertData = attrib.vertices;

        idxData.resize(shapes[0].mesh.indices.size());
        for (uint32_t idx = 0; idx < shapes[0].mesh.indices.size(); ++idx) {
            const auto& vertIdx = shapes[0].mesh.indices[idx].vertex_index;
            idxData[idx] = static_cast<uint32_t>(vertIdx);
        }
    }
} geo;

GLFWwindow* createWindow()
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    return glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
}

void createVkInstance(GLFWwindow* window)
{
    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Hello Triangle",
        .apiVersion = VK_API_VERSION_1_3
    };

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (ON_DEBUG) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    std::vector<const char*> validationLayers;
    if (ON_DEBUG) validationLayers.push_back("VK_LAYER_KHRONOS_validation");
    if (!checkValidationLayerSupport(validationLayers)) {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debugCallback,
    };

    VkInstanceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = ON_DEBUG ? &debugCreateInfo : nullptr,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = (uint)validationLayers.size(),
        .ppEnabledLayerNames = validationLayers.data(),
        .enabledExtensionCount = (uint)extensions.size(),
        .ppEnabledExtensionNames = extensions.data(),
    };

    if (vkCreateInstance(&createInfo, nullptr, &vk.instance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }

    if (ON_DEBUG) {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vk.instance, "vkCreateDebugUtilsMessengerEXT");
        if (!func || func(vk.instance, &debugCreateInfo, nullptr, &vk.debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    if (glfwCreateWindowSurface(vk.instance, window, nullptr, &vk.surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
}

void createVkDevice()
{
    vk.physicalDevice = VK_NULL_HANDLE;

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(vk.instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(vk.instance, &deviceCount, devices.data());

    std::vector<const char*> extentions = { 
        VK_KHR_SWAPCHAIN_EXTENSION_NAME, 

        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, // not used
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, // not used
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,

        VK_KHR_SPIRV_1_4_EXTENSION_NAME, // not used
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    };

    for (const auto& device : devices)
    {
        if (checkDeviceExtensionSupport(device, extentions))
        {
            vk.physicalDevice = device;
            break;
        }
    }

    if (vk.physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable GPU!");
    }

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vk.physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(vk.physicalDevice, &queueFamilyCount, queueFamilies.data());

    vk.queueFamilyIndex = 0;
    {
        for (; vk.queueFamilyIndex < queueFamilyCount; ++vk.queueFamilyIndex)
        {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(vk.physicalDevice, vk.queueFamilyIndex, vk.surface, &presentSupport);

            if (queueFamilies[vk.queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT && presentSupport)
                break;
        }

        if (vk.queueFamilyIndex >= queueFamilyCount)
            throw std::runtime_error("failed to find a graphics & present queue!");
    }
    float queuePriority = 1.0f;

    VkDeviceQueueCreateInfo queueCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = vk.queueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };

    VkDeviceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledExtensionCount = (uint)extentions.size(),
        .ppEnabledExtensionNames = extentions.data(),
    };

    VkPhysicalDeviceBufferDeviceAddressFeatures f1{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
        .bufferDeviceAddress = VK_TRUE,
    };

	VkPhysicalDeviceAccelerationStructureFeaturesKHR f2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .accelerationStructure = VK_TRUE,
    };
	
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR f3{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .rayTracingPipeline = VK_TRUE,
    };

    createInfo.pNext = &f1;
    f1.pNext = &f2;
    f2.pNext = &f3;

    if (vkCreateDevice(vk.physicalDevice, &createInfo, nullptr, &vk.device) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }

    vkGetDeviceQueue(vk.device, vk.queueFamilyIndex, 0, &vk.graphicsQueue);
}

void createSwapChain()
{
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk.physicalDevice, vk.surface, &capabilities);

    const VkColorSpaceKHR defaultSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    {
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(vk.physicalDevice, vk.surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(vk.physicalDevice, vk.surface, &formatCount, formats.data());

        bool found = false;
        for (auto format : formats) {
            if (format.format == vk.swapChainImageFormat && format.colorSpace == defaultSpace) {
                found = true;
                break;
            }
        }

        if (!found) {
            throw std::runtime_error("");
        }
    }

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    {
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(vk.physicalDevice, vk.surface, &presentModeCount, nullptr);
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(vk.physicalDevice, vk.surface, &presentModeCount, presentModes.data());

        for (auto mode : presentModes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
        }
    }

    uint imageCount = capabilities.minImageCount + 1;
    VkSwapchainCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = vk.surface,
        .minImageCount = imageCount,
        .imageFormat = vk.swapChainImageFormat,
        .imageColorSpace = defaultSpace,
        .imageExtent = {.width = WIDTH , .height = HEIGHT },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
    };

    if (vkCreateSwapchainKHR(vk.device, &createInfo, nullptr, &vk.swapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(vk.device, vk.swapChain, &imageCount, nullptr);
    vk.swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(vk.device, vk.swapChain, &imageCount, vk.swapChainImages.data());

    for (const auto& image : vk.swapChainImages) {
        VkImageViewCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = vk.swapChainImageFormat,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };

        // VkImageView imageView;
        // if (vkCreateImageView(vk.device, &createInfo, nullptr, &imageView) != VK_SUCCESS) {
        //     throw std::runtime_error("failed to create image views!");
        // }
        // vk.swapChainImageViews.push_back(imageView);
    }
}

void createCommandCenter()
{
    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = vk.queueFamilyIndex,
    };

    if (vkCreateCommandPool(vk.device, &poolInfo, nullptr, &vk.commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }

    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = vk.commandPool,
        .commandBufferCount = 1,
    };

    if (vkAllocateCommandBuffers(vk.device, &allocInfo, &vk.commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

void createSyncObjects()
{
    VkSemaphoreCreateInfo semaphoreInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };
    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    if (vkCreateSemaphore(vk.device, &semaphoreInfo, nullptr, &vk.imageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateSemaphore(vk.device, &semaphoreInfo, nullptr, &vk.renderFinishedSemaphore) != VK_SUCCESS ||
        vkCreateFence(vk.device, &fenceInfo, nullptr, &vk.fence0) != VK_SUCCESS) {
        throw std::runtime_error("failed to create synchronization objects for a frame!");
    }

}


uint findMemoryType(uint32_t memoryTypeBits, VkMemoryPropertyFlags reqMemProps)
{
    uint memTypeIndex = 0;
    std::bitset<32> isSuppoted(memoryTypeBits);

    VkPhysicalDeviceMemoryProperties spec;
    vkGetPhysicalDeviceMemoryProperties(vk.physicalDevice, &spec);

    for (auto& [props, _] : std::span<VkMemoryType>(spec.memoryTypes, spec.memoryTypeCount)) {
        if (isSuppoted[memTypeIndex] && (props & reqMemProps) == reqMemProps) {
            break;
        }
        ++memTypeIndex;
    }
    return memTypeIndex;
}

std::tuple<VkBuffer, VkDeviceMemory> createBuffer(
    VkDeviceSize size, 
    VkBufferUsageFlags usage, 
    VkMemoryPropertyFlags reqMemProps)
{
    VkBuffer buffer;
    VkDeviceMemory bufferMemory;

    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
    };
    if (vkCreateBuffer(vk.device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create vertex buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(vk.device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, reqMemProps),
    };

    if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        VkMemoryAllocateFlagsInfo flagsInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
            .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR,
        };
        allocInfo.pNext = &flagsInfo;
    }

    if (vkAllocateMemory(vk.device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate vertex buffer memory!");
    }

    vkBindBufferMemory(vk.device, buffer, bufferMemory, 0);

    return { buffer, bufferMemory };
}

std::tuple<VkImage, VkDeviceMemory> createImage(
    VkExtent2D extent,
    VkFormat format,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags reqMemProps)
{
    VkImage image;
    VkDeviceMemory imageMemory;

    VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = { extent.width, extent.height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    if (vkCreateImage(vk.device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(vk.device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, reqMemProps),
    };

    if (vkAllocateMemory(vk.device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(vk.device, image, imageMemory, 0);

    return { image, imageMemory };
}

void setImageLayout(
    VkCommandBuffer cmdbuffer,
    VkImage image,
    VkImageLayout oldImageLayout,
    VkImageLayout newImageLayout,
    VkImageSubresourceRange subresourceRange,
    VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)
{
    VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = oldImageLayout,
        .newLayout = newImageLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = subresourceRange,
    };

    if (oldImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    } else if (oldImageLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    }

    if (newImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    } else if (newImageLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    }

    vkCmdPipelineBarrier(
        cmdbuffer, srcStageMask, dstStageMask, 0,
        0, nullptr, 0, nullptr, 1, &barrier);
}

inline VkDeviceAddress getDeviceAddressOf(VkBuffer buffer)
{
    VkBufferDeviceAddressInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer,
    };
    return vk.vkGetBufferDeviceAddressKHR(vk.device, &info);
}

inline VkDeviceAddress getDeviceAddressOf(VkAccelerationStructureKHR as)
{
    VkAccelerationStructureDeviceAddressInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = as,
    };
    return vk.vkGetAccelerationStructureDeviceAddressKHR(vk.device, &info);
}

void createBLAS()
{
    std::vector<float> vertices = geo.vertData;
    std::vector<uint32_t> indices = geo.idxData;

    VkTransformMatrixKHR geoTransforms[] = {
        {
            1.0f, 0.0f, 0.0f, -2.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f
        }, 
        {
            1.0f, 0.0f, 0.0f, 2.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f
        },
    };

    auto vertSize = sizeof(vertices[0]) * vertices.size();
    auto indSize = sizeof(indices[0]) * indices.size();

    auto [vertexBuffer, vertexBufferMem] = createBuffer(
        vertSize,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    auto [indexBuffer, indexBufferMem] = createBuffer(
        indSize,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    auto [geoTransformBuffer, geoTransformBufferMem] = createBuffer(
        sizeof(geoTransforms), 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    void* dst;

    vkMapMemory(vk.device, vertexBufferMem, 0, vertSize, 0, &dst);
    memcpy(dst, vertices.data(), vertSize);
    vkUnmapMemory(vk.device, vertexBufferMem);

    vkMapMemory(vk.device, indexBufferMem, 0, indSize, 0, &dst);
    memcpy(dst, indices.data(), indSize);
    vkUnmapMemory(vk.device, indexBufferMem);

    vkMapMemory(vk.device, geoTransformBufferMem, 0, sizeof(geoTransforms), 0, &dst);
    memcpy(dst, geoTransforms, sizeof(geoTransforms));
    vkUnmapMemory(vk.device, geoTransformBufferMem);

    VkAccelerationStructureGeometryKHR geometry0{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = {
            .triangles = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                .vertexData = { .deviceAddress = getDeviceAddressOf(vertexBuffer) },
                .vertexStride = sizeof(vertices[0]) * 3,
                .maxVertex = static_cast<uint32_t>(vertices.size() / 3) - 1,
                .indexType = VK_INDEX_TYPE_UINT32,
                .indexData = { .deviceAddress = getDeviceAddressOf(indexBuffer) },
                .transformData = { .deviceAddress = getDeviceAddressOf(geoTransformBuffer) },
            },
        },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
    };
    VkAccelerationStructureGeometryKHR geometries[] = { geometry0, geometry0 };

    uint32_t triangleCount0 = static_cast<uint32_t>(indices.size()) / 3;
    uint32_t triangleCounts[] = { triangleCount0, triangleCount0 };

    VkAccelerationStructureBuildGeometryInfoKHR buildBlasInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .geometryCount = sizeof(geometries) / sizeof(geometries[0]),
        .pGeometries = geometries,
    };
    
    VkAccelerationStructureBuildSizesInfoKHR requiredSize{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    vk.vkGetAccelerationStructureBuildSizesKHR(
        vk.device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildBlasInfo,
        triangleCounts,
        &requiredSize);

    std::tie(vk.blasBuffer, vk.blasBufferMem) = createBuffer(
        requiredSize.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    auto [scratchBuffer, scratchBufferMem] = createBuffer(
        requiredSize.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Generate BLAS handle
    {
        VkAccelerationStructureCreateInfoKHR asCreateInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = vk.blasBuffer,
            .size = requiredSize.accelerationStructureSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        };
        vk.vkCreateAccelerationStructureKHR(vk.device, &asCreateInfo, nullptr, &vk.blas);

        vk.blasAddress = getDeviceAddressOf(vk.blas);
    }

    // Build BLAS using GPU operations
    {
        vkResetCommandBuffer(vk.commandBuffer, 0);
        VkCommandBufferBeginInfo beginInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkBeginCommandBuffer(vk.commandBuffer, &beginInfo);
        {
            buildBlasInfo.dstAccelerationStructure = vk.blas;
            buildBlasInfo.scratchData.deviceAddress = getDeviceAddressOf(scratchBuffer);
            VkAccelerationStructureBuildRangeInfoKHR buildBlasRangeInfo[] = {
                { 
                    .primitiveCount = triangleCounts[0],
                    .transformOffset = 0,
                },
                { 
                    .primitiveCount = triangleCounts[1],
                    .transformOffset = sizeof(geoTransforms[0]),
                }
            };

            VkAccelerationStructureBuildGeometryInfoKHR buildBlasInfos[] = { buildBlasInfo };
            VkAccelerationStructureBuildRangeInfoKHR* buildBlasRangeInfos[] = { buildBlasRangeInfo };
            vk.vkCmdBuildAccelerationStructuresKHR(vk.commandBuffer, 1, buildBlasInfos, buildBlasRangeInfos);
            // vkCmdBuildAccelerationStructuresKHR(vk.commandBuffer, 1, &buildBlasInfo, 
            // (const VkAccelerationStructureBuildRangeInfoKHR *const *)&buildBlasRangeInfo);
        }
        vkEndCommandBuffer(vk.commandBuffer);

        VkSubmitInfo submitInfo {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &vk.commandBuffer,
        }; 
        vkQueueSubmit(vk.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(vk.graphicsQueue);
    }

    vkFreeMemory(vk.device, scratchBufferMem, nullptr);
    vkFreeMemory(vk.device, vertexBufferMem, nullptr);
    vkFreeMemory(vk.device, indexBufferMem, nullptr);
    vkFreeMemory(vk.device, geoTransformBufferMem, nullptr);
    vkDestroyBuffer(vk.device, scratchBuffer, nullptr);
    vkDestroyBuffer(vk.device, vertexBuffer, nullptr);
    vkDestroyBuffer(vk.device, indexBuffer, nullptr);
    vkDestroyBuffer(vk.device, geoTransformBuffer, nullptr);
}

void createTLAS()
{
    VkTransformMatrixKHR insTransforms[] = {
        {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 2.0f,
            0.0f, 0.0f, 1.0f, 0.0f
        }, 
        {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, -2.0f,
            0.0f, 0.0f, 1.0f, 0.0f
        },
    };

    VkAccelerationStructureInstanceKHR instance0 {
        .instanceCustomIndex = 100,
        .mask = 0xFF,
        .instanceShaderBindingTableRecordOffset = 0,
        .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
        .accelerationStructureReference = vk.blasAddress,
    };
    VkAccelerationStructureInstanceKHR instanceData[] = { instance0, instance0 };
    instanceData[0].transform = insTransforms[0];
    instanceData[1].transform = insTransforms[1];
    instanceData[1].instanceShaderBindingTableRecordOffset = 2; // 2 geometry (in instance0) + 2 geometry (in instance1)

    auto [instanceBuffer, instanceBufferMem] = createBuffer(
        sizeof(instanceData), 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* dst;
    vkMapMemory(vk.device, instanceBufferMem, 0, sizeof(instanceData), 0, &dst);
    memcpy(dst, instanceData, sizeof(instanceData));
    vkUnmapMemory(vk.device, instanceBufferMem);

    VkAccelerationStructureGeometryKHR instances{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = {
            .instances = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                .data = { .deviceAddress = getDeviceAddressOf(instanceBuffer) },
            },
        },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
    };

    uint32_t instanceCount = 2;

    VkAccelerationStructureBuildGeometryInfoKHR buildTlasInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .geometryCount = 1,     // It must be 1 with .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR as shown in the vulkan spec.
        .pGeometries = &instances,
    };

    VkAccelerationStructureBuildSizesInfoKHR requiredSize{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    vk.vkGetAccelerationStructureBuildSizesKHR(
        vk.device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildTlasInfo,
        &instanceCount,
        &requiredSize);

    std::tie(vk.tlasBuffer, vk.tlasBufferMem) = createBuffer(
        requiredSize.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    auto [scratchBuffer, scratchBufferMem] = createBuffer(
        requiredSize.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Generate TLAS handle
    {
        VkAccelerationStructureCreateInfoKHR asCreateInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = vk.tlasBuffer,
            .size = requiredSize.accelerationStructureSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        };
        vk.vkCreateAccelerationStructureKHR(vk.device, &asCreateInfo, nullptr, &vk.tlas);
    }

    // Build TLAS using GPU operations
    {
        vkResetCommandBuffer(vk.commandBuffer, 0);
        VkCommandBufferBeginInfo beginInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkBeginCommandBuffer(vk.commandBuffer, &beginInfo);
        {
            buildTlasInfo.dstAccelerationStructure = vk.tlas;
            buildTlasInfo.scratchData.deviceAddress = getDeviceAddressOf(scratchBuffer);

            VkAccelerationStructureBuildRangeInfoKHR buildTlasRangeInfo = { .primitiveCount = instanceCount };
            VkAccelerationStructureBuildRangeInfoKHR* buildTlasRangeInfo_[] = { &buildTlasRangeInfo };
            vk.vkCmdBuildAccelerationStructuresKHR(vk.commandBuffer, 1, &buildTlasInfo, buildTlasRangeInfo_);
        }
        vkEndCommandBuffer(vk.commandBuffer);

        VkSubmitInfo submitInfo {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &vk.commandBuffer,
        }; 
        vkQueueSubmit(vk.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(vk.graphicsQueue);
    }

    vkFreeMemory(vk.device, scratchBufferMem, nullptr);
    vkFreeMemory(vk.device, instanceBufferMem, nullptr);
    vkDestroyBuffer(vk.device, scratchBuffer, nullptr);
    vkDestroyBuffer(vk.device, instanceBuffer, nullptr);
}

void createOutImage()
{
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM; //VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_SRGB(==vk.swapChainImageFormat)
    std::tie(vk.outImage, vk.outImageMem) = createImage(
        { WIDTH, HEIGHT },
        format, 
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkImageSubresourceRange subresourceRange{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    
    VkImageViewCreateInfo ci0{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = vk.outImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = subresourceRange,
    };
    vkCreateImageView(vk.device, &ci0, nullptr, &vk.outImageView);

    vkResetCommandBuffer(vk.commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(vk.commandBuffer, &beginInfo);
    {
        setImageLayout(
            vk.commandBuffer, 
            vk.outImage, 
            VK_IMAGE_LAYOUT_UNDEFINED, 
            VK_IMAGE_LAYOUT_GENERAL, 
            subresourceRange);
    }
    vkEndCommandBuffer(vk.commandBuffer);

    VkSubmitInfo submitInfo {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &vk.commandBuffer,
    }; 
    vkQueueSubmit(vk.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(vk.graphicsQueue);
}

void createUniformBuffer()
{
    struct Data{
        float cameraPos[3];
        float yFov_degree;
    } dataSrc; 

    std::tie(vk.uniformBuffer, vk.uniformBufferMem) = createBuffer(
        sizeof(dataSrc), 
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* dst;
    vkMapMemory(vk.device, vk.uniformBufferMem, 0, sizeof(dataSrc), 0, &dst);
    *(Data*) dst = {0, 0, 10, 60};
    vkUnmapMemory(vk.device, vk.uniformBufferMem);
}

const char* raygen_src = R"(
#version 460
#extension GL_EXT_ray_tracing : enable

layout(binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 1, rgba8) uniform image2D image;
layout(binding = 2) uniform CameraProperties 
{
    vec3 cameraPos;
    float yFov_degree;
} g;

layout(location = 0) rayPayloadEXT vec3 hitValue;

void main() 
{
    const vec3 cameraX = vec3(1, 0, 0);
    const vec3 cameraY = vec3(0, -1, 0);
    const vec3 cameraZ = vec3(0, 0, -1);
    const float aspect_y = tan(radians(g.yFov_degree) * 0.5);
    const float aspect_x = aspect_y * float(gl_LaunchSizeEXT.x) / float(gl_LaunchSizeEXT.y);

    const vec2 screenCoord = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
    const vec2 ndc = screenCoord/vec2(gl_LaunchSizeEXT.xy) * 2.0 - 1.0;
    vec3 rayDir = ndc.x*aspect_x*cameraX + ndc.y*aspect_y*cameraY + cameraZ;

    hitValue = vec3(0.0);

    traceRayEXT(
        topLevelAS,                         // topLevel
        gl_RayFlagsOpaqueEXT, 0xff,         // rayFlags, cullMask
        0, 1, 0,                            // sbtRecordOffset, sbtRecordStride, missIndex
        g.cameraPos, 0.0, rayDir, 100.0,    // origin, tmin, direction, tmax
        0);                                 // payload

    imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(hitValue.bgr, 0.0));
})";

const char* miss_src = R"(
#version 460
#extension GL_EXT_ray_tracing : enable

layout(location = 0) rayPayloadInEXT vec3 hitValue;

void main()
{
    hitValue = vec3(0.0, 0.0, 0.2);
})";

const char* chit_src = R"(
#version 460
#extension GL_EXT_ray_tracing : enable

layout(shaderRecordEXT) buffer CustomData
{
    vec3 color;
};

layout(location = 0) rayPayloadInEXT vec3 hitValue;
hitAttributeEXT vec2 attribs;

void main()
{
    if (gl_PrimitiveID == 1 && 
        gl_InstanceID == 1 && 
        gl_InstanceCustomIndexEXT == 100 && 
        gl_GeometryIndexEXT == 1) {
        hitValue = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
    }
    else {
        hitValue = color;
    }
})";

void createRayTracingPipeline()
{
    VkDescriptorSetLayoutBinding bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
        },
        {
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
        },
    };

    VkDescriptorSetLayoutCreateInfo ci0{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = sizeof(bindings) / sizeof(bindings[0]),
        .pBindings = bindings,
    };
    vkCreateDescriptorSetLayout(vk.device, &ci0, nullptr, &vk.descriptorSetLayout);

    VkPipelineLayoutCreateInfo ci1{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &vk.descriptorSetLayout,
    };
    vkCreatePipelineLayout(vk.device, &ci1, nullptr, &vk.pipelineLayout);

    ShaderModule<VK_SHADER_STAGE_RAYGEN_BIT_KHR> raygenModule(vk.device, raygen_src);
    ShaderModule<VK_SHADER_STAGE_MISS_BIT_KHR> missModule(vk.device, miss_src);
    ShaderModule<VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR> chitModule(vk.device, chit_src);
    VkPipelineShaderStageCreateInfo stages[] = { raygenModule, missModule, chitModule };

    VkRayTracingShaderGroupCreateInfoKHR shaderGroups[] = {
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 0,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
        },
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 1,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
        },
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = 2,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
        },
    };
    
    VkRayTracingPipelineCreateInfoKHR ci2{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount = sizeof(stages) / sizeof(stages[0]),
        .pStages = stages,
        .groupCount = sizeof(shaderGroups) / sizeof(shaderGroups[0]),
        .pGroups = shaderGroups,
        .maxPipelineRayRecursionDepth = 1,
        .layout = vk.pipelineLayout,
    };
    vk.vkCreateRayTracingPipelinesKHR(vk.device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &ci2, nullptr, &vk.pipeline);
}

void createDescriptorSets()
{
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
    };
    VkDescriptorPoolCreateInfo ci0 {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = sizeof(poolSizes) / sizeof(poolSizes[0]),
        .pPoolSizes = poolSizes,
    };
    vkCreateDescriptorPool(vk.device, &ci0, nullptr, &vk.descriptorPool);

    VkDescriptorSetAllocateInfo ai0 {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = vk.descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &vk.descriptorSetLayout,
    };
    vkAllocateDescriptorSets(vk.device, &ai0, &vk.descriptorSet);

    VkWriteDescriptorSet write_temp{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = vk.descriptorSet,
        .descriptorCount = 1,
    };

    // Descriptor(binding = 0), VkAccelerationStructure
    VkWriteDescriptorSetAccelerationStructureKHR desc0{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &vk.tlas,  
    };
    VkWriteDescriptorSet write0 = write_temp;
    write0.pNext = &desc0;
    write0.dstBinding = 0;
    write0.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    
    // Descriptor(binding = 1), VkImage for output
    VkDescriptorImageInfo desc1{
        .imageView = vk.outImageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    VkWriteDescriptorSet write1 = write_temp;
    write1.dstBinding = 1;
    write1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write1.pImageInfo = &desc1;
 
    // Descriptor(binding = 2), VkBuffer for uniform
    VkDescriptorBufferInfo desc2{
        .buffer = vk.uniformBuffer,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };
    VkWriteDescriptorSet write2 = write_temp;
    write2.dstBinding = 2;
    write2.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write2.pBufferInfo = &desc2;

    VkWriteDescriptorSet writeInfos[] = { write0, write1, write2 };
    vkUpdateDescriptorSets(vk.device, sizeof(writeInfos) / sizeof(writeInfos[0]), writeInfos, 0, VK_NULL_HANDLE);
    /*
    [VUID-VkWriteDescriptorSet-descriptorType-00336]
    If descriptorType is VK_DESCRIPTOR_TYPE_STORAGE_IMAGE or VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 
    the imageView member of each element of pImageInfo must have been created with the identity swizzle.
    */
}

struct ShaderGroupHandle {
    uint8_t data[SHADER_GROUP_HANDLE_SIZE];
};

struct HitgCustomData {
    float color[3];
};

/*
In the vulkan spec,
[VUID-vkCmdTraceRaysKHR-stride-03686] pMissShaderBindingTable->stride must be a multiple of VkPhysicalDeviceRayTracingPipelinePropertiesKHR::shaderGroupHandleAlignment
[VUID-vkCmdTraceRaysKHR-stride-03690] pHitShaderBindingTable->stride must be a multiple of VkPhysicalDeviceRayTracingPipelinePropertiesKHR::shaderGroupHandleAlignment
[VUID-vkCmdTraceRaysKHR-pRayGenShaderBindingTable-03682] pRayGenShaderBindingTable->deviceAddress must be a multiple of VkPhysicalDeviceRayTracingPipelinePropertiesKHR::shaderGroupBaseAlignment
[VUID-vkCmdTraceRaysKHR-pMissShaderBindingTable-03685] pMissShaderBindingTable->deviceAddress must be a multiple of VkPhysicalDeviceRayTracingPipelinePropertiesKHR::shaderGroupBaseAlignment
[VUID-vkCmdTraceRaysKHR-pHitShaderBindingTable-03689] pHitShaderBindingTable->deviceAddress must be a multiple of VkPhysicalDeviceRayTracingPipelinePropertiesKHR::shaderGroupBaseAlignment

As shown in the vulkan spec 40.3.1. Indexing Rules,  
    pHitShaderBindingTable->deviceAddress + pHitShaderBindingTable->stride × (
    instanceShaderBindingTableRecordOffset + geometryIndex × sbtRecordStride + sbtRecordOffset )
*/
void createShaderBindingTable() 
{
    auto alignTo = [](auto value, auto alignment) -> decltype(value) {
        return (value + (decltype(value))alignment - 1) & ~((decltype(value))alignment - 1);
    };
    const uint32_t handleSize = SHADER_GROUP_HANDLE_SIZE;
    const uint32_t groupCount = 3; // 1 raygen, 1 miss, 1 hit group
    std::vector<ShaderGroupHandle> handels(groupCount);
    vk.vkGetRayTracingShaderGroupHandlesKHR(vk.device, vk.pipeline, 0, groupCount, handleSize * groupCount, handels.data());
    ShaderGroupHandle rgenHandle = handels[0];
    ShaderGroupHandle missHandle = handels[1];
    ShaderGroupHandle hitgHandle = handels[2];

    const uint32_t rgenStride = alignTo(handleSize, vk.rtProperties.shaderGroupHandleAlignment);
    vk.rgenSbt = { 0, rgenStride, rgenStride };
    
    const uint64_t missOffset = alignTo(vk.rgenSbt.size, vk.rtProperties.shaderGroupBaseAlignment);
    const uint32_t missStride = alignTo(handleSize, vk.rtProperties.shaderGroupHandleAlignment);
    vk.missSbt = { 0, missStride, missStride };

    const uint32_t hitgCustomDataSize = sizeof(HitgCustomData);
    const uint32_t geometryCount = 4;
    const uint64_t hitgOffset = alignTo(missOffset + vk.missSbt.size, vk.rtProperties.shaderGroupBaseAlignment);
    const uint32_t hitgStride = alignTo(handleSize + hitgCustomDataSize, vk.rtProperties.shaderGroupHandleAlignment);
    vk.hitgSbt = { 0, hitgStride, hitgStride * geometryCount };

    const uint64_t sbtSize = hitgOffset + vk.hitgSbt.size;
    std::tie(vk.sbtBuffer, vk.sbtBufferMem) = createBuffer(
        sbtSize,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    auto sbtAddress = getDeviceAddressOf(vk.sbtBuffer);
    if (sbtAddress != alignTo(sbtAddress, vk.rtProperties.shaderGroupBaseAlignment)) {
        throw std::runtime_error("It will not be happened!");
    }
    vk.rgenSbt.deviceAddress = sbtAddress;
    vk.missSbt.deviceAddress = sbtAddress + missOffset;
    vk.hitgSbt.deviceAddress = sbtAddress + hitgOffset;

    uint8_t* dst;
    vkMapMemory(vk.device, vk.sbtBufferMem, 0, sbtSize, 0, (void**)&dst);
    {
        *(ShaderGroupHandle*)dst = rgenHandle; 
        *(ShaderGroupHandle*)(dst + missOffset) = missHandle;

        *(ShaderGroupHandle*)(dst + hitgOffset + 0 * hitgStride             ) = hitgHandle;
        *(HitgCustomData*   )(dst + hitgOffset + 0 * hitgStride + handleSize) = {0.6f, 0.1f, 0.2f}; // Deep Red Wine
        *(ShaderGroupHandle*)(dst + hitgOffset + 1 * hitgStride             ) = hitgHandle;
        *(HitgCustomData*   )(dst + hitgOffset + 1 * hitgStride + handleSize) = {0.1f, 0.8f, 0.4f}; // Emerald Green
        *(ShaderGroupHandle*)(dst + hitgOffset + 2 * hitgStride             ) = hitgHandle;
        *(HitgCustomData*   )(dst + hitgOffset + 2 * hitgStride + handleSize) = {0.9f, 0.7f, 0.1f}; // Golden Yellow
        *(ShaderGroupHandle*)(dst + hitgOffset + 3 * hitgStride             ) = hitgHandle;
        *(HitgCustomData*   )(dst + hitgOffset + 3 * hitgStride + handleSize) = {0.3f, 0.6f, 0.9f}; // Dawn Sky Blue
    }
    vkUnmapMemory(vk.device, vk.sbtBufferMem);
}

void render()
{
    static const VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    static const VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    static const VkImageCopy copyRegion = {
        .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .extent = { WIDTH, HEIGHT, 1 },
    };
    static const VkStridedDeviceAddressRegionKHR callSbt{};

    vkWaitForFences(vk.device, 1, &vk.fence0, VK_TRUE, UINT64_MAX);
    vkResetFences(vk.device, 1, &vk.fence0);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(vk.device, vk.swapChain, UINT64_MAX, vk.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

    vkResetCommandBuffer(vk.commandBuffer, 0);
    if (vkBeginCommandBuffer(vk.commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }
    {
        vkCmdBindPipeline(vk.commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, vk.pipeline);
        vkCmdBindDescriptorSets(
            vk.commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, 
            vk.pipelineLayout, 0, 1, &vk.descriptorSet, 0, 0);

        vk.vkCmdTraceRaysKHR(
            vk.commandBuffer,
            &vk.rgenSbt,
            &vk.missSbt,
            &vk.hitgSbt,
            &callSbt,
            WIDTH, HEIGHT, 1);
        
        setImageLayout(
            vk.commandBuffer,
            vk.outImage,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            subresourceRange);
            
        setImageLayout(
            vk.commandBuffer,
            vk.swapChainImages[imageIndex],
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subresourceRange);
        
        vkCmdCopyImage(
            vk.commandBuffer,
            vk.outImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            vk.swapChainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &copyRegion);

        setImageLayout(
            vk.commandBuffer,
            vk.outImage,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_GENERAL,
            subresourceRange);

        setImageLayout(
            vk.commandBuffer,
            vk.swapChainImages[imageIndex],
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            subresourceRange);
    }
    if (vkEndCommandBuffer(vk.commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }

    VkSemaphore waitSemaphores[] = { 
        vk.imageAvailableSemaphore 
    };
    VkPipelineStageFlags waitStages[] = { 
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT 
    };
    
    VkSubmitInfo submitInfo{ 
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = sizeof(waitSemaphores) / sizeof(waitSemaphores[0]),
        .pWaitSemaphores = waitSemaphores,
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &vk.commandBuffer,
    };
    
    if (vkQueueSubmit(vk.graphicsQueue, 1, &submitInfo, vk.fence0) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }
    
    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .swapchainCount = 1,
        .pSwapchains = &vk.swapChain,
        .pImageIndices = &imageIndex,
    };
    
    vkQueuePresentKHR(vk.graphicsQueue, &presentInfo);
}

int main()
{
    glfwInit();
    GLFWwindow* window = createWindow();
    createVkInstance(window);
    createVkDevice();
    loadDeviceExtensionFunctions(vk.device);
    createSwapChain();
    createCommandCenter();
    createSyncObjects();

    geo.readfile("box.obj");
    createBLAS();
    createTLAS();
    createOutImage();
    createUniformBuffer();
    createRayTracingPipeline();
    createDescriptorSets();
    createShaderBindingTable();

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        render();
    }

    vkDeviceWaitIdle(vk.device);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}