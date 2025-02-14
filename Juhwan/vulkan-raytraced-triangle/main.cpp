#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <filesystem>
#include <tuple>
#include <bitset>
#include <span>
#include "shader_module.h"

typedef unsigned int uint;

const uint32_t WIDTH = 1200;
const uint32_t HEIGHT = 800;

#ifdef NDEBUG
    const bool ON_DEBUG = false;
#else
    const bool ON_DEBUG = true;
#endif


struct Global {             // �� �Ʒ��� 6�� �Լ��� ��� ���̺귯���� ������ �Ǿ� �ֱ⿡ �ߺ������� �����ϱ� ���� �̷��� struct �ȿ� ��ġ���״�.
    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;                    // Extension �� �ش��ϴ� �Լ��� ( ����̹����� �����´�. )
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;

    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;

    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice;
    VkDevice device;

    VkQueue graphicsQueue; // assume allowing graphics and present
    uint queueFamilyIndex;

    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
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
    VkDeviceAddress tlasAddress;

    ~Global() {
        vkDestroyBuffer(device, tlasBuffer, nullptr);
        vkFreeMemory(device, tlasBufferMem, nullptr);
        vkDestroyAccelerationStructureKHR(device, tlas, nullptr);

        vkDestroyBuffer(device, blasBuffer, nullptr);
        vkFreeMemory(device, blasBufferMem, nullptr);
        vkDestroyAccelerationStructureKHR(device, blas, nullptr);

        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroyFence(device, fence0, nullptr);

        vkDestroyCommandPool(device, commandPool, nullptr);

        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }
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

bool checkValidationLayerSupport(std::vector<const char*>& reqestNames) {
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

bool checkDeviceExtensionSupport(VkPhysicalDevice device, std::vector<const char*>& reqestNames) {
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
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,            // �ش� extension �� �߰��ؼ� �̸� �����ϴ� device �� ã�´ٰ� �ٷ� ����� �� �ִ� ���� �ƴϴ�.
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,         //  ���Ŀ�, extension �� ���� ���� ������ �ݵ�� �ʿ��ϴ�.
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
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

	VkPhysicalDeviceAccelerationStructureFeaturesKHR f1{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .accelerationStructure = VK_TRUE,       // VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME extension �� ���� ���� ������ �ʿ��ѵ�, 
    };                                          //  , accelerationStructure �� true �� �����ؾ� ��μ� �ش� extension �� ����� �� �ִ�.

	VkPhysicalDeviceBufferDeviceAddressFeatures f2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
        .bufferDeviceAddress = VK_TRUE,         // VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME extension ���� �׷���.
    };

    createInfo.pNext = &f1;         // createInfo �� Next Chain ���� �־��־�� �Ѵ�. ( Linked List �ڷᱸ�� ) 
    f1.pNext = &f2;                 //  f2 ���� �׷���. ( �� ������ �翬�� null �̴�. )

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
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
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

        VkImageView imageView;
        if (vkCreateImageView(vk.device, &createInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image views!");
        }
        vk.swapChainImageViews.push_back(imageView);
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

    uint memTypeIndex = 0;
    {
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(vk.device, buffer, &memRequirements);
        size = memRequirements.size;
        std::bitset<32> isSuppoted(memRequirements.memoryTypeBits);

        VkPhysicalDeviceMemoryProperties spec;
        vkGetPhysicalDeviceMemoryProperties(vk.physicalDevice, &spec);

        for (auto& [props, _] : std::span<VkMemoryType>(spec.memoryTypes, spec.memoryTypeCount)) {
            if (isSuppoted[memTypeIndex] && (props & reqMemProps) == reqMemProps) {
                break;
            }
            ++memTypeIndex;
        }
    }

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = size,
        .memoryTypeIndex = memTypeIndex,
    };

    if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        VkMemoryAllocateFlagsInfo flagsInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
            .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR,     // ���� createBuffer �Լ����� VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT flag ������ ����
        };                                                          //  �̷��� flagsInfo �� ����� allocInfo �� Next Chain ���� �������ش�.
        allocInfo.pNext = &flagsInfo;
    }

    if (vkAllocateMemory(vk.device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate vertex buffer memory!");
    }

    vkBindBufferMemory(vk.device, buffer, bufferMemory, 0);

    return { buffer, bufferMemory };
}

