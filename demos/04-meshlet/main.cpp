#include <chrono>

#include "../../src/gfx/gfx.h"
#include "../../src/gfx/gfx_geometry.h"
#include "../../src/gfx/gfx_transfer.h"

#include "../../src/io/io.h"
#include "../../src/io/io_archive.h"

#include "../../src/util/util_error.h"
#include "../../src/util/util_log.h"
#include "../../src/util/util_matrix.h"
#include "../../src/util/util_quaternion.h"

#include "../../src/wsi/wsi.h"

using namespace as;

struct SceneConstants {
  Projection projection;
  QuatTransform viewTransform;
  ViewFrustum viewFrustum;
};


struct InstanceConstants {
  QuatTransform modelTransform;
  float morphWeights[4];
};


class MeshletApp {

public:

  MeshletApp()
  : m_io    (IoBackend::eDefault, std::thread::hardware_concurrency())
  , m_wsi   (WsiBackend::eDefault)
  , m_gfx   (GfxBackend::eDefault, m_wsi,
              // GfxInstanceFlag::eApiValidation |
              GfxInstanceFlag::eDebugValidation |
              GfxInstanceFlag::eDebugMarkers) {
    m_window = createWindow();
    m_device = createDevice();
    m_presenter = createPresenter();
    // m_presenter->setPresentMode(GfxPresentMode::eImmediate);
    m_archive = loadArchive();

    IoRequest request = loadResources();
    request->wait();

    // Create mesh shader pipeline
    GfxDeviceFeatures features = m_device->getFeatures();

    if (!(features.shaderStages & GfxShaderStage::eMesh))
      throw Error("Mesh shader support required");

    GfxMeshPipelineDesc msPipelineDesc;
    msPipelineDesc.debugName = "MS pipeline";
    msPipelineDesc.task = findShader("ts_task");
    msPipelineDesc.mesh = findShader("ms_object");
    msPipelineDesc.fragment = findShader("fs_object");

    m_msPipeline = m_device->createGraphicsPipeline(msPipelineDesc);

    // Create joint transform pipeline
    GfxComputePipelineDesc csPipelineDesc;
    csPipelineDesc.debugName = "Joint transform pipeline";
    csPipelineDesc.compute = findShader("cs_transform");
    m_csTransform = m_device->createComputePipeline(csPipelineDesc);

    csPipelineDesc.debugName = "Joint animation pipeline";
    csPipelineDesc.compute = findShader("cs_animate");
    m_csAnimate = m_device->createComputePipeline(csPipelineDesc);

    // Initialize transfer manager
    m_transfer = GfxTransferManager(m_io, m_device, 16ull << 20);

    // Create geometry object and buffer
    m_geometry = createGeometry();
    m_geometryBuffer = createGeometryBuffer();

    // Create state objects
    m_renderState = createRenderState();

    // Create feedback buffer
    m_jointTransforms = createTransformBuffer();
    m_jointRelative = createTransformBuffer();
    m_morphWeights = createMorphTargetBuffer();
    m_animation = createAnimationBuffer();
  }


