#include "../../src/gfx/gfx.h"

#include "../../src/io/io.h"
#include "../../src/io/io_archive.h"

#include "../../src/util/util_error.h"
#include "../../src/util/util_log.h"

#include "../../src/wsi/wsi.h"

using namespace as;

const std::array<Vector4D, 8> g_vertexData = {
  Vector4D(-1.0f, -1.0f, -1.0f, 0.0f),
  Vector4D(-1.0f,  1.0f, -1.0f, 0.0f),
  Vector4D(-1.0f,  1.0f,  1.0f, 0.0f),
  Vector4D(-1.0f, -1.0f,  1.0f, 0.0f),
  Vector4D( 1.0f, -1.0f, -1.0f, 0.0f),
  Vector4D( 1.0f,  1.0f, -1.0f, 0.0f),
  Vector4D( 1.0f,  1.0f,  1.0f, 0.0f),
  Vector4D( 1.0f, -1.0f,  1.0f, 0.0f),
};


const std::array<uint16_t, 36> g_indexData = {
  0, 1, 2, 2, 3, 0,
  4, 5, 6, 6, 7, 4,
  0, 1, 5, 5, 4, 0,
  2, 3, 7, 7, 6, 2,
  0, 3, 7, 7, 4, 0,
  1, 2, 6, 6, 5, 1,
};


GfxComputePipeline load_pipeline(const GfxDevice& device) {
  Io io(IoBackend::eDefault, 1);

  IoArchive archive(io->open("resources/demo_03_raytracing_resources.asa", IoOpenMode::eRead));

  if (!archive)
    throw Error("Failed to open demo_03_raytracing_resources.asa");

  auto file = archive.findFile("cs_rt");

  if (!file)
    throw Error("Could not find file cs_rt in archive");

  auto format = device->getShaderInfo();
  auto subFile = file->findSubFile(format.identifier);

  if (!subFile)
    throw Error("Could not find shader code for selected graphics backend");

  GfxShaderDesc shaderDesc;
  shaderDesc.debugName = file->getName();

  if (!shaderDesc.deserialize(file->getInlineData()))
    throw Error("Failed to deserialize shader metadata");

  GfxShaderBinaryDesc binaryDesc;
  binaryDesc.format = format.format;
  binaryDesc.data.resize(subFile->getSize());

  if (archive.read(subFile, binaryDesc.data.data()) != IoStatus::eSuccess)
    throw Error("Failed to read shader binary");

  GfxComputePipelineDesc pipelineDesc;
  pipelineDesc.compute = GfxShader(std::move(shaderDesc), std::move(binaryDesc));

  return device->createComputePipeline(pipelineDesc);
}


