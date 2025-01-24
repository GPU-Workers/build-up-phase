#define GLFW_INCLUDE_VULKAN         // Vulkan �� ��� ���ɼ��� ���ο� �ΰ� ���������.
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <fstream>
//#include "glsl2spv.h"

typedef unsigned int uint;

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

#ifdef NDEBUG                   // ���� Debug ����� ���� ON_DEBUG �� True �� (�ڵ����� T, F ������)
const bool ON_DEBUG = false;
#else
const bool ON_DEBUG = true;
#endif

struct Global {
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;

    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice;            // Physical Device
    VkDevice device;                            // Logical Device

    VkQueue graphicsQueue; // assume allowing graphics and present
    uint queueFamilyIndex;

    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    const VkFormat swapChainImageFormat = VK_FORMAT_B8G8R8A8_SRGB;    // intentionally chosen to match a specific format ( �Ϲ������� ���Ǵ� �����̱⿡ �ǵ������� ���õ� )
    const VkExtent2D swapChainImageExtent = { .width = WIDTH, .height = HEIGHT };

    VkRenderPass renderPass;
    std::vector<VkFramebuffer> framebuffers;

    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;

    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;

    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;

    ~Global() {
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroyFence(device, inFlightFence, nullptr);

        vkDestroyCommandPool(device, commandPool, nullptr);

        for (auto framebuffer : framebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }

        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyRenderPass(device, renderPass, nullptr);

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
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,     // �޽��� ���ݵ�
    VkDebugUtilsMessageTypeFlagsEXT messageType,                // �޽��� Ÿ��
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, 
    void* pUserData) 
{
    const char* severity;
    switch (messageSeverity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: severity = "[Verbose]"; break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: severity = "[Warning]"; break;    // Warning �޽��� Ȱ��ȭ. 
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: severity = "[Error]"; break;        // Error �޽��� Ȱ��ȭ.
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

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

GLFWwindow* createWindow()
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);                             // â ũ�� ���� �Ұ�
    return glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);     // â ũ�� �� â �̸� �� ����
}