  void run() {
    bool quit = false;

    float deltaX = 0.0f;
    float deltaY = 0.0f;
    float deltaZ = 0.0f;

    std::array<float, 2> deltaRot = { };

    while (!quit) {
      m_presenter->synchronize(1);

      m_wsi->processEvents([&] (const WsiEvent& e) {
        quit |= e.type == WsiEventType::eQuitApp
             || e.type == WsiEventType::eWindowClose;

        if (e.type == WsiEventType::eKeyPress) {
          if (e.info.key.scancode == WsiScancode::eW)
            deltaZ = e.info.key.pressed ? -1.0f : 0.0f;
          if (e.info.key.scancode == WsiScancode::eS)
            deltaZ = e.info.key.pressed ? 1.0f : 0.0f;
          if (e.info.key.scancode == WsiScancode::eA)
            deltaX = e.info.key.pressed ? -1.0f : 0.0f;
          if (e.info.key.scancode == WsiScancode::eD)
            deltaX = e.info.key.pressed ? 1.0f : 0.0f;
          if (e.info.key.scancode == WsiScancode::eLeft)
            deltaRot[0] = e.info.key.pressed ? 1.0f : 0.0f;
          if (e.info.key.scancode == WsiScancode::eRight)
            deltaRot[0] = e.info.key.pressed ? -1.0f : 0.0f;
          if (e.info.key.scancode == WsiScancode::eUp)
            deltaRot[1] = e.info.key.pressed ? 1.0f : 0.0f;
          if (e.info.key.scancode == WsiScancode::eDown)
            deltaRot[1] = e.info.key.pressed ? -1.0f : 0.0f;
        }
      });

      auto t = std::chrono::high_resolution_clock::now();
      auto s = std::chrono::duration_cast<std::chrono::duration<float, std::ratio<1, 1>>>(t - m_frameTime);

      m_frameDelta = s.count();
      m_frameTime = t;

      m_dir = Vector3D(normalize(computeRotationQuaternion(Vector3D(0.0f, 1.0f, 0.0f), deltaRot[0] * m_frameDelta).apply(Vector4D(m_dir, 0.0f))));
      m_dir = Vector3D(normalize(computeRotationQuaternion(cross(Vector3D(0.0f, 1.0f, 0.0f), m_dir), deltaRot[1] * m_frameDelta).apply(Vector4D(m_dir, 0.0f))));

      Vector3D zDir = normalize(m_dir);
      Vector3D yDir(0.0f, 1.0f, 0.0f);
      Vector3D xDir = cross(yDir, zDir);

      m_eye += xDir * m_frameDelta * deltaX;
      m_eye += yDir * m_frameDelta * deltaY;
      m_eye += zDir * m_frameDelta * deltaZ;

      m_presenter->present([this] (const GfxPresenterContext& args) {
        GfxContext context = args.getContext();
        GfxImage image = args.getImage();

        // Update absolute bone transforms
        updateTransformBuffer(context);

        // Initialize swap chain image and prepare it for rendering
        context->imageBarrier(image, image->getAvailableSubresources(),
          0, 0, GfxUsage::eRenderTarget, 0, GfxBarrierFlag::eDiscard);

        // Create depth image as necessary
        Extent2D extent(image->computeMipExtent(0));

        if (m_depthImage == nullptr || Extent2D(m_depthImage->computeMipExtent(0)) != extent)
          m_depthImage = createDepthImage(extent);

        // Create an image view for rendering
        GfxImageViewDesc viewDesc;
        viewDesc.type = GfxImageViewType::e2D;
        viewDesc.format = image->getDesc().format;
        viewDesc.subresource = image->getAvailableSubresources();
        viewDesc.usage = GfxUsage::eRenderTarget;

        GfxRenderingInfo renderInfo;
        renderInfo.color[0].op = GfxRenderTargetOp::eClear;
        renderInfo.color[0].view = image->createView(viewDesc);
        renderInfo.color[0].clearValue = GfxColorValue(1.0f, 1.0f, 1.0f, 1.0f);

        viewDesc.format = m_depthImage->getDesc().format;
        viewDesc.subresource = m_depthImage->getAvailableSubresources();

        renderInfo.depthStencil.depthOp = GfxRenderTargetOp::eClear;
        renderInfo.depthStencil.view = m_depthImage->createView(viewDesc);
        renderInfo.depthStencil.clearValue = GfxDepthStencilValue(0.0f, 0);

        context->beginRendering(renderInfo, 0);
        context->setViewport(GfxViewport(Offset2D(0, 0), extent));

        bindConstantBuffer(context, extent);
        bindGeometryBuffer(context);
        bindPipeline(context);

        // Set mesh index
        for (uint32_t i = 0; i < m_geometry->meshes.size(); i++) {
          context->setShaderConstants(8, uint32_t(i));

          Extent3D workgroupCount = gfxComputeWorkgroupCount(Extent3D(
              m_geometry->meshes[i].info.maxMeshletCount,
              m_geometry->meshes[i].info.instanceCount, 1),
            m_msPipeline->getWorkgroupSize());

          context->drawMesh(workgroupCount);
        }

        context->endRendering();

        // Prepare the swap chain image for presentation
        context->imageBarrier(image, image->getAvailableSubresources(),
          GfxUsage::eRenderTarget, 0, GfxUsage::ePresent, 0, 0);
      });
    }

    m_device->waitIdle();
  }

private:

  Io                    m_io;

  Wsi                   m_wsi;
  Gfx                   m_gfx;

  WsiWindow             m_window;
  GfxDevice             m_device;

  GfxPresenter          m_presenter;
  GfxTransferManager    m_transfer;

  GfxGraphicsPipeline   m_msPipeline;
  GfxComputePipeline    m_csTransform;
  GfxComputePipeline    m_csAnimate;

  GfxRenderState        m_renderState;

