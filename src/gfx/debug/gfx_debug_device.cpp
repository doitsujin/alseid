#include "../../util/util_log.h"

#include "gfx_debug_device.h"

namespace as {

GfxDebugDevice::GfxDebugDevice(
        GfxDevice&&                   device)
: m_device(std::move(device)) {
  Log::info("Gfx: Initializing debug device");
}


GfxDebugDevice::~GfxDebugDevice() {
  Log::info("Gfx: Destroying debug device");
}


GfxShaderFormatInfo GfxDebugDevice::getShaderInfo() const {
  return m_device->getShaderInfo();
}


GfxDeviceFeatures GfxDebugDevice::getFeatures() const {
  return m_device->getFeatures();
}


GfxFormatFeatures GfxDebugDevice::getFormatFeatures(
        GfxFormat                     format) const {
  return m_device->getFormatFeatures(format);
}


bool GfxDebugDevice::supportsShadingRate(
        Extent2D                      shadingRate,
        uint32_t                      samples) const {
  return m_device->supportsShadingRate(shadingRate, samples);
}


uint64_t GfxDebugDevice::computeRayTracingBvhSize(
  const GfxRayTracingGeometryDesc&    desc) const {
  // TODO validate desc
  return m_device->computeRayTracingBvhSize(desc);
}


uint64_t GfxDebugDevice::computeRayTracingBvhSize(
  const GfxRayTracingInstanceDesc&    desc) const {
  // TODO validate desc
  return m_device->computeRayTracingBvhSize(desc);
}


GfxBuffer GfxDebugDevice::createBuffer(
  const GfxBufferDesc&                desc,
        GfxMemoryTypes                memoryTypes) {
  // TODO wrap buffer
  return m_device->createBuffer(desc, memoryTypes);
}


GfxColorBlendState GfxDebugDevice::createColorBlendState(
  const GfxColorBlendStateDesc&       desc) {
  // TODO validate state
  return m_device->createColorBlendState(desc);
}


GfxComputePipeline GfxDebugDevice::createComputePipeline(
  const GfxComputePipelineDesc&       desc) {
  if (!desc.compute)
    Log::err("GfxDevice::createComputePipeline: No compute shader specified");

  // TODO wrap pipeline
  return m_device->createComputePipeline(desc);          
}


GfxContext GfxDebugDevice::createContext(
        GfxQueue                      queue) {
  if (queue == GfxQueue::eSparseBinding || queue == GfxQueue::ePresent) {
    Log::err("GfxDevice::createContext: Invalid queue: ", uint32_t(queue), "\n"
      "For context creation, GfxQueue::eSparseBinding and GfxQueue::ePresent are not allowed.");
  }

  // TODO wrap context
  return m_device->createContext(queue);          
}


GfxDepthStencilState GfxDebugDevice::createDepthStencilState(
  const GfxDepthStencilStateDesc&     desc) {
  // TODO validate state
  return m_device->createDepthStencilState(desc);
}


GfxDescriptorArray GfxDebugDevice::createDescriptorArray(
  const GfxDescriptorArrayDesc&       desc) {
  if (!desc.descriptorCount)
    Log::err("GfxDevice::createDescriptorArray: Invalid descriptor count");

  if (desc.bindingType != GfxShaderBindingType::eResourceBuffer
   && desc.bindingType != GfxShaderBindingType::eResourceImageView
   && desc.bindingType != GfxShaderBindingType::eStorageBuffer
   && desc.bindingType != GfxShaderBindingType::eStorageImageView)
    Log::err("GfxDevice::createDescriptorArray: Invalid descriptor type");

  // TODO wrap descriptor array
  return m_device->createDescriptorArray(desc);
}


GfxGraphicsPipeline GfxDebugDevice::createGraphicsPipeline(
  const GfxGraphicsPipelineDesc&      desc) {
  if (!desc.vertex)
    Log::err("GfxDevice::createGraphicsPipeline: No vertex shader specified");

  // TODO wrap pipeline
  return m_device->createGraphicsPipeline(desc);          
}


GfxGraphicsPipeline GfxDebugDevice::createGraphicsPipeline(
  const GfxMeshPipelineDesc&          desc) {
  if (!desc.mesh)
    Log::err("GfxDevice::createGraphicsPipeline: No mesh shader specified");

  // TODO wrap pipeline
  return m_device->createGraphicsPipeline(desc);          
}


GfxImage GfxDebugDevice::createImage(
  const GfxImageDesc&                 desc,
        GfxMemoryTypes                memoryTypes) {
  // TODO wrap image
  return m_device->createImage(desc, memoryTypes);
}


GfxMultisampleState GfxDebugDevice::createMultisampleState(
  const GfxMultisampleStateDesc&      desc) {
  // TODO validate state
  return m_device->createMultisampleState(desc);
}


GfxPresenter GfxDebugDevice::createPresenter(
  const GfxPresenterDesc&             desc) {
  // TODO wrap presenter
  return m_device->createPresenter(desc);
}


GfxRasterizerState GfxDebugDevice::createRasterizerState(
  const GfxRasterizerStateDesc&       desc) {
  // TODO validate state
  return m_device->createRasterizerState(desc);
}


GfxRayTracingBvh GfxDebugDevice::createRayTracingBvh(
  const GfxRayTracingGeometryDesc&    desc) {
  // TODO validate desc and wrap BVH
  return m_device->createRayTracingBvh(desc);
}


GfxRayTracingBvh GfxDebugDevice::createRayTracingBvh(
  const GfxRayTracingInstanceDesc&    desc) {
  // TODO validate desc and wrap BVH
  return m_device->createRayTracingBvh(desc);
}


GfxRenderTargetState GfxDebugDevice::createRenderTargetState(
  const GfxRenderTargetStateDesc&     desc) {
  // TODO validate state
  return m_device->createRenderTargetState(desc);
}


GfxSampler GfxDebugDevice::createSampler(
  const GfxSamplerDesc&               desc) {
  // TODO wrap sampler
  return m_device->createSampler(desc);          
}


GfxSemaphore GfxDebugDevice::createSemaphore(
  const GfxSemaphoreDesc&             desc) {
  // TODO wrap semaphore
  return m_device->createSemaphore(desc);          
}


GfxVertexInputState GfxDebugDevice::createVertexInputState(
  const GfxVertexInputStateDesc&      desc) {
  // TODO validate state
  return m_device->createVertexInputState(desc);
}


void GfxDebugDevice::submit(
        GfxQueue                      queue,
        GfxCommandSubmission&&        submission) {
  // TODO validate everything
  m_device->submit(queue, std::move(submission));
}


void GfxDebugDevice::waitIdle() {
  m_device->waitIdle();
}

}