void run_app() {
  Wsi wsi(WsiBackend::eDefault);

  Gfx gfx(GfxBackend::eDefault, wsi,
    GfxInstanceFlag::eDebugValidation |
    GfxInstanceFlag::eDebugMarkers |
    GfxInstanceFlag::eApiValidation);

  // Find a device with ray tracing support
  GfxDevice device;

  for (uint32_t i = 0; gfx->enumAdapters(i); i++) {
    GfxDevice currDevice = gfx->createDevice(gfx->enumAdapters(i));
    GfxDeviceFeatures features = currDevice->getFeatures();

    if (features.rayTracing) {
      device = currDevice;
      break;
    }
  }

  if (!device) {
    wsi->showMessage(LogSeverity::eError, "Ray tracing", "No ray tracing-capable device found.");
    return;
  }

  // Create pipeline as early as possible
  GfxComputePipeline pipeline = load_pipeline(device);

  // Create window and presenter
  WsiWindowDesc windowDesc;
  windowDesc.title = "Ray tracing";
  windowDesc.surfaceType = gfx->getBackendType();

  WsiWindow window = wsi->createWindow(windowDesc);

  GfxPresenterDesc presenterDesc;
  presenterDesc.window = window;
  presenterDesc.queue = GfxQueue::eCompute;
  presenterDesc.imageUsage = GfxUsage::eShaderStorage;

  GfxPresenter presenter = device->createPresenter(presenterDesc);

  // Create geometry BVH
  GfxRayTracingGeometry geometryInfo;
  geometryInfo.type = GfxRayTracingGeometryType::eMesh;
  geometryInfo.opacity = GfxRayTracingOpacity::eOpaque;
  geometryInfo.data.mesh.vertexFormat = GfxFormat::eR32G32B32A32f;
  geometryInfo.data.mesh.indexFormat = GfxFormat::eR16ui;
  geometryInfo.data.mesh.vertexCount = g_vertexData.size();
  geometryInfo.data.mesh.primitiveCount = g_indexData.size() / 3;

  GfxRayTracingGeometryDesc geometryBvhDesc;
  geometryBvhDesc.debugName = "Geometry BVH";
  geometryBvhDesc.geometries.push_back(geometryInfo);

  GfxRayTracingBvh geometryBvh = device->createRayTracingBvh(geometryBvhDesc);

  // Create instance BVH
  GfxRayTracingInstance instanceInfo;
  instanceInfo.opacity = GfxRayTracingOpacity::eOpaque;
  instanceInfo.instanceCount = 1;

  GfxRayTracingInstanceDesc instanceBvhDesc;
  instanceBvhDesc.debugName = "Instance BVH";
  instanceBvhDesc.flags = GfxRayTracingBvhFlag::eDynamic;
  instanceBvhDesc.instances.push_back(instanceInfo);

  GfxRayTracingBvh instanceBvh = device->createRayTracingBvh(instanceBvhDesc);

  // Upload geometry data
  GfxContext bvhContext = device->createContext(GfxQueue::eCompute);

  GfxBufferDesc bufferDesc;
  bufferDesc.debugName = "Geometry buffer";
  bufferDesc.size = sizeof(g_vertexData) + sizeof(g_indexData);
  bufferDesc.usage = GfxUsage::eTransferDst | GfxUsage::eBvhBuild;

  GfxBuffer geometryBuffer = device->createBuffer(bufferDesc, GfxMemoryType::eAny);

  GfxScratchBuffer scratch = bvhContext->allocScratch(GfxUsage::eCpuWrite | GfxUsage::eTransferSrc, bufferDesc.size);
  std::memcpy(scratch.map(GfxUsage::eCpuWrite, 0), g_vertexData.data(), sizeof(g_vertexData));
  std::memcpy(scratch.map(GfxUsage::eCpuWrite, sizeof(g_vertexData)), g_indexData.data(), sizeof(g_indexData));

  bvhContext->copyBuffer(geometryBuffer, 0, scratch.buffer, scratch.offset, scratch.size);
  bvhContext->memoryBarrier(GfxUsage::eTransferDst, 0, GfxUsage::eBvhBuild, 0);

  GfxRayTracingBvhData geometryData;
  geometryData.mesh.vertexData = geometryBuffer->getGpuAddress();
  geometryData.mesh.indexData = geometryBuffer->getGpuAddress() + sizeof(g_vertexData);

  bvhContext->buildRayTracingBvh(geometryBvh, GfxRayTracingBvhBuildMode::eBuild, &geometryData);
  bvhContext->memoryBarrier(GfxUsage::eBvhBuild, 0, GfxUsage::eBvhBuild, 0);

  GfxRayTracingInstanceData instanceProperties;
  instanceProperties.transform = Matrix4x3::identity();
  instanceProperties.instanceId = uint24_t(0);
  instanceProperties.visibilityMask = 0xff;
  instanceProperties.flags = GfxRayTracingInstanceFlag::eDisableFaceCulling;
  instanceProperties.geometryBvhAddress = geometryBvh->getGpuAddress();

  GfxScratchBuffer instanceDataBuffer = bvhContext->writeScratch(GfxUsage::eBvhBuild, instanceProperties);

  GfxRayTracingBvhData instanceData;
  instanceData.instances.instanceData = instanceDataBuffer.getGpuAddress();

  bvhContext->buildRayTracingBvh(instanceBvh, GfxRayTracingBvhBuildMode::eBuild, &instanceData);
  bvhContext->memoryBarrier(GfxUsage::eBvhBuild, 0, GfxUsage::eBvhBuild | GfxUsage::eBvhTraversal, 0);

  // Submit and wait. We're lazy here for simplicity.
  GfxCommandSubmission submission;
  submission.addCommandList(bvhContext->endCommandList());

  device->submit(GfxQueue::eCompute, std::move(submission));
  device->waitIdle();

  // We don't need the temporary context anymore
  bvhContext = nullptr;

  bool quit = false;

  auto startTime = std::chrono::high_resolution_clock::now();

  while (!quit) {
    auto curTime = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::duration<float, std::ratio<1>>>(curTime - startTime).count();

    wsi->processEvents([&quit] (const WsiEvent& e) {
      quit |= e.type == WsiEventType::eQuitApp
           || e.type == WsiEventType::eWindowClose;
    });

    presenter->present([&] (const GfxPresenterContext& args) {
      GfxContext context = args.getContext();
      GfxImage image = args.getImage();

      // Compute model matrix
      float th = elapsed * 3.141592654f / 2.0f;
      Matrix4x4 transformMatrix = computeRotationMatrix(normalize(Vector3D(0.5f, 1.0f, 0.2f)), th);

      // Update instance BVH with the new matrix
      GfxRayTracingInstanceData instanceProperties;
      instanceProperties.transform = Matrix4x3(transpose(transformMatrix));
      instanceProperties.instanceId = uint24_t(0);
      instanceProperties.visibilityMask = 0xff;
      instanceProperties.flags = GfxRayTracingInstanceFlag::eDisableFaceCulling;
      instanceProperties.geometryBvhAddress = geometryBvh->getGpuAddress();

      GfxScratchBuffer instanceDataBuffer = context->writeScratch(GfxUsage::eBvhBuild, instanceProperties);

      GfxRayTracingBvhData instanceData;
      instanceData.instances.instanceData = instanceDataBuffer.getGpuAddress();

      context->buildRayTracingBvh(instanceBvh, GfxRayTracingBvhBuildMode::eUpdate, &instanceData);
      context->memoryBarrier(GfxUsage::eBvhBuild, 0, GfxUsage::eBvhTraversal, GfxShaderStage::eCompute);

      // Compute view and projection matrix
      float f = 5.0f / 3.141592654f;
      float zNear = 0.001f;

      th = 3.141592654f / 6.0f;

      Matrix4x4 viewMatrix = computeViewMatrix(
        Vector3D(1.0f,  0.0f,  0.0f), th,
        Vector3D(0.0f, -2.0f, -3.0f));

      Matrix4x4 projMatrix = computeProjectionMatrix(
        Vector2D(image->getDesc().extent), f, zNear);

      // Initialize swap chain image and prepare it for rendering
      context->imageBarrier(image, image->getAvailableSubresources(),
        0, 0, GfxUsage::eShaderStorage, GfxShaderStage::eCompute, GfxBarrierFlag::eDiscard);

      // Create an image view for rendering
      GfxImageViewDesc viewDesc;
      viewDesc.type = GfxImageViewType::e2D;
      viewDesc.format = image->getDesc().format;
      viewDesc.subresource = image->getAvailableSubresources();
      viewDesc.usage = GfxUsage::eShaderStorage;

      GfxImageView view = image->createView(viewDesc);

      context->bindPipeline(pipeline);
      context->bindDescriptor(0, 0, view->getDescriptor());
      context->bindDescriptor(0, 1, instanceBvh->getDescriptor());
      context->setShaderConstants(0, viewMatrix);
      context->setShaderConstants(64, projMatrix);
      context->dispatch(gfxComputeWorkgroupCount(
        image->getDesc().extent, pipeline->getWorkgroupSize()));

      // Prepare the swap chain image for presentation
      context->imageBarrier(image, image->getAvailableSubresources(),
        GfxUsage::eShaderStorage, GfxShaderStage::eCompute, GfxUsage::ePresent, 0, 0);

      // Avoid write-after-read hazards for BVH updates
      context->memoryBarrier(GfxUsage::eBvhTraversal, GfxShaderStage::eCompute, GfxUsage::eBvhBuild, 0);
    });
  }

  device->waitIdle();
}


int main(int argc, char** argv) {
  try {
    run_app();
    return 0;
  } catch (const Error& e) {
    Log::err(e.what());
    return 1;
  }
}