  GfxBuffer             m_geometryBuffer;
  GfxBuffer             m_jointTransforms;
  GfxBuffer             m_jointRelative;
  GfxBuffer             m_morphWeights;
  GfxBuffer             m_animation;

  GfxImage              m_depthImage;

  float                 m_x = 0.0f;
  float                 m_y = 0.0f;
  float                 m_z = 0.0f;

  float                 m_xRot = 0.0f;
  float                 m_yRot = 0.0f;

  float                 m_step = 0.0f;
  float                 m_frameDelta = 0.0f;
  uint32_t              m_animationIndex = 0;

  uint32_t              m_frameId = 0;

  std::chrono::high_resolution_clock::time_point m_frameTime =
    std::chrono::high_resolution_clock::now();

  Vector3D              m_eye = Vector3D(0.0f, 2.0f, 3.0f);
  Vector3D              m_dir = Vector3D(0.0f, 0.5f, 1.0f);

  float                 m_rotation = 0.0f;

  std::unique_ptr<IoArchive>                  m_archive;

  std::mutex                                  m_shaderMutex;
  std::unordered_map<std::string, GfxShader>  m_shaders;

  std::shared_ptr<GfxGeometry>                m_geometry;


  void bindPipeline(const GfxContext& context) {
    context->bindPipeline(m_msPipeline);

    context->setRenderState(m_renderState);
  }


  void bindConstantBuffer(const GfxContext& context, Extent2D imageExtent) {
    float f = 5.0f / 3.141592654f;
    float zNear = 0.001f;

    Vector3D up = Vector3D(0.0f, 1.0f, 0.0f);

    SceneConstants sceneInfo = { };
    sceneInfo.projection = computePerspectiveProjection(
      Vector2D(imageExtent), f, zNear);
    sceneInfo.viewTransform = computeViewTransform(m_eye, normalize(m_dir), up);
    sceneInfo.viewFrustum = computeViewFrustum(sceneInfo.projection);

    InstanceConstants instanceInfo;
    instanceInfo.modelTransform = QuatTransform(
      computeRotationQuaternion(Vector4D(0.0f, 1.0f, 0.0f, 0.0f), m_rotation),
      Vector4D(0.0f));
    instanceInfo.morphWeights[0] = 0.0f;
    instanceInfo.morphWeights[1] = 0.0f;

    auto cbScene = context->writeScratch(GfxUsage::eConstantBuffer, sceneInfo);
    auto cbInstance = context->writeScratch(GfxUsage::eConstantBuffer, instanceInfo);

    context->bindDescriptor(0, 0, cbScene.getDescriptor(GfxUsage::eConstantBuffer));

    std::array<GfxDescriptor, 3> descriptors = { };
    descriptors[0] = cbInstance.getDescriptor(GfxUsage::eConstantBuffer);

    if (m_jointTransforms) {
      descriptors[1] = m_jointTransforms->getDescriptor(
        GfxUsage::eShaderResource, 0, m_jointTransforms->getDesc().size);
    }
    
    if (m_morphWeights) {
      descriptors[2] = m_morphWeights->getDescriptor(
        GfxUsage::eShaderResource, 0, m_morphWeights->getDesc().size);
    }

    context->bindDescriptors(2, 0,
      descriptors.size(),
      descriptors.data());
  }


  void bindGeometryBuffer(const GfxContext& context) {
    context->bindDescriptor(1, 0, m_geometryBuffer->getDescriptor(
      GfxUsage::eConstantBuffer, 0, m_geometry->getConstantDataSize()));
    context->setShaderConstants(0, m_geometryBuffer->getGpuAddress());
  }


  GfxRenderState createRenderState() {
    GfxCullMode cullMode = GfxCullMode::eBack;

    GfxDepthTest depthTest;
    depthTest.enableDepthWrite = true;
    depthTest.depthCompareOp = GfxCompareOp::eGreater;

    GfxRenderStateDesc desc;
    desc.cullMode = &cullMode;
    desc.depthTest = &depthTest;

    return m_device->createRenderState(desc);
  }


  GfxImage createDepthImage(Extent2D imageExtent) {
    m_device->waitIdle();

    GfxImageDesc desc;
    desc.debugName = "Depth image";
    desc.type = GfxImageType::e2D;
    desc.format = GfxFormat::eD32;
    desc.usage = GfxUsage::eRenderTarget;
    desc.extent = Extent3D(imageExtent, 1u);
    desc.samples = 1;

    return m_device->createImage(desc, GfxMemoryType::eAny);
  }