void createVkInstance(GLFWwindow* window)
{
    VkApplicationInfo appInfo{                          // ���ø����̼��� �̸� �Ǵ� �������׵�
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Hello Triangle",
        .apiVersion = VK_API_VERSION_1_0
    };

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);       // Instance Extension : Vulkan �ν��Ͻ��� �ʱ�ȭ�� �� �ʿ��� �߰� ����� ����
                                                                                                // �ʿ��� Extension ���� �÷������� �ٸ��� ������ GLFW �� ���� ���� �÷����� �´� Extension �� �޾ƿ�
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);   // Vector �����̳ʷ� ������ -> Debugger ����� ��� Extension �� �ϳ� �� �߰��ϱ� ����.
    if(ON_DEBUG) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);                       // Debugger ����� ���, �ʼ����� Instance Extension �� �߰���

    std::vector<const char*> validationLayers;
    if(ON_DEBUG) validationLayers.push_back("VK_LAYER_KHRONOS_validation");                     // Debugger ����� ���, Instance Layer �� �߰���
    if (!checkValidationLayerSupport(validationLayers)) {                                       // ���� Vulkan �� �ش� Layer �� �����ϴ��� �� �� �ִ� �Լ�
        throw std::runtime_error("validation layers requested, but not available!");
    }
                                                                                                // Ctrl + F5 => ������ �־ ����� ��� â�� ��� �Ǵ� ���� �޽����� ��
                                                                                                // F5        => ����� ��� â�� ��� �Ǵ� ���� �޽����� ��

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{                                         // Debug Layer �� Ȱ��ȭ�Ǿ����� ������ �۵��ϴµ�, ���� �޽������� ����� ���â�� �ƴ� �ܼ�â�� �����.
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,                       // �׳� ����ϸ� �������� �޽����� ���� ���� �ö���⿡, ���ϴ� ������ �޽����� �ö������ Ŀ���͸���¡�� ����.
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,     // Warning �޽����� Error �޽����� ���� ��. ( �������� ���� )
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debugCallback,       // ������ �߻����� ��, �޽����� ����ϴµ� debugCallback �Լ��� �������� ������.
    };

    VkInstanceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = ON_DEBUG ? &debugCreateInfo : nullptr,         // �� ���� �ּ�ó���ϸ� ������ �ش� ���κ����� nullptr �� �ȴ�.
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = (uint)validationLayers.size(),
        .ppEnabledLayerNames = validationLayers.data(),
        .enabledExtensionCount = (uint)extensions.size(),       // Extension �� �������� �ʴ´ٸ� ���Ŀ� Instance �� �����ϴ� �������� �����ϰ� �� ���̴�.
        .ppEnabledExtensionNames = extensions.data(),
    };

    if (vkCreateInstance(&createInfo, nullptr, &vk.instance) != VK_SUCCESS) {       // Instance ����
        throw std::runtime_error("failed to create instance!");
    }

    if (ON_DEBUG) {                                                                 // vkCreateDebugUtilsMessengerEXT ��� �Լ��� ȣ���� �ؾ� �ܼ� â�� ����� �޽����� Ȱ��ȭ�ȴ�.
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vk.instance, "vkCreateDebugUtilsMessengerEXT");   // EXT �� ������ �Լ��� Ȯ���Լ��ε�, Ȯ���Լ��� �⺻������ �������� �ʾ� �Լ� �����͸� �޶�� ��û�ؾ� ��.
        if (!func || func(vk.instance, &debugCreateInfo, nullptr, &vk.debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    if (glfwCreateWindowSurface(vk.instance, window, nullptr, &vk.surface) != VK_SUCCESS) {     // Surface : Swap Chain �� ���õ� Instance �Ӽ� ( Instance ���� �� ������ �Ѵ�. )
        throw std::runtime_error("failed to create window surface!");                           // Surface ���� �÷������� �ٸ��⿡ GLFW �� ������ �޴´�.
    }
}

void createVkDevice()
{
    vk.physicalDevice = VK_NULL_HANDLE;
                                                                        // Count �� ���� ���ϰ� �ش� Count ��ŭ �迭�� ���� �� �����ּҵ��� �ش� �迭�鿡 �ִ� ����
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(vk.instance, &deviceCount, nullptr);         // Physical Device, ��, GPU �� ã�� �� ������ deviceCount �� ����.
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(vk.instance, &deviceCount, devices.data());  // Physical Device �� �ڵ���� device.data() �� ���� ��. ( Logical Device �� ����� �� �۾� )

    std::vector<const char*> extentions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };      // Device Extension �����. ( "VK_KHR_swapchain" ��� Extension �� �ʼ����� )

    for (const auto& device : devices) 
    {
        if (checkDeviceExtensionSupport(device, extentions))            // ������ Device �� (�׷���ī���) �� �ش� Extension �� �����ϴ��� �˾Ƴ�.
        {
            vk.physicalDevice = device;                                     // �����ϸ� Physical Device (GPU) �� �ش� �׷���ī��� ����
            break;
        }
    }

    if (vk.physicalDevice == VK_NULL_HANDLE) {                          // ��� GPU �� Extension �� �������� ������ ���α׷��� �����.
        throw std::runtime_error("failed to find a suitable GPU!");
    }

    uint32_t queueFamilyCount = 0;                                      // ��ɵ��� ���������� ����ִ� Command Queue �� Family ����
    vkGetPhysicalDeviceQueueFamilyProperties(vk.physicalDevice, &queueFamilyCount, nullptr);    // �׷���ī�帶�� �����ϴ� Queue ���� ���� �� �ٸ��⿡ Queue ������ �����´�.
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);                       // Queue Family ������ ���� ���� Queue �� �ִ�.
    vkGetPhysicalDeviceQueueFamilyProperties(vk.physicalDevice, &queueFamilyCount, queueFamilies.data());       // Queue ���� Compute ���� Queue �� Transfer ���� Queue ���� �ִ�.
                                                                                                                // ���� Queue �� �׷��� ���, Compute, Transfer �� ���� ������ ������ �� �ִ�.
    vk.queueFamilyIndex = 0;                                                                                // ���� Queue �� ����ϱ� ���� ���� Queue �� ã�ƾ� �Ѵ�.
    {
        for (; vk.queueFamilyIndex < queueFamilyCount; ++vk.queueFamilyIndex)
        {
            VkBool32 presentSupport = false;                                                    // Present : ������ �� ȭ�鿡 presentation �� �ϴ� Swap Chain �� ��� ���� Queue �� ���� ����Ǵµ�, �̷��� presentation �� �����ϴ� Queue ������ ���� ����
            vkGetPhysicalDeviceSurfaceSupportKHR(vk.physicalDevice, vk.queueFamilyIndex, vk.surface, &presentSupport);  // �� GPU �� ���� QueueFamily �� Present �� �����ϴ� �� �˾Ƴ�.

            if (queueFamilies[vk.queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT && presentSupport)    // Queue Flag �� �ش� Queue �� � ��ɿ� ���� Ȱ��ȭ�Ǿ��ִ��� �� �� �ִ�.
                break;                                                                                      // �׷��� ��� ����� presentSupport �� �����ϴ� ���� Queue �� ã���� for �� ����.
        }

        if (vk.queueFamilyIndex >=  queueFamilyCount)  
            throw std::runtime_error("failed to find a graphics & present queue!");
    }
    float queuePriority = 1.0f;

    VkDeviceQueueCreateInfo queueCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = vk.queueFamilyIndex,
        .queueCount = 1,                                            // Queue Family �ȿ� 2�� �̻��� Queue �� ���� �� �־ 1�� �����ؾ� �Ѵ�.
        .pQueuePriorities = &queuePriority,
    };

    VkDeviceCreateInfo createInfo{                                  // Logical Device �� ����� ���� �۾� (�������� �� �͵��� �� �׷���ī���� ��� �� ��� ��ɱ��� ����� �� ���ϰ� ���� ���̴�.)
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledExtensionCount = (uint)extentions.size(),
        .ppEnabledExtensionNames = extentions.data(),
    };

    if (vkCreateDevice(vk.physicalDevice, &createInfo, nullptr, &vk.device) != VK_SUCCESS) {    // Logical Device : Physical Device �� � ������ ����� �� ���ϱ� ���� �������� �������̽��� ���� ��
        throw std::runtime_error("failed to create logical device!");                           // vk.device ������ Queue �� ������ ���ӽ�Ų��. 
    }

    vkGetDeviceQueue(vk.device, vk.queueFamilyIndex, 0, &vk.graphicsQueue);
}