inline VkDeviceAddress getDeviceAddressOf(VkBuffer buffer)                  // Extension �Լ��� vkGetBufferDeviceAddressKHR �� ����
{                                                                           //  buffer �� Device �ּ� (GPU �� �Ҵ�� �޸� �ּ�) �� ������ �� �ִ�.
    VkBufferDeviceAddressInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer,
    };
    return vk.vkGetBufferDeviceAddressKHR(vk.device, &info);
}

inline VkDeviceAddress getDeviceAddressOf(VkAccelerationStructureKHR as)    // GPU �� �Ҵ�� Acceleration Structure Device �� �޸� �ּҸ� Extension �Լ��� ���� �����´�.
{
    VkAccelerationStructureDeviceAddressInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = as,
    };
    return vk.vkGetAccelerationStructureDeviceAddressKHR(vk.device, &info);
}

void createBLAS()
{
    float vertices[][3] = {         // 4 ���� Vertices
        { -1.0f, -1.0f, 0.0f },     // Ray Tracing ������ 2 ���� Vertex �� �Ұ���.
        {  1.0f, -1.0f, 0.0f },
        {  1.0f,  1.0f, 0.0f },
        { -1.0f,  1.0f, 0.0f },
    };
    uint32_t indices[] = { 0, 1, 3, 1, 2, 3 };

    VkTransformMatrixKHR geoTransforms[] = {
        {
            1.0f, 0.0f, 0.0f, -2.0f,            // �������� 2 ��ŭ �̵�
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f
        }, 
        {
            1.0f, 0.0f, 0.0f, 2.0f,            // ���������� 2 ��ŭ �̵�
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f
        },
    };

    auto [vertexBuffer, vertexBufferMem] = createBuffer(        // Vertex, Index, Geometry Buffer ����
        sizeof(vertices),           // GPU ���� �� Buffer �� �ּҸ� �˾ƾ� �ϱ� ������ VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT flag �� ����Ѵ�.
             // VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR flag �� �� Buffer ���� Ray Tracing �� ���� ������ Input ���� �ۿ��ϴµ�, GPU �� Read Only ���ٸ� �����ϰ� �ϴ� flag �̴�.
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    auto [indexBuffer, indexBufferMem] = createBuffer(
        sizeof(indices), 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    auto [geoTransformBuffer, geoTransformBufferMem] = createBuffer(
        sizeof(geoTransforms), 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    void* dst;

    vkMapMemory(vk.device, vertexBufferMem, 0, sizeof(vertices), 0, &dst);
    memcpy(dst, vertices, sizeof(vertices));
    vkUnmapMemory(vk.device, vertexBufferMem);

    vkMapMemory(vk.device, indexBufferMem, 0, sizeof(indices), 0, &dst);
    memcpy(dst, indices, sizeof(indices));
    vkUnmapMemory(vk.device, indexBufferMem);

    vkMapMemory(vk.device, geoTransformBufferMem, 0, sizeof(geoTransforms), 0, &dst);
    memcpy(dst, geoTransforms, sizeof(geoTransforms));
    vkUnmapMemory(vk.device, geoTransformBufferMem);

    VkAccelerationStructureGeometryKHR geometry0{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,     // Geometry �� ����� �� (BLAS ������) �� VK_GEOMETRY_TYPE_TRIANGLES_KHR type �� ����Ѵ�.
        .geometry = {                                           // �ٸ� type �� �� VK_GEOMETRY_TYPE_AABBS_KHR �� �ִµ�, �̴� Intersection Shader �� ����� �� ����Ѵ�.
                                                            // �߰���, TLAS ������ VK_GEOMETRY_TYPE_INSTANCES_KHR �� ����Ѵ�.
        
            .triangles = {                          // BLAS �̱� ������ VkAccelerationStructureGeometryDataKHR �� geometry �� triangles �� ������ �־��ش�.

                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,                                         // 3 ������ ����ϱ⿡ RGB ��� ���.
                .vertexData = { .deviceAddress = getDeviceAddressOf(vertexBuffer) },                // GPU �� �Ҵ�� �޸� �ּ� (uint64_t) �� ������.
                .vertexStride = sizeof(vertices[0]),
                .maxVertex = sizeof(vertices) / sizeof(vertices[0]) - 1,                            // �ִ� Index Number �� �������ֱ� ������, �������� -1 ���ش�. (�ݵ�� -1 �ؾ��ϴ� ���� �ƴϴ�.)
                .indexType = VK_INDEX_TYPE_UINT32,                                                  // Indices �迭�� �� ��Ұ� uint32_t �̱� ������ �̷��� ����.
                .indexData = { .deviceAddress = getDeviceAddressOf(indexBuffer) },
                .transformData = { .deviceAddress = getDeviceAddressOf(geoTransformBuffer) },
            },
        },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,        // ������ �������� ������Ʈ�� �����ϵ��� ������. ( Ray �� ������ X )
    };
    VkAccelerationStructureGeometryKHR geometries[] = { geometry0, geometry0 }; // ������ Geometry (�簢��) 2 ���� �־���.

    uint32_t triangleCount0 = sizeof(indices) / (sizeof(indices[0]) * 3);   // �ϳ��� Geometry ���� triangle 2 ���� ������.
    uint32_t triangleCounts[] = { triangleCount0, triangleCount0 };         // Geometry ���� �ش��ϴ� triangle ������ ����־���.

    VkAccelerationStructureBuildGeometryInfoKHR buildBlasInfo{                      // BLAS �� Build �ϱ����� ������ ( ����, Prebuild �� ��ģ �� ���߿� �������� Build �� ��ģ��. )
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,  // buildBlasInfo ������ �ʿ��� �������� ������, �� �� �Ϻθ� �̸� �����صξ���.
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,                    //  �̴� Prebuild �����̱⿡ ������ �����ϰ� �����ϴ� ���̴�.
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .geometryCount = sizeof(geometries) / sizeof(geometries[0]),
        .pGeometries = geometries,
    };
    
    VkAccelerationStructureBuildSizesInfoKHR requiredSize{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    vk.vkGetAccelerationStructureBuildSizesKHR(             // vkGetAccelerationStructureBuildSizesKHR �Լ��� Prebuild �ϱ� ���� �ʿ��� Extension �Լ��̴�.
        vk.device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,    // Build �� CPU ���� �� ������ GPU ���� �� ������ �������� �� �ִµ�, 
        &buildBlasInfo,                                     //  VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR �� GPU ���� Build �ϱ� ���� �����̴�.
        triangleCounts,                                     // ( Vulkan �� DirectX �� �ٸ��� Acceleration Structure Build ������ CPU ���� ������ �� �ִ�. (DirectX �� ������ GPU �󿡼� ����) )
        
        &requiredSize);             // ���߿� �������� Build �� �� ���ε�, �� �� �ʿ��� �뷮�� �̸� Ȯ���ϴ� ���̴�. ( Prebuild ���� )

    std::tie(vk.blasBuffer, vk.blasBufferMem) = createBuffer(           // BLAS Buffer �� ������ ��� ������ �����ϱ⿡ ���������� ���ȴ�.
        requiredSize.accelerationStructureSize,                     // BLAS �� ����Ǿ� �ִ� Buffer �� ũ��
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);                           // BLAS Buffer �� Scratch Buffer �� GPU �� �˾Ƽ� ó���ϱ� ������ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT �� ����Ѵ�.

    auto [scratchBuffer, scratchBufferMem] = createBuffer(              // scratchBuffer �� BLAS Buffer Build �� ������ �����ϱ� ������ �̿� ���� ���������� ������ ���̴�.
        requiredSize.buildScratchSize,                              // Build �������� �߻��ϴ� �߰����� Buffer ũ��
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Generate BLAS handle
    {
        VkAccelerationStructureCreateInfoKHR asCreateInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = vk.blasBuffer,
            .size = requiredSize.accelerationStructureSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,            // BLAS ���� �˷��ִ� Bottom Level Ÿ�� ( �ռ� buildBlasInfo �� ���� ���� ������ Ÿ���� �־��־���. )
        };
        vk.vkCreateAccelerationStructureKHR(vk.device, &asCreateInfo, nullptr, &vk.blas);   // ������ BLAS �� �� Buffer �� vk.blasBuffer �� �ٷ�� �ڵ��� vk.blas �� ����� ��

        vk.blasAddress = getDeviceAddressOf(vk.blas);       // vk.blas �� �ش��ϴ� Acceleration Structure Device �� �޸� �ּҸ� �޾ƿ�.
    }                                                           // ���� �������� BLAS Build �� �����Ѵ�.

    // Build BLAS using GPU operations
    {
        VkCommandBufferBeginInfo beginInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkBeginCommandBuffer(vk.commandBuffer, &beginInfo);                                 // �������� BLAS Build ����
        {
            buildBlasInfo.dstAccelerationStructure = vk.blas;
            buildBlasInfo.scratchData.deviceAddress = getDeviceAddressOf(scratchBuffer);
            VkAccelerationStructureBuildRangeInfoKHR buildBlasRangeInfo[] = {   // ������ buildBlasInfo.pGeometries �� 2 ���� geometry0 �� �־����Ƿ�,
                {                                                               //  buildBlasRangeInfo �� 2 ���� geometry0 ������ ���� ������ �־��־�� �Ѵ�.
                
                    .primitiveCount = triangleCounts[0],            // �� Transformation �� ���Ǵ� triangle ����
                    .transformOffset = 0,
                },
                { 
                    .primitiveCount = triangleCounts[1],
                    .transformOffset = sizeof(geoTransforms[0]),    // ���� Transformation �� ��Ÿ���� ������ Offset �� ����
                }
            };

            VkAccelerationStructureBuildGeometryInfoKHR buildBlasInfos[] = { buildBlasInfo };           // buildBlasInfos �迭�� �����
            VkAccelerationStructureBuildRangeInfoKHR* buildBlasRangeInfos[] = { buildBlasRangeInfo };   //  buildBlasRangeInfos �迭�� �����.
            vk.vkCmdBuildAccelerationStructuresKHR(vk.commandBuffer, 1, buildBlasInfos, buildBlasRangeInfos);   // �ش� �Լ����� Structures �� �߰��� �� �ִµ�,
                                                                                                                //  �̴� ���� ���� BLAS ���� ���ÿ� ���� �� ������ �ǹ��Ѵ�.
            // vkCmdBuildAccelerationStructuresKHR(vk.commandBuffer, 1, &buildBlasInfo, 
            // (const VkAccelerationStructureBuildRangeInfoKHR *const *)&buildBlasRangeInfo);
        }
        vkEndCommandBuffer(vk.commandBuffer);

        VkSubmitInfo submitInfo {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &vk.commandBuffer,
        }; 
        vkQueueSubmit(vk.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);        // BLAS ����� ����� GPU �� ����
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
    VkTransformMatrixKHR insTransforms[] = {    // Transformation 2 ���� �־���. ������, �̹����� �̸��� insTransforms ( Instance Transformation ) �̴�.
        {                                       // BLAS ������ geoTransforms �̾��µ� �̴� Geometry ���� ������ �ִ� Transformation ������ �ǹ��ϰ�,
            1.0f, 0.0f, 0.0f, 0.0f,             //  TLAS �� insTransforms �� Instance ���� ����Ǵ� Transformation ������ �ǹ��Ѵ�.
            0.0f, 1.0f, 0.0f, 2.0f,
            0.0f, 0.0f, 1.0f, 0.0f          // ��, ���⿡���� 2 ���� Instance �� ����� �׿� ���� ������ Transformation �� �����.
        }, 
        {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, -2.0f,
            0.0f, 0.0f, 1.0f, 0.0f
        },
    };

    VkAccelerationStructureInstanceKHR instance0 {
        .mask = 0xFF,                                   // mask �� ��κ� 0xFF �� ���
        .instanceShaderBindingTableRecordOffset = 0,        // ���߿� Shader Binding Table ���� ���Ǵ� ����
        .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,     // Back Face Culling �� �������� �ʰڴٴ� �ǹ��� flag
        .accelerationStructureReference = vk.blasAddress,   // Instance �� BLAS �� �����Ѵ�.
    };
    VkAccelerationStructureInstanceKHR instanceData[] = { instance0, instance0 };
    instanceData[0].transform = insTransforms[0];
    instanceData[1].transform = insTransforms[1];

    auto [instanceBuffer, instanceBufferMem] = createBuffer(        // instanceData �� ���� Buffer �� ����.
        sizeof(instanceData), 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* dst;
    vkMapMemory(vk.device, instanceBufferMem, 0, sizeof(instanceData), 0, &dst);
    memcpy(dst, instanceData, sizeof(instanceData));
    vkUnmapMemory(vk.device, instanceBufferMem);

    VkAccelerationStructureGeometryKHR instances{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,         // BLAS ������ VK_GEOMETRY_TYPE_TRIANGLES_KHR �� ����ϴ� �ݸ�,
                                                                //  TLAS ������ type �� VK_GEOMETRY_TYPE_INSTANCES_KHR �� ����Ѵ�.
        .geometry = {           // geometry union �� ä���
            .instances = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                .data = { .deviceAddress = getDeviceAddressOf(instanceBuffer) },    // Instance Buffer �� �ּҷ� ����
            },
        },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
    };
                                                    // BLAS �ϳ��� �������� Geometry �� �� �� �ְ�, ������ Geometry ���� ���� ���� Triangles �� �� �� �ִ�.
    uint32_t instanceCount = 2;                     // TALS ���� �ϳ��� Geometry �� �� �� �ְ�, �� Geometry �� ���� ���� Instance ���� �����̱⵵ �ϴ�.

    VkAccelerationStructureBuildGeometryInfoKHR buildTlasInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .geometryCount = 1,     // It must be 1 with .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR as shown in the vulkan spec.
        .pGeometries = &instances,      // ���� TLAS ���� geometry �� 1 ���̹Ƿ� geometryCount �� 1 �̾�� �Ѵ�.
    };

    VkAccelerationStructureBuildSizesInfoKHR requiredSize{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};   // Prebuild ����
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

        vk.tlasAddress = getDeviceAddressOf(vk.tlas);
    }

    // Build TLAS using GPU operations
    {
        VkCommandBufferBeginInfo beginInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};       // �������� TLAS Build ����
        vkBeginCommandBuffer(vk.commandBuffer, &beginInfo);
        {
            buildTlasInfo.dstAccelerationStructure = vk.tlas;
            buildTlasInfo.scratchData.deviceAddress = getDeviceAddressOf(scratchBuffer);

            VkAccelerationStructureBuildRangeInfoKHR buildTlasRangeInfo = { .primitiveCount = instanceCount };  // TLAS �� Geometry �� 1 �����̱⿡ ���� �迭�� ������ �ʾҴ�.
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

void loadDeviceExtensionFunctions(VkDevice device)      // ����̹����� ����� Ȯ�� �Լ��� �����͸� �������� �����´�. �ش� ������ device �� ������ �� ����Ǿ�� �Ѵ�.
{
    vk.vkGetBufferDeviceAddressKHR = (PFN_vkGetBufferDeviceAddressKHR)(vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR"));
                                        // device �� ������ �ܺ� ����̹����� vkGetBufferDeviceAddressKHR �Լ��� ã�� �� �ش� �Լ��� �����͸�
                                        //  vk.vkGetBufferDeviceAddressKHR �Լ� �����Ϳ� �����Ѵ�.

    vk.vkGetAccelerationStructureDeviceAddressKHR = (PFN_vkGetAccelerationStructureDeviceAddressKHR)(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR"));
    vk.vkCreateAccelerationStructureKHR = (PFN_vkCreateAccelerationStructureKHR)(vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
    vk.vkDestroyAccelerationStructureKHR = (PFN_vkDestroyAccelerationStructureKHR)(vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));
    vk.vkGetAccelerationStructureBuildSizesKHR = (PFN_vkGetAccelerationStructureBuildSizesKHR)(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR"));
    vk.vkCmdBuildAccelerationStructuresKHR = (PFN_vkCmdBuildAccelerationStructuresKHR)(vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR"));
}

int main()
{
    glfwInit();
    GLFWwindow* window = createWindow();
    createVkInstance(window);
    createVkDevice();
    loadDeviceExtensionFunctions(vk.device);
    createSwapChain();
    
    // createRenderPass();          // BLAS �� TLAS �� ������, RenderPass �� Pipeline �� �ʿ��������.
    // createGraphicsPipeline();
    createCommandCenter();
    createSyncObjects(); 

    createBLAS();       // Geometry ��� ����   ( BLAS (Bottom-Level Acceleration Structure) )
    createTLAS();       // Instance ��� ����   ( TLAS (Top-Level Acceleration Structure) )

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        //render();
    }

    vkDeviceWaitIdle(vk.device);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}