  WsiWindow createWindow() {
    WsiWindowDesc windowDesc;
    windowDesc.title = "Meshlets";
    windowDesc.surfaceType = m_gfx->getBackendType();

    return m_wsi->createWindow(windowDesc);
  }


  GfxDevice createDevice() {
    return m_gfx->createDevice(m_gfx->enumAdapters(0));
  }


  GfxPresenter createPresenter() {
    GfxPresenterDesc presenterDesc;
    presenterDesc.window = m_window;
    presenterDesc.queue = GfxQueue::eGraphics;
    presenterDesc.imageUsage = GfxUsage::eRenderTarget;

    return m_device->createPresenter(presenterDesc);
  }


  std::unique_ptr<IoArchive> loadArchive() {
    std::filesystem::path archivePath = "resources/demo_04_meshlet_resources.asa";

    IoFile file = m_io->open(archivePath, IoOpenMode::eRead);

    if (!file) {
      Log::err("Failed to open ", archivePath);
      return nullptr;
    }

    return std::make_unique<IoArchive>(file);
  }


  GfxShader findShader(const char* name) {
    auto entry = m_shaders.find(name);

    if (entry == m_shaders.end())
      return GfxShader();

    return entry->second;
  }


  IoRequest loadResources() {
    GfxShaderFormatInfo format = m_device->getShaderInfo();

    IoRequest request = m_io->createRequest();

    for (uint32_t i = 0; i < m_archive->getFileCount(); i++) {
      const IoArchiveFile* file = m_archive->getFile(i);

      if (file->getType() != FourCC('S', 'H', 'D', 'R'))
        continue;

      const IoArchiveSubFile* subFile = file->findSubFile(format.identifier);

      if (!subFile)
        continue;

      m_archive->streamCompressed(request, subFile, [this,
        cFile       = file,
        cFormat     = format.format,
        cSubFile    = subFile
      ] (const void* compressedData, size_t compressedSize) {
        GfxShaderDesc shaderDesc;
        shaderDesc.debugName = cFile->getName();

        if (!shaderDesc.deserialize(cFile->getInlineData()))
          return IoStatus::eError;

        GfxShaderBinaryDesc binaryDesc;
        binaryDesc.format = cFormat;
        binaryDesc.data.resize(cSubFile->getSize());

        if (!m_archive->decompress(cSubFile, binaryDesc.data.data(), compressedData))
          return IoStatus::eError;

        // Callbacks can be executed from worker threads, so we
        // need to lock before modifying global data structures
        std::lock_guard lock(m_shaderMutex);

        m_shaders.emplace(std::piecewise_construct,
          std::forward_as_tuple(cFile->getName()),
          std::forward_as_tuple(std::move(shaderDesc), std::move(binaryDesc)));

        Log::info("Loaded ", cFile->getName());
        return IoStatus::eSuccess;
      });
    }

    m_io->submit(request);
    return request;
  }


  std::shared_ptr<GfxGeometry> createGeometry() {
    auto file = m_archive->findFile("CesiumMan");
    auto geometry = std::make_shared<GfxGeometry>();

    if (!geometry->deserialize(file->getInlineData()))
      throw Error("Failed to deserialize geometry data");

    return geometry;
  }


  GfxBuffer createGeometryBuffer() {
    auto file = m_archive->findFile("CesiumMan");
    auto subFile = file->getSubFile(0);

    GfxBufferDesc bufferDesc;
    bufferDesc.debugName = "Geometry buffer";
    bufferDesc.size = subFile->getSize();
    bufferDesc.usage = GfxUsage::eShaderResource |
      GfxUsage::eConstantBuffer |
      GfxUsage::eDecompressionDst |
      GfxUsage::eTransferDst;

    GfxBuffer buffer = m_device->createBuffer(bufferDesc, GfxMemoryType::eAny);

    m_transfer->uploadBuffer(subFile, buffer, 0);
    m_transfer->waitForCompletion(m_transfer->flush());
    return buffer;
  }


  GfxBuffer createAnimationBuffer() {
    auto file = m_archive->findFile("CesiumMan");
    auto subFile = file->findSubFile(FourCC('A', 'N', 'I', 'M'));

    if (!subFile)
      return GfxBuffer();

    GfxBufferDesc bufferDesc;
    bufferDesc.debugName = "Animation buffer";
    bufferDesc.size = subFile->getSize();
    bufferDesc.usage = GfxUsage::eShaderResource |
      GfxUsage::eDecompressionDst |
      GfxUsage::eTransferDst;

    GfxBuffer buffer = m_device->createBuffer(bufferDesc, GfxMemoryType::eAny);

    m_transfer->uploadBuffer(subFile, buffer, 0);
    m_transfer->waitForCompletion(m_transfer->flush());
    return buffer;
  }


  GfxBuffer createTransformBuffer() {
    if (!m_geometry->info.jointCount)
      return GfxBuffer();

    GfxBufferDesc bufferDesc;
    bufferDesc.debugName = "Transforms";
    bufferDesc.usage =
      GfxUsage::eShaderStorage |
      GfxUsage::eShaderResource;
    bufferDesc.size = sizeof(QuatTransform) * m_geometry->info.jointCount;

    return m_device->createBuffer(bufferDesc, GfxMemoryType::eAny);
  }


  GfxBuffer createMorphTargetBuffer() {
    if (!m_geometry->info.morphTargetCount)
      return GfxBuffer();

    GfxBufferDesc bufferDesc;
    bufferDesc.debugName = "Morph targets";
    bufferDesc.usage =
      GfxUsage::eShaderStorage |
      GfxUsage::eShaderResource;
    bufferDesc.size = sizeof(float) * m_geometry->info.morphTargetCount;

    return m_device->createBuffer(bufferDesc, GfxMemoryType::eAny);
  }


  void updateTransformBuffer(const GfxContext& context) {
    size_t jointDataSize = sizeof(QuatTransform) * m_geometry->info.jointCount;
    size_t morphDataSize = sizeof(float) * m_geometry->info.morphTargetCount;

    if (m_animation) {
      context->bindPipeline(m_csAnimate);

      m_step += m_frameDelta;

      if (m_step > m_geometry->animations.at(m_animationIndex).duration) {
        m_animationIndex = (m_animationIndex + 1) % m_geometry->animations.size();
        m_step = 0.0f;
      }

      const auto& animation = m_geometry->animations.at(m_animationIndex);

      std::array<GfxDescriptor, 2> descriptors = { };

      if (jointDataSize)
        descriptors[0] = m_jointRelative->getDescriptor(GfxUsage::eShaderStorage, 0, jointDataSize);

      if (morphDataSize)
        descriptors[1] = m_morphWeights->getDescriptor(GfxUsage::eShaderStorage, 0, morphDataSize);

      context->bindDescriptors(0, 0,
        descriptors.size(),
        descriptors.data());

      context->setShaderConstants(0, m_animation->getGpuAddress());
      context->setShaderConstants(8, animation.groupIndex);
      context->setShaderConstants(12, m_step);

      context->dispatch(Extent3D(1, animation.groupCount, 1));

      context->memoryBarrier(
        GfxUsage::eShaderStorage, GfxShaderStage::eCompute,
        GfxUsage::eShaderResource, GfxShaderStage::eCompute);
    } else {
      if (jointDataSize) {
        std::vector<QuatTransform> transforms(m_geometry->info.jointCount);

        for (auto& transform : transforms)
          transform = QuatTransform::identity();

        auto scratch = context->writeScratch(GfxUsage::eTransferSrc,
          jointDataSize, transforms.data());

        context->copyBuffer(m_jointRelative, 0,
          scratch.buffer, scratch.offset, scratch.size);

        context->memoryBarrier(GfxUsage::eTransferDst, 0,
          GfxUsage::eShaderResource, GfxShaderStage::eCompute);
      }
    }

    if (jointDataSize) {
      context->bindPipeline(m_csTransform);
      context->setShaderConstants(0, m_geometryBuffer->getGpuAddress());

      std::array<GfxDescriptor, 2> descriptors = { };
      descriptors[0] = m_jointRelative->getDescriptor(GfxUsage::eShaderResource, 0, jointDataSize);
      descriptors[1] = m_jointTransforms->getDescriptor(GfxUsage::eShaderStorage, 0, jointDataSize);

      context->bindDescriptors(0, 0,
        descriptors.size(),
        descriptors.data());

      context->dispatch(Extent3D(1, 1, 1));
    }

    context->memoryBarrier(
      GfxUsage::eShaderStorage | GfxUsage::eTransferDst, GfxShaderStage::eCompute,
      GfxUsage::eShaderResource, GfxShaderStage::eTask | GfxShaderStage::eMesh);
  }

};


int main(int argc, char** argv) {
  try {
    MeshletApp app;
    app.run();
    return 0;
  } catch (const Error& e) {
    Log::err(e.what());
    return 1;
  }
}