void createSwapChain()                                                  // Swap Chain �� Window ���̿� Surface ��� �������̽��� �ִ�.
{                                                                       // Swap Chain �� Surface �� ���� Window �� ȭ���� ����Ѵ�. Vulkan ������ -> Swap Chain -> Surface -> Window ���
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk.physicalDevice, vk.surface, &capabilities); // ������ �׷���ī�尡 Surface �� ���� ������ ��밡������ �� ������ capabilities �� �־���. 
    
    const VkColorSpaceKHR defaultSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;     // Swap Chain ������ VK_FORMAT_B8G8R8A8_SRGB �� �ƴϸ� �ش� VK_COLOR_SPACE_SRGB_NONLINEAR_KHR �� ��� �� ��.
    {
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(vk.physicalDevice, vk.surface, &formatCount, nullptr);         // ���� �׷���ī�忡�� Surface �� ����ϴ� Format �� ���� ������ ������.
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(vk.physicalDevice, vk.surface, &formatCount, formats.data());      // Vector �� �����ϴ� ���˵��� ����־� �����ϰ�
                                                                                                                //  �ǵ������� ���� Swap Chain �����̰� defaultSpace �� ��� �������� Ȯ�εǸ�
        bool found = false;                                                                                     //  Swap Chain Format ���δ� B8G8R8A8_SRGB �� ����ϰ�
        for (auto format : formats) {                                                                           //  Default Color Space �δ� SRGB_NONLINEAR �� ����ϰ� �ȴ�.
            if (format.format == vk.swapChainImageFormat && format.colorSpace == defaultSpace) {
                found = true;
                break;
            }
        }

        if (!found) {                               // Format �� Presentation Mode �� ���� : Presentation Mode �� Swap Chain �� Performance �� ���õ� ����
            throw std::runtime_error("");           //                                      Format �� �� ��ü�� �ٲ�� �Ǹ� ��ü���� ������ �ٲ��. Format �� �ٲ�� ������ �ڵ� ��ΰ� ������ �ٲ��� �Ѵ�.
        }
    }

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;            // Presentation Mode �� VK_PRESENT_MODE_MAILBOX_KHR �� ���� �ȵǸ� VK_PRESENT_MODE_FIFO_KHR �� �״�� ���.
    {
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(vk.physicalDevice, vk.surface, &presentModeCount, nullptr);       // Swap Chain ���� Surface �� �����ϴ� Presentation Mode �� ������ �ִ���
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);                                               //  vector �� �����ϰ� Mode �� VK_PRESENT_MODE_MAILBOX_KHR �� �����Ǹ� ���.
        vkGetPhysicalDeviceSurfacePresentModesKHR(vk.physicalDevice, vk.surface, &presentModeCount, presentModes.data());

        for (auto mode : presentModes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {                              // VK_PRESENT_MODE_MAILBOX_KHR �� ���� ���α�ɵ��� ��� �׷���ī�忡�� �����ϴ� ���� �ƴϱ� ������
                presentMode = VK_PRESENT_MODE_MAILBOX_KHR;                          //  �ݵ�� �ش� �׷���ī�忡�� �����ϴ��� ���� �˾ƺ��� ����ؾ�, ���߿� ȣȯ�� ������ ���������� �ʴ´�.
                break;
            }
        }
    }

    uint imageCount = capabilities.minImageCount + 1;
    VkSwapchainCreateInfoKHR createInfo{                                        // Swap Chain �� �����ϱ� ���� ������
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = vk.surface,
        .minImageCount = imageCount,                                // imageCount �� 3 �� ������ ��������� �̹���(����) ������ �ּ� 3���̴�.
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

    vkGetSwapchainImagesKHR(vk.device, vk.swapChain, &imageCount, nullptr);                     // swapChainImages �� ũ�⸦ imageCount ��ŭ���� ����
    vk.swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(vk.device, vk.swapChain, &imageCount, vk.swapChainImages.data());   // swapChainImages �� �̹� ���������� ������� �ִµ�, �װ��� �������� ��.

    for (const auto& image : vk.swapChainImages) {
        VkImageViewCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = vk.swapChainImageFormat,                  // Swap Chain �������� Fotmat �� �����ϰ� �Ѵ�.
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };

        VkImageView imageView;
        if (vkCreateImageView(vk.device, &createInfo, nullptr, &imageView) != VK_SUCCESS) {     // swapChainImages �� ���� Image View �� ����
            throw std::runtime_error("failed to create image views!");
        }
        vk.swapChainImageViews.push_back(imageView);
    }
}

