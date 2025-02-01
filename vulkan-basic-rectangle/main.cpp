#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <tuple>
#include <bitset>
#include <span>
#include <cmath>
//#include "glsl2spv.h"
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

#define TINYOBJLOADER_IMPLEMENTATION // define this in only *one* .cc
// Optional. define TINYOBJLOADER_USE_MAPBOX_EARCUT gives robust triangulation. Requires C++11
//#define TINYOBJLOADER_USE_MAPBOX_EARCUT
#include "tiny_obj_loader.h"

typedef unsigned int uint;

const uint32_t WIDTH = 900;
const uint32_t HEIGHT = 600; // 

#ifdef NDEBUG
const bool ON_DEBUG = false;
#else
const bool ON_DEBUG = true;
#endif


struct UniformBufferObject
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

struct Global {
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

    VkRenderPass renderPass;
    std::vector<VkFramebuffer> framebuffers;

    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;

    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;

    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    VkBuffer uniformBuffer;
    VkDeviceMemory uniformBufferMemory;

    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;

    ~Global() {
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);
        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);
        vkDestroyBuffer(device, uniformBuffer, nullptr);
        vkFreeMemory(device, uniformBufferMemory, nullptr);

        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

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

class StanfordBunny
{
public:
    std::string inputfile = "C:/Users/rachel/Documents/GPU/build-up-phase/vulkan-basic-rectangle/include/bunny-good.obj";
    tinyobj::ObjReaderConfig reader_config;
    tinyobj::ObjReader reader;

    std::vector<float> vertData;
    std::vector<uint32_t> idxData;

    const uint vertexBytesSize = 24;
    const uint vertexPositionOffset = 0;
    const uint vertexColorOffset = 12;

    StanfordBunny() {
        if (!reader.ParseFromFile(inputfile, reader_config)) {
            if (!reader.Error().empty()) {
                std::cerr << "TinyObjReader: " << reader.Error();
            }
            exit(1);
        }

        if (!reader.Warning().empty()) {
            std::cout << "TinyObjReader: " << reader.Warning();
        }

        auto& attrib = reader.GetAttrib();
        auto& shapes = reader.GetShapes();

        size_t index_offset = 0;
        for (; index_offset < attrib.vertices.size(); index_offset += 3) {
            vertData.push_back(attrib.vertices[index_offset + 0]);
            vertData.push_back(attrib.vertices[index_offset + 1]);
            vertData.push_back(attrib.vertices[index_offset + 2]);

            vertData.push_back(attrib.colors[index_offset + 0]);
            vertData.push_back(attrib.colors[index_offset + 1]);
            vertData.push_back(attrib.colors[index_offset + 2]);
        }

        for (const auto& idx: shapes[0].mesh.indices) {
            idxData.push_back(static_cast<uint32_t>(idx.vertex_index));
        }
    }

    std::tuple<const std::vector<float>*, size_t> getVertices() {
        return { &vertData, sizeof(vertData[0]) * vertData.size() };
    }

    std::tuple<const std::vector<uint32_t>*, size_t> getIndices() {
        return { &idxData, sizeof(idxData[0]) * idxData.size() };
    }

    VkVertexInputBindingDescription getBindingDescription() {
        return {
            .binding = 0,
            .stride = vertexBytesSize,
            //.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };
    }

    std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
        return {
            {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,   // x, y, z
                .offset = vertexPositionOffset, // 0
            }, {
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,   // r, g, b
                .offset = vertexColorOffset,    // 12
            }
        };
    }

} sb;

//struct Geometry {
//    static const uint vertexBytesSize = 20;
//    static const uint vertexPositionOffset = 0;
//    static const uint vertexColorOffset = 8;
//
//    static std::tuple<float*, size_t> getVertices() {
//        static float data[] = {
//            -0.5, -0.5, 1, 0, 0,   // x, y, r, g, b -> total 20 bytes per vertex
//            0.5, -0.5, 0, 1, 0,
//            0.5, 0.5, 0, 0, 1,
//            -0.5, 0.5, 0, 0, 1,
//        };
//        return { data, sizeof(data) };
//    }
//
//    static std::tuple<uint16_t*, size_t> getIndices() {
//        static uint16_t data[] = {
//            0, 1, 2, 2, 3, 0
//        };
//        return { data, sizeof(data) };
//    }
//
//    static VkVertexInputBindingDescription getBindingDescription() {
//        return {
//            .binding = 0,
//            .stride = vertexBytesSize,
//            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
//        };
//    }
//
//    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
//        return {
//            {
//                .location = 0,
//                .format = VK_FORMAT_R32G32_SFLOAT,      // x, y
//                .offset = vertexPositionOffset,
//            }, {
//                .location = 1,
//                .format = VK_FORMAT_R32G32B32_SFLOAT,   // r, g, b
//                .offset = vertexColorOffset,
//            }
//        };
//    }
//};


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

