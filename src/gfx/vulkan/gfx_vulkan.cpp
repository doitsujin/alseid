#include "../../util/util_error.h"
#include "../../util/util_log.h"

#include "../debug/gfx_debug_device.h"

#include "gfx_vulkan.h"
#include "gfx_vulkan_device.h"

namespace as {

VkBool32 VKAPI_PTR gfxVulkanDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
    void*                                            pUserData) {
  LogSeverity severity = LogSeverity::eInfo;

  if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    severity = LogSeverity::eWarn;
  if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    severity = LogSeverity::eError;

  Log::message(severity, pCallbackData->pMessage);
  return VK_FALSE;
}


GfxVulkan::GfxVulkan(
  const Wsi&                          wsi,
        GfxInstanceFlags              flags)
: m_wsiBridge   (wsi, m_vk)
, m_vk          (m_wsiBridge->getVulkanEntryPoint())
, m_flags       (flags) {
  // Ensure that the Vulkan loader is set up properly
  Log::info("Initializing Vulkan graphics");

  if (!m_vk.vkGetInstanceProcAddr)
    throw Error("vkGetInstanceProcAddr not found");

  Log::info("Vulkan: Found vkGetInstanceProcAddr @ ", m_vk.vkGetInstanceProcAddr.getAddress());

  // Initialize extension and layer list
  std::vector<const char*> layers;
  std::vector<const char*> extensions;

  unsigned int extensionCount = 0;
  m_wsiBridge->getInstanceExtensions(&extensionCount, nullptr);

  extensions.resize(extensionCount);
  m_wsiBridge->getInstanceExtensions(&extensionCount, extensions.data());

  if (m_flags & GfxInstanceFlag::eApiValidation)
    layers.push_back("VK_LAYER_KHRONOS_validation");

  if (m_flags & (GfxInstanceFlag::eApiValidation | GfxInstanceFlag::eDebugMarkers))
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

  // Initialize some debugging stuff if enabled
  VkDebugUtilsMessengerCreateInfoEXT messengerInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
  messengerInfo.messageSeverity =
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  messengerInfo.messageType =
    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  messengerInfo.pfnUserCallback = &gfxVulkanDebugCallback;
  messengerInfo.pUserData = nullptr;

  // Create Vulkan instance
  VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
  appInfo.apiVersion = VK_API_VERSION_1_3;
  appInfo.pApplicationName = nullptr;
  appInfo.applicationVersion = 0;
  appInfo.pEngineName = "alseid";
  appInfo.engineVersion = 0;

  VkInstanceCreateInfo instanceInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
  instanceInfo.pApplicationInfo = &appInfo;
  instanceInfo.enabledLayerCount = layers.size();
  instanceInfo.ppEnabledLayerNames = layers.data();
  instanceInfo.enabledExtensionCount = extensions.size();
  instanceInfo.ppEnabledExtensionNames = extensions.data();

  if (m_flags & GfxInstanceFlag::eApiValidation)
    instanceInfo.pNext = &messengerInfo;

  VkInstance instance = VK_NULL_HANDLE;
  VkResult vr = m_vk.vkCreateInstance(&instanceInfo, nullptr, &instance);

  if (vr)
    throw VulkanError("Vulkan: Failed to create Vulkan instance", vr);

  m_vk = GfxVulkanProcs(m_vk, instance);

  // Create debug messenger if requested
  if (m_flags & GfxInstanceFlag::eApiValidation) {
    if ((vr = m_vk.vkCreateDebugUtilsMessengerEXT(m_vk.instance,
        &messengerInfo, nullptr, &m_debugMessenger))) {
      destroyObjects();
      throw VulkanError("Vulkan: Failed to create Vulkan debug messenger", vr);
    }
  }

  // Initialize adapter objects for physical devices
  uint32_t adapterCount = 0;
  
  if ((vr = m_vk.vkEnumeratePhysicalDevices(m_vk.instance, &adapterCount, nullptr))) {
    destroyObjects();
    throw VulkanError("Vulkan: Failed to enumerate physical devices", vr);
  }

  std::vector<VkPhysicalDevice> adapters(adapterCount);

  if ((vr = m_vk.vkEnumeratePhysicalDevices(m_vk.instance, &adapterCount, adapters.data()))) {
    destroyObjects();
    throw VulkanError("Vulkan: Failed to enumerate physical devices", vr);
  }

  m_adapters.reserve(adapterCount);

  for (uint32_t i = 0; i < adapterCount; i++) {
    VkPhysicalDeviceProperties properties = { };
    m_vk.vkGetPhysicalDeviceProperties(adapters[i], &properties);

    if (properties.apiVersion < VK_MAKE_VERSION(1, 3, 0)) {
      Log::warn("Vulkan: Skipping Vulkan ",
        VK_VERSION_MAJOR(properties.apiVersion), ".",
        VK_VERSION_MINOR(properties.apiVersion), " adapter: ",
        properties.deviceName);
      continue;
    }

    auto adapter = std::make_shared<GfxVulkanAdapter>(m_vk, adapters[i]);

    if (!adapter->isSuitable()) {
      Log::warn("Vulkan: Skipping unsupported adapter: ", properties.deviceName);
      continue;
    }

    Log::info("Vulkan: Found adapter: ", properties.deviceName);
    m_adapters.push_back(std::move(adapter));
  }

  if (m_adapters.empty()) {
    destroyObjects();

    throw Error("Vulkan: No suitable physical devices found");
  }
}


GfxVulkan::~GfxVulkan() {
  Log::info("Shutting down Vulkan graphics");

  destroyObjects();
}


GfxBackend GfxVulkan::getBackendType() const {
  return GfxBackend::eVulkan;
}


GfxAdapter GfxVulkan::enumAdapters(
        uint32_t                      index) {
  return index < m_adapters.size()
    ? GfxAdapter(m_adapters[index])
    : GfxAdapter();
}


GfxDevice GfxVulkan::createDevice(
  const GfxAdapter&                   adapter) {
  auto& vulkanAdapter = static_cast<GfxVulkanAdapter&>(*adapter);

  GfxDevice device(std::make_shared<GfxVulkanDevice>(
    shared_from_this(), vulkanAdapter.getHandle()));

  if (m_flags & GfxInstanceFlag::eDebugValidation)
    device = GfxDevice(std::make_shared<GfxDebugDevice>(std::move(device)));

  return device;
}


void GfxVulkan::destroyObjects() {
  m_wsiBridge = nullptr;

  if (m_debugMessenger)
    m_vk.vkDestroyDebugUtilsMessengerEXT(m_vk.instance, m_debugMessenger, nullptr);

  m_vk.vkDestroyInstance(m_vk.instance, nullptr);
}

}