void createRenderPass()                                                 // RenderPass �� ���� ���� Subpass ��� �̷���� �� �ִ�.
{
    VkAttachmentDescription colorAttachment{                            // ����� Color Attachment �� �ϳ��ۿ� �� ���⿡ colorAttachment �迭�� 0 �� �ε����� �ۼ��Ͽ���.
                                                                        // ���� �� �κ�(��)������ Render Target �� ���ҽ��� bind �������� �ʾҴ�. �ش� �۾��� ���Ŀ� ����� ���̴�.
        .format = vk.swapChainImageFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,                          // VK_ATTACHMENT_LOAD_OP_CLEAR �� Shader �� ȣ���ϱ� ������ �ȼ��� clear color �� �� ����� ����
                                                                        //  VK_ATTACHMENT_LOAD_OP_LOAD �� ������ �ȼ� color �� ������ �ʰ� ���ο� ������ ����� �����ϴ� ���� Ȱ�밡���� ����
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,                        // VK_ATTACHMENT_STORE_OP_STORE �� Shader �� ȣ���ϱ� ���Ŀ� �������� color �� �����ϵ��� ����
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference colorAttachmentRef0{                                  // ���� colorAttachment �� �ϳ����̱� ������ 0 �� index �̴�.
        .attachment = 0,                                                // Attachment �� Render Target �� ���� ������ �ǹ��Ѵ�. 0 �� �ǹ̴� ���� colorAttachment �� Reference �̴�.
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,             // Color ��¸� �ϱ⿡ VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL �� layout �� ��å�Ѵ�.
    };

    VkSubpassDescription subpass{                                               // fragment Shader �� �ƿ�ǲ (Render Target)�� 1�����̱� ������ colorAttachmentCount �� 1�� �����ش�.
        .colorAttachmentCount = 1,                                      // Render Target, ��, Shader �� ȣ��Ǿ��� �� ���ÿ� ����� �� �� ����Ǵ����� �������ְ�
        .pColorAttachments = &colorAttachmentRef0,                      //  �׸��� ��� ����� ��� Render Target (���ҽ�) �� �̾��������� ���� ������ �������ش�.
    };                                                                          // ���� Attachment �� ���� Reference ���� ������ ���� Subpass �� �����ָ�
                                                                                //  Render Target �� �������� ��Ȳ�� ������ �� �ִ�.

    VkRenderPassCreateInfo renderPassInfo{                              // Attachment (�Ǵ� �迭)�� Subpass (�Ǵ� �迭)�� ���� �ʿ��ѵ�, 
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,                               // ����� Color Attachment �� �ϳ��ۿ� �� ������, ���� ���� Attachment �� ����ϸ� �迭�� ���� �����ּҸ� �ִ´�.
        .subpassCount = 1,
        .pSubpasses = &subpass,                                         // Subpass ���� ���� ���� Subpass �� ����ϸ� �迭�� ���� �����ּҸ� �ִ´�.
    };

    if (vkCreateRenderPass(vk.device, &renderPassInfo, nullptr, &vk.renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }

    for (const auto& view : vk.swapChainImageViews) {                       // Frame Buffer : ���ϴ� ������ Render Target ���� ���õ� Subpass ��� ��Ϳ� ȣȯ�Ǵ� �ܼ�Ʈ�� ����
                                                                            //                Subpass �� Render Target, ��, ���ҽ����� �Ѳ����� ������ �� �ִ� Buffer
        VkFramebufferCreateInfo framebufferInfo{                            // Render Target �� Swap Chain �� ���� ����ؼ� �Ź� Render Target �� ����� �޶��� ���̱� ������ �� �޶����� ����ŭ Frame Buffer �� �ʿ��ϴ�.
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = vk.renderPass,                                    // Bind �� RenderPass �� ���� ������ ����
            .attachmentCount = 1,
            .pAttachments = &view,                                          // pAttachments �� ���� subpass ���� pColorAttachments �� ȣȯ�Ǿ�� bind �� �����ϴ�.
            .width = WIDTH,
            .height = HEIGHT,
            .layers = 1,
        };

        VkFramebuffer frameBuffer;
        if (vkCreateFramebuffer(vk.device, &framebufferInfo, nullptr, &frameBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }

        vk.framebuffers.push_back(frameBuffer);
    }
}

void createGraphicsPipeline()                               // State Machine �� OpenGL �� ������ ������������ �� �� �� �����Ѵ�. OpenGL �� State �� �ٲٸ鼭 ���� �ٸ� ������������ ����� ������.
{                                                           //  State ���� �������� �ٲ�� ������ ����ȭ�� ����ٴ� ���� OpenGL �� �����̴�.
    auto spv2shaderModule = [](const char* filename) {
        auto vsSpv = readFile(filename);                    // �ݸ�, Vulkan �� ������������ ������ �� �̸� ���������� State ���� ��� ����Ѵ�. �� ������������ State ���� �� ���ķ� ���� ���ؼ��� �ȵǴ� ���̴�.
        VkShaderModuleCreateInfo createInfo{                //  �̴� ����ȭ�� ���� ����Ǵ� �۾��̴�.
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = vsSpv.size(),                   // Shader Module �� ũ�⸦ ����
            .pCode = (uint*)vsSpv.data(),               // Shader Module �� unsigned int �� ����ȯ�ؼ� �����ؾ� ��. ( char Ÿ���̾��� vsSpv )
        };

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(vk.device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }
        return shaderModule;
    };
    VkShaderModule vsModule = spv2shaderModule("simple_vs.spv");    // simple_vs.glsl       // ��ǥ���� ������������ Graphics Pipeline �� Vertex 
    VkShaderModule fsModule = spv2shaderModule("simple_fs.spv");    // simple_fs.glsl

    VkPipelineShaderStageCreateInfo vsStageInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vsModule,
        .pName = "main",                                    // Shader �� main �Լ��� ���������� �˷��� ( main �� �Ǵ� �Լ��� a �̸�, a �� ����ص� �� )
    };
    VkPipelineShaderStageCreateInfo fsStageInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fsModule,
        .pName = "main",
    };

    VkPipelineShaderStageCreateInfo shaderStages[] = { vsStageInfo, fsStageInfo };      // Shader Stage �� ���� �� �ִ� ������ ���߿� ������������ ����� ���� ����Ѵ�.

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,       // ���� Shader �� ���� input �� ���� ������ ���� ������ �������� �ۼ��Ǿ���.
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    VkPipelineViewportStateCreateInfo viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo rasterizer{                              // Vertex �� Fragment Shader ���̿� �ִ� Shader
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1.0,                                   // lineWIdth �� ���� �������־�� �Ѵ�.
    };

    VkPipelineMultisampleStateCreateInfo multisampling{                         // ��Ƽ���ø� ( �� �ȼ� �� 1 ���� ���÷� ���� )
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachment{                   // Fragment Shader ȣ�� ������ Color �� ȣ�� ������ Color �� ���� ��� �ٷ��� ������.
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,    // ȣ�� ������ Color �� ������� ��
    };

    VkPipelineColorBlendStateCreateInfo colorBlending{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
    };

    std::vector<VkDynamicState> dynamicStates = {       // Dynamic State : �׷��Ƚ� ������������ State �� ������ �����̾�� �ϴµ�, �Ϻκ��� Dynamic State �� �������ָ� �� �Ϻδ� ��ȭ�� ���ȴ�.
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data(),
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    };

    if (vkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, nullptr, &vk.pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{                          // Pipeline �� ����� ���� ���Ǵ� ������
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = vk.pipelineLayout,
        .renderPass = vk.renderPass,
        .subpass = 0,                                       // basePipelineHandle �� �뵵�� ���� ������ pipeline �� ���ҽ��� ��Ȱ���ϸ鼭 �� �� ���� Pipeline �� ���� �� �ְ��ϴ� ���̴�. 
    };

    if (vkCreateGraphicsPipelines(vk.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &vk.graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");        // �׷��Ƚ� Pipeline �� ���ÿ� ���� �� ���� �� �ֱ⵵ �ϴ�.
    }
    
    vkDestroyShaderModule(vk.device, vsModule, nullptr);    // Pipeline �� ������� �Ŀ��� �޸� Ȯ���� ���� Shader �� ���� �޸𸮸� �������ִ� ���� ����.
    vkDestroyShaderModule(vk.device, fsModule, nullptr);
}

void createCommandCenter()                                              // Command Queue �� Device �������� �̹� �������, ���� Command Buffer �� Command Pool �� ������ �Ѵ�.
{
    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = vk.queueFamilyIndex,
    };

    if (vkCreateCommandPool(vk.device, &poolInfo, nullptr, &vk.commandPool) != VK_SUCCESS) {    // Command Pool : Command Buffer �� ��ɾ���� ����� �޸� ����
        throw std::runtime_error("failed to create command pool!");
    }

    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = vk.commandPool,
        .commandBufferCount = 1,
    };
                                                                                                // Command Buffer : ������ ���� ��ɵ��� �� Command Buffer �� ���̰� �Ǵµ� ���߿� �̷��� ���� Buffer ��
    if (vkAllocateCommandBuffers(vk.device, &allocInfo, &vk.commandBuffer) != VK_SUCCESS) {     //                   Command Queue �� �Ѳ����� �����Ѵ�. GPU �� ��ɾ ó���ϴ� �������� GPU ��ɵ���
        throw std::runtime_error("failed to allocate command buffers!");                        //                   Command Queue �� ���̴µ�, �̴� Command Buffer �� ����� ��ɵ��� ����� ���̴�.
    }
}

void createSyncObjects()                                        // Semaphore �� Fence, �� �ΰ����� ���� (����ȭ�� ���õ� �κе�) <- DirectX �� ���� ���̰� ���� �κ�
{
    VkSemaphoreCreateInfo semaphoreInfo{ 
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO 
    };
    VkFenceCreateInfo fenceInfo{                                // Fence �� ���°� Signaled �� Unsignaled, �� ������ ������.
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, 
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    if (vkCreateSemaphore(vk.device, &semaphoreInfo, nullptr, &vk.imageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateSemaphore(vk.device, &semaphoreInfo, nullptr, &vk.renderFinishedSemaphore) != VK_SUCCESS ||
        vkCreateFence(vk.device, &fenceInfo, nullptr, &vk.inFlightFence) != VK_SUCCESS) {
        throw std::runtime_error("failed to create synchronization objects for a frame!");
    }

}

void render()
{
    const VkClearValue clearColor = { .color = {0.0f, 0.0f, 0.0f, 1.0f} };
    const VkViewport viewport{ .width = (float)WIDTH, .height = (float)HEIGHT, .maxDepth = 1.0f };      // Viewport ũ��
    const VkRect2D scissor{ .extent = {.width = WIDTH, .height = HEIGHT } };                            // ���� ũ�� ( ������ �ʴ� ���� )
    const VkCommandBufferBeginInfo beginInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
                                                                                                               
    vkWaitForFences(vk.device, 1, &vk.inFlightFence, VK_TRUE, UINT64_MAX);      // Wait Fence : ���� Fence �� Signaled ���°� �� ������ ��ٸ��� �۾� (�̹� �ռ� Signaled ���·� ��������� �ٷ� �������� ����)
                                                                                    // VK_TRUE �� &vk.inFlightFence �� ���� Fence �迭�� ��� Fence ���� Signaled �����̸� �Ѿ���� �Ѵ�.
                                                                                    // UINT64_MAX �� ��������̴�. (Time Out) ���� ��� 1000000 �̸� 1���ε�, �� ��� 1�� ��ٸ��ٰ� ��ȣ�� �� ���� �������� �Ѿ��.
    vkResetFences(vk.device, 1, &vk.inFlightFence);                             // ��� Fance ���� Unsignaled ���·� �ٲ�

    uint32_t imageIndex;
    vkAcquireNextImageKHR(vk.device, vk.swapChain, UINT64_MAX, vk.imageAvailableSemaphore, nullptr, &imageIndex);   // Swap Chain �� �̹������� ȭ�鿡 ��µǰ� �ִ� �ϳ��� ������ �������� ��
                                                                                                                    //  �ϳ��� ���� �����ӿ��� �������� ���� ���� (Busy ��) �ǰ� �ִٰ� �� �� �� ���� ������ ���������� �ִµ�
                                                                                                                    //  �� �������� �� �ϳ��� return ���ִ� ���� ( ���⿡�� �������ϴ� ����� return ���� �� �ϳ��� �׷�����. )
                                                                                                                    //  ( imageIndex �� return ���� �̹����� ��° �� )
                                                                                                                    //  ( Fence �� �� �Ѱ��ְ� Semaphore �� �Ѱ��־��µ�, ����� ����� �ϳ��� ������ ��� �̹����� Busy �� ���, ������ ��� �� �̹����� �ε����� �ϴ��� return ������ ����� ������ ���� �״�� �Ѿ��. )
                                                                                                                    //    => ����Ϸ��� �̹����� ���� ������ ���°� �� �� �𸣱� ������, �ش� �̹����� ������ ���°� �Ǵ� ���� imageAvailableSemaphore �� Signal �� �ش�. 
    
    vkResetCommandBuffer(vk.commandBuffer, 0);                                                              // Draw Call �� ���� ��ɵ��� Command Buffer �� �ִ� �۾� ( ����, Command Buffer ������ ���� )
    {
        if (vkBeginCommandBuffer(vk.commandBuffer, &beginInfo) != VK_SUCCESS) {                             // Command Buffer �� ����
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = vk.renderPass,
            .framebuffer = vk.framebuffers[imageIndex],                                             // � Frame Buffer �� ����� �� ����
            .renderArea = { .extent = { .width = WIDTH, .height = HEIGHT } },                       // ������ ���� ���� (�Ź� �������־�� �ϴ� ����̴�.)
            .clearValueCount = 1,
            .pClearValues = &clearColor,
        };

        vkCmdBeginRenderPass(vk.commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);                // Render Pass �� ���� -> Frame Buffer �� Render Pass �� �� Bind ��.
        {
            vkCmdBindPipeline(vk.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.graphicsPipeline);      // Graphics Pipiline �� Bind ��Ŵ.
            vkCmdSetViewport(vk.commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(vk.commandBuffer, 0, 1, &scissor);                                              // viewport �� scissor �� Dynamic State �� �����߱� ������ ������ ���⿡�� �������־�� �Ѵ�. �׷��� ������ ȭ�鿡 �ƹ��͵� ���� �ʰ� �ȴ�.
            vkCmdDraw(vk.commandBuffer, 3, 1, 0, 0);
        }
        vkCmdEndRenderPass(vk.commandBuffer);

        if (vkEndCommandBuffer(vk.commandBuffer) != VK_SUCCESS) {                                           // Command Buffer �� ���� ( Command Buffer �� ���� ��� (�������� ���ҽ��� ���) �� ���� )
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    VkPipelineStageFlags waitStages[] = { 
        //VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT 
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
    };

    VkSubmitInfo submitInfo{ 
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &vk.imageAvailableSemaphore,                         // ����Ϸ��� Swap Chain �� �̹����� �غ���� ���� ������ ���� �����Ƿ� pWaitSemaphores �� imageAvailableSemaphore �� �������ش�.
        .pWaitDstStageMask = waitStages,                                        // Semaphore ���� waitStage Flag �� �ϳ��� �����ȴ�. Vulkan ������ Semaphore �� Signal ���� ������ ������� �ʰ� ������ ���� �Ϸ��� �۾��� �����س��⵵ �����ϴ�.
        .commandBufferCount = 1,
        .pCommandBuffers = &vk.commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &vk.renderFinishedSemaphore,                       // �������� �Ϸ�Ǹ� renderFinishedSemaphore �� �۾��� �Ϸ�Ǿ��ٴ� Signal �� ������ �Ѵ�.
    };

    if (vkQueueSubmit(vk.graphicsQueue, 1, &submitInfo, vk.inFlightFence) != VK_SUCCESS) {      // ����� ���� Command Buffer �� Present Queue (graphicsQueue) �� ���� (Submit)
        throw std::runtime_error("failed to submit draw command buffer!");                      //  �� ������ imageAvailableSemaphore �� Signal �� �Ŀ��� ����ȴ�.
    }

    VkPresentInfoKHR presentInfo{                                                           // Present Queue �� ������ Present �ϱ� ���� ����
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &vk.renderFinishedSemaphore,                                     // �������� �� ������ �ʾҴµ� Present �Ǹ� �ȵǱ� ������ renderFinishedSemaphore �� Ȱ���Ѵ�. ( Signal ������ �׶��� �Ǿ�� Present )
        .swapchainCount = 1,
        .pSwapchains = &vk.swapChain,
        .pImageIndices = &imageIndex,                                                       // Present �� �̹����� �ε���
    };

    vkQueuePresentKHR(vk.graphicsQueue, &presentInfo);                                      // Present Queue �� ������ Present �϶�� ����� ���� (Submit)
}

int main() 
{
    glfwInit();                             // �÷������� �ٸ� ȯ�濡 ���� ���� �ٸ� �������� GLFW �� ���ôµ�, �� GLFW �� �ʱ�ȭ
    GLFWwindow* window = createWindow();    // Window ����
    
                                            // Vulkan �� Instance �� ���� ����� Device �� ������ �Ѵ�.

    createVkInstance(window);               // Instance :   Vulkan ���ø����̼ǰ� GPU(�׷��� �ϵ����)�� ������ Vulkan ����̹� ���� ���� ������ �����ϴ� �ٽ� ��ü.
                                            // ���� : Vulkan ����̹� �ʱ�ȭ, ������ ����̽� Ž��, Ȯ��(�߰� ���) ����, ���� ���̾� Ȱ��ȭ, Vulkan ��ü ������ ������
    
    createVkDevice();                       // Device   :   Vulkan ���ø����̼��� GPU�� ����� Ȱ���ϱ� ���� ������ ����̽��� �����Ͽ� �����Ǵ� ��ü
                                            // ���� : GPU�� ���ø����̼� ���� �������̽� ����, �۾� ť ����, Ȯ�� Ȱ��ȭ, �޸� ����
                                            // Device �� Physical Device �� Logical Device �� ������.
                                            // Physical Device : �ý��ۿ� ��ġ�� ���� GPU �� ��ü
                                            // Logical Device : ������ ����̽��� ���� ǥ�� ( ���ø����̼��� Vulkan API�� ���� GPU�� ������ �� �ְ� �� )

    createSwapChain();                      // Swap Chain ����
    createRenderPass();                     // Render Pass ����
    createGraphicsPipeline();               // �׷��Ƚ� Pipeline ����
    createCommandCenter();
    createSyncObjects();

    while (!glfwWindowShouldClose(window)) 
    {
        glfwPollEvents();                       // �� ���� �״�� Debug ��忡�� �����ϸ�, ȭ���� �����ϸ� Error ���� ���.
        render();                               // render() ȣ���ϴ� �������� �������� �Ϸ�� ���ҽ��� ���� Command Buffer �� Present �϶�� ����� Present Queue �� ����־����µ�
                                                //  while ������ ���������� window �� ������ �Ǵµ� �� �������� Queue �� ����� ��� ���ҽ����� ���������� ����Ǿ��⿡ �߻��ϴ� �������̴�.
    }                                                                                                                                         //
                                                                                                                                             //
    vkDeviceWaitIdle(vk.device);        //////////// ���� GPU �� Queue �� ��� ����� ������ CPU �� ��ٷ��ֵ��� �̿� ���� ��������� �Ѵ�. <===//
    
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}