template <typename T, typename F, typename... Args>
inline std::vector<T> arrayOf(F pFunc, Args... args)
{
    uint32_t count;
    pFunc(args..., &count, nullptr);
    std::vector<T> result(count);
    pFunc(args..., &count, result.data());
    return result;
}

bool checkValidationLayerSupport(std::vector<const char*>& reqestNames)
{
    auto availables = arrayOf<VkLayerProperties>(vkEnumerateInstanceLayerProperties);

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
    auto availables = arrayOf<VkExtensionProperties>(vkEnumerateDeviceExtensionProperties, device, nullptr);

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

std::vector<char> readFile(const std::string& filename)
{
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
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    return glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
}

void createVkInstance(GLFWwindow* window)
{
    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Hello Triangle",
        .apiVersion = VK_API_VERSION_1_0
    };

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if(ON_DEBUG) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    std::vector<const char*> validationLayers;
    if(ON_DEBUG) validationLayers.push_back("VK_LAYER_KHRONOS_validation");
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

    auto devices = arrayOf<VkPhysicalDevice>(vkEnumeratePhysicalDevices, vk.instance);

    std::vector<const char*> extentions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

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

    auto queueFamilies = arrayOf<VkQueueFamilyProperties>(vkGetPhysicalDeviceQueueFamilyProperties, vk.physicalDevice);

    vk.queueFamilyIndex = 0;
    {
        for (; vk.queueFamilyIndex < queueFamilies.size(); ++vk.queueFamilyIndex)
        {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(vk.physicalDevice, vk.queueFamilyIndex, vk.surface, &presentSupport);

            if (queueFamilies[vk.queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT && presentSupport)
                break;
        }

        if (vk.queueFamilyIndex >=  queueFamilies.size())
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
        auto formats = arrayOf<VkSurfaceFormatKHR>(vkGetPhysicalDeviceSurfaceFormatsKHR, vk.physicalDevice, vk.surface);

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
        auto presentModes = arrayOf<VkPresentModeKHR>(vkGetPhysicalDeviceSurfacePresentModesKHR, vk.physicalDevice, vk.surface);

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

    vk.swapChainImages = arrayOf<VkImage>(vkGetSwapchainImagesKHR, vk.device, vk.swapChain);

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

void createRenderPass()
{
    VkAttachmentDescription colorAttachment{
        .format = vk.swapChainImageFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference colorAttachmentRef0{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass{
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef0,
    };

    VkRenderPassCreateInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
    };

    if (vkCreateRenderPass(vk.device, &renderPassInfo, nullptr, &vk.renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }

    for (const auto& view : vk.swapChainImageViews) {
        VkFramebufferCreateInfo framebufferInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = vk.renderPass,
            .attachmentCount = 1,
            .pAttachments = &view,
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

void createDescriptorRelated()
{
    // Create Descriptor Set Layout
    {
        VkDescriptorSetLayoutBinding uboLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        };

        VkDescriptorSetLayoutCreateInfo layoutInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings = &uboLayoutBinding,
        };

        if (vkCreateDescriptorSetLayout(vk.device, &layoutInfo, nullptr, &vk.descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }

    // Create Descriptor Pool
    {
        VkDescriptorPoolSize poolSize{
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
        };

        VkDescriptorPoolCreateInfo poolInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &poolSize,
        };

        if (vkCreateDescriptorPool(vk.device, &poolInfo, nullptr, &vk.descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    // Create Descriptor Set
    {
        VkDescriptorSetAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = vk.descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &vk.descriptorSetLayout,
        };

        if (vkAllocateDescriptorSets(vk.device, &allocInfo, &vk.descriptorSet) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }
    }

    // Create Pipeline Layout
    {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &vk.descriptorSetLayout,
        };

        if (vkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, nullptr, &vk.pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }
    }

    // vkDestroyDescriptorSetLayout(vk.device, vk.descriptorSetLayout, nullptr);
}

void createGraphicsPipeline() 
{
    auto spv2shaderModule = [](const char* filename) {
        auto vsSpv = readFile(filename);
        VkShaderModuleCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = vsSpv.size(),
            .pCode = (uint*)vsSpv.data(),
        };

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(vk.device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }
        return shaderModule;
    };
    VkShaderModule vsModule = spv2shaderModule("vertex_input_vs.glsl");
    VkShaderModule fsModule = spv2shaderModule("vertex_input_fs.glsl");

    VkPipelineShaderStageCreateInfo vsStageInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vsModule,
        .pName = "main",
    };
    VkPipelineShaderStageCreateInfo fsStageInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fsModule,
        .pName = "main",
    };

    VkPipelineShaderStageCreateInfo shaderStages[] = { vsStageInfo, fsStageInfo };

    auto bindingDescription = sb.getBindingDescription();
    auto attributeDescriptions = sb.getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount = (uint) attributeDescriptions.size(),
        .pVertexAttributeDescriptions = attributeDescriptions.data(),
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    VkPipelineViewportStateCreateInfo viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo rasterizer{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0,
    };

    VkPipelineMultisampleStateCreateInfo multisampling{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo colorBlending{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
    };

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data(),
    };

    VkGraphicsPipelineCreateInfo pipelineInfo{
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
        .subpass = 0,
    };

    if (vkCreateGraphicsPipelines(vk.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &vk.graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }
    
    vkDestroyShaderModule(vk.device, vsModule, nullptr);
    vkDestroyShaderModule(vk.device, fsModule, nullptr);
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
        vkCreateFence(vk.device, &fenceInfo, nullptr, &vk.inFlightFence) != VK_SUCCESS) {
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
    if (vkAllocateMemory(vk.device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate vertex buffer memory!");
    }

    vkBindBufferMemory(vk.device, buffer, bufferMemory, 0);

    return { buffer, bufferMemory };
}

void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) 
{
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        //.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,  
    };

    vkBeginCommandBuffer(vk.commandBuffer, &beginInfo);
    {
        VkBufferCopy copyRegion{ .size = size };
        vkCmdCopyBuffer(vk.commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    }
    vkEndCommandBuffer(vk.commandBuffer);

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &vk.commandBuffer,
    };

    vkQueueSubmit(vk.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(vk.graphicsQueue);
}

void createVertexBuffer()
{
    //auto [data, size] = Geometry::getVertices();
    auto [data, size] = sb.getVertices();

    std::tie(vk.vertexBuffer, vk.vertexBufferMemory) = createBuffer(
        size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* dst;
    vkMapMemory(vk.device, vk.vertexBufferMemory, 0, size, 0, &dst);
    memcpy(dst, (*data).data(), size);
    vkUnmapMemory(vk.device, vk.vertexBufferMemory);
}

//void updateVertexBuffer(float t)
//{
//    size_t size = std::get<1>(Geometry::getVertices());
//    uint count = (uint)size / sizeof(float);
//    void* dst;
//    vkMapMemory(vk.device, vk.vertexBufferMemory, 0, size, 0, &dst);
//    for (uint i = 0; i < count; i+=5)
//        ((float*)dst)[i] += t;
//    vkUnmapMemory(vk.device, vk.vertexBufferMemory);
//}

void createIndexBuffer()
{
    //auto [data, size] = Geometry::getIndices();
    auto [data, size] = sb.getIndices();

    auto [stagingBuffer, stagingBufferMemory] = createBuffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    std::tie(vk.indexBuffer, vk.indexBufferMemory) = createBuffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    void* dst;
    vkMapMemory(vk.device, stagingBufferMemory, 0, size, 0, &dst);
    memcpy(dst, (*data).data(), size);
    vkUnmapMemory(vk.device, stagingBufferMemory);

    copyBuffer(stagingBuffer, vk.indexBuffer, size);

    vkDestroyBuffer(vk.device, stagingBuffer, nullptr);
    vkFreeMemory(vk.device, stagingBufferMemory, nullptr);
}

void updateUniformBuffer()
{
    static void* dst;
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{
        .model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
        //.model = glm::mat4(1.0f),
        .view = glm::lookAt(glm::vec3(0.0f, 4.0f, 4.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
        .proj = glm::perspective(glm::radians(45.0f), WIDTH / (float)HEIGHT, 0.1f, 10.0f)
    };
    ubo.proj[1][1] *= -1;

    //float translation[2] = { 0, std::sin(t * 100) };

    if (!vk.uniformBuffer)
    {
        std::tie(vk.uniformBuffer, vk.uniformBufferMemory) = createBuffer(
            sizeof(ubo),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        vkMapMemory(vk.device, vk.uniformBufferMemory, 0, sizeof(ubo), 0, &dst);

        VkDescriptorBufferInfo bufferInfo{
            .buffer = vk.uniformBuffer,
            .range = VK_WHOLE_SIZE,
        };

        VkWriteDescriptorSet descriptorWrite{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = vk.descriptorSet,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &bufferInfo,
        };

        vkUpdateDescriptorSets(vk.device, 1, &descriptorWrite, 0, nullptr);
    }

    memcpy(dst, &ubo, sizeof(ubo));
}

void render()
{
    const VkClearValue clearColor = { .color = {0.0f, 0.0f, 0.0f, 1.0f} };
    const VkViewport viewport{ .width = (float)WIDTH, .height = (float)HEIGHT, .maxDepth = 1.0f };
    const VkRect2D scissor{ .extent = {.width = WIDTH, .height = HEIGHT } };
    const VkCommandBufferBeginInfo beginInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };

    vkWaitForFences(vk.device, 1, &vk.inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(vk.device, 1, &vk.inFlightFence);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(vk.device, vk.swapChain, UINT64_MAX, vk.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

    vkResetCommandBuffer(vk.commandBuffer, 0);
    {
        if (vkBeginCommandBuffer(vk.commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = vk.renderPass,
            .framebuffer = vk.framebuffers[imageIndex],
            .renderArea = { .extent = { .width = WIDTH, .height = HEIGHT } },
            .clearValueCount = 1,
            .pClearValues = &clearColor,
        };

        vkCmdBeginRenderPass(vk.commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        {
            //vkCmdBindPipeline(vk.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.graphicsPipeline);
            vkCmdSetViewport(vk.commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(vk.commandBuffer, 0, 1, &scissor);

            VkDeviceSize offsets[] = { 0 };
            //size_t numIndices = std::get<1>(Geometry::getIndices()) / sizeof(uint16_t);
            uint32_t numIndices = (uint32_t)std::get<0>(sb.getIndices())->size();
            vkCmdBindVertexBuffers(vk.commandBuffer, 0, 1, &vk.vertexBuffer, offsets);
            vkCmdBindIndexBuffer(vk.commandBuffer, vk.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(
                vk.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                vk.pipelineLayout, 0,
                1, &vk.descriptorSet,
                0, nullptr);
            vkCmdBindPipeline(vk.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.graphicsPipeline);

            vkCmdDrawIndexed(vk.commandBuffer, numIndices, 1, 0, 0, 0);
        }
        vkCmdEndRenderPass(vk.commandBuffer);

        if (vkEndCommandBuffer(vk.commandBuffer) != VK_SUCCESS) {
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
        .pWaitSemaphores = &vk.imageAvailableSemaphore,
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &vk.commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &vk.renderFinishedSemaphore,
    };

    if (vkQueueSubmit(vk.graphicsQueue, 1, &submitInfo, vk.inFlightFence) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &vk.renderFinishedSemaphore,
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
    createSwapChain();
    createRenderPass();
    createDescriptorRelated();
    createGraphicsPipeline();
    createCommandCenter();
    createSyncObjects();
    createVertexBuffer();
    createIndexBuffer();

    //float t = 0.f;
    while (!glfwWindowShouldClose(window)) 
    {
        glfwPollEvents();
        //updateVertexBuffer(t * 0.01f);
        updateUniformBuffer();
        render();
        //t += 0.00001f;
    }
    
    vkDeviceWaitIdle(vk.device);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}