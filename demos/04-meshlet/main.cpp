#include <chrono>

#include "../../src/gfx/gfx.h"
#include "../../src/gfx/gfx_geometry.h"
#include "../../src/gfx/gfx_transfer.h"

#include "../../src/gfx/scene/gfx_scene_draw.h"
#include "../../src/gfx/scene/gfx_scene_instance.h"
#include "../../src/gfx/scene/gfx_scene_material.h"
#include "../../src/gfx/scene/gfx_scene_node.h"
#include "../../src/gfx/scene/gfx_scene_pass.h"
#include "../../src/gfx/scene/gfx_scene_pipelines.h"

#include "../../src/io/io.h"
#include "../../src/io/io_archive.h"

#include "../../src/util/util_error.h"
#include "../../src/util/util_log.h"
#include "../../src/util/util_matrix.h"
#include "../../src/util/util_quaternion.h"

#include "../../src/wsi/wsi.h"

using namespace as;

struct PushConstants {
  uint64_t drawListVa;
  uint64_t passInfoVa;
  uint64_t passGroupVa;
  uint64_t instanceVa;
  uint64_t sceneVa;
  uint32_t drawGroup;
  uint32_t frameId;
};


struct TsPayload {
  uint32_t instanceIndex;
  uint32_t passIndex;
  uint32_t drawIndex;
  uint32_t skinningDataOffset;
  GfxMeshInstance meshInstance;
  QuatTransform transforms[2];
  uint64_t meshletVa;
  uint32_t meshletOffsets[64];
  uint16_t meshletPayloads[64];
};


class MeshletApp {

public:

  MeshletApp()
  : m_io    (IoBackend::eDefault, std::thread::hardware_concurrency())
  , m_wsi   (WsiBackend::eDefault)
  , m_gfx   (GfxBackend::eDefault, m_wsi,
              GfxInstanceFlag::eApiValidation |
              GfxInstanceFlag::eDebugValidation |
              GfxInstanceFlag::eDebugMarkers)
  , m_jobs  (std::thread::hardware_concurrency()) {
    m_window = createWindow();
    m_device = createDevice();
    m_presenter = createPresenter();
    // m_presenter->setPresentMode(GfxPresentMode::eImmediate);
    m_archive = loadArchive();

    IoRequest request = loadResources();
    request->wait();

    // Initialize transfer manager
    m_transfer = GfxTransferManager(m_io, m_device, 16ull << 20);

    // Create geometry object and buffer
    m_geometry = createGeometry();
    m_geometryBuffer = createGeometryBuffer();

    // Create state objects
    m_renderState = createRenderState();

    // Initialize scene objects
    m_sceneNodeManager = std::make_unique<GfxSceneNodeManager>(m_device);
    m_scenePassManager = std::make_unique<GfxScenePassManager>(m_device);
    m_sceneInstanceManager = std::make_unique<GfxSceneInstanceManager>(m_device);
    m_scenePassGroup = std::make_unique<GfxScenePassGroupBuffer>(m_device);
    m_scenePipelines = std::make_unique<GfxScenePipelines>(m_device);
    m_sceneDrawBuffer = std::make_unique<GfxSceneDrawBuffer>(m_device);

    GfxSceneMaterialManagerDesc materialManagerDesc = { };
    m_sceneMaterialManager = std::make_unique<GfxSceneMaterialManager>(m_device, materialManagerDesc);

    initScene();
    initContexts();
  }


  void run() {
    bool quit = false;

    float deltaX = 0.0f;
    float deltaY = 0.0f;
    float deltaZ = 0.0f;

    std::array<float, 2> deltaRot = { };

    while (!quit) {
      GfxContext context = getNextContext();

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

      m_eye += xDir * m_frameDelta * deltaX * 20.0f;
      m_eye += yDir * m_frameDelta * deltaY * 20.0f;
      m_eye += zDir * m_frameDelta * deltaZ * 20.0f;

      m_rotation += m_frameDelta;

      m_sceneNodeManager->updateNodeTransform(m_sceneInstanceNode,
        QuatTransform(computeRotationQuaternion(Vector4D(0.0f, 1.0f, 0.0f, 0.0f), m_rotation), Vector4D(0.0f)));

//      updateAnimation();

      Vector3D up = Vector3D(0.0f, 1.0f, 0.0f);

      m_scenePassManager->updateRenderPassProjection(m_scenePassIndex,
        computePerspectiveProjection(Vector2D(1280.0f, 720.0f), 2.0f, 0.001f));
      m_scenePassManager->updateRenderPassTransform(m_scenePassIndex,
        computeViewTransform(m_eye, normalize(m_dir), up), false);
      m_scenePassManager->updateRenderPassViewDistance(m_scenePassIndex, 30.0f);

      m_sceneMaterialManager->updateDrawBuffer(context, *m_sceneDrawBuffer);

      // Update scene buffers appropriately
      m_scenePassGroup->resizeBuffer(*m_sceneNodeManager, 0u);

      m_sceneNodeManager->commitUpdates(context,
        *m_scenePipelines, m_frameId, m_frameId - 1);
      m_sceneInstanceManager->commitUpdates(context,
        *m_scenePipelines, m_frameId, m_frameId - 1);
      m_scenePassManager->commitUpdates(context,
        *m_scenePipelines, m_frameId, m_frameId - 1);
      m_scenePassGroup->commitUpdates(context,
        m_frameId, m_frameId - 1);

      context->memoryBarrier(
        GfxUsage::eShaderStorage | GfxUsage::eTransferDst, GfxShaderStage::eCompute,
        GfxUsage::eShaderStorage | GfxUsage::eShaderResource, GfxShaderStage::eCompute);

      m_scenePassManager->processPasses(context,
        *m_scenePipelines, *m_sceneNodeManager, m_frameId);

      GfxScenePassGroupInfo passGroup = { };
      passGroup.passBufferVa = m_scenePassManager->getGpuAddress();
      passGroup.rootNodeCount = 1;
      passGroup.rootNodes = &m_sceneRootRef;

      m_sceneNodeManager->traverseBvh(context,
        *m_scenePipelines, *m_scenePassGroup, passGroup, m_frameId, 0);

      m_sceneInstanceManager->processPassGroupAnimations(context,
        *m_scenePipelines, *m_scenePassGroup, m_frameId);
      m_sceneInstanceManager->processPassGroupInstances(context,
        *m_scenePipelines, *m_sceneNodeManager, *m_scenePassGroup, m_frameId);

      m_sceneDrawBuffer->generateDraws(context, *m_scenePipelines,
        m_scenePassManager->getGpuAddress(), *m_sceneNodeManager, *m_sceneInstanceManager, *m_scenePassGroup,
        m_frameId, 0x1, 0);

      GfxCommandSubmission submission;
      submission.addSignalSemaphore(m_semaphore, m_frameId);
      submission.addCommandList(context->endCommandList());

      m_device->submit(GfxQueue::eGraphics, std::move(submission));

      m_presenter->present([this] (const GfxPresenterContext& args) {
        GfxContext context = args.getContext();
        GfxImage image = args.getImage();

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

        context->setRenderState(m_renderState);

        TsPayload payload = { };
        payload.instanceIndex = uint32_t(m_sceneInstanceRef.index);
        payload.passIndex = m_scenePassIndex;
        payload.drawIndex = 0;
        payload.skinningDataOffset = m_geometry->meshes[0].info.skinDataOffset;
        payload.meshInstance.transform = Vector4D(0.0f, 0.0f, 0.0f, 1.0f);
        payload.meshInstance.translate = Vector3D(0.0f);
        payload.transforms[0] = QuatTransform::identity();
        payload.transforms[1] = QuatTransform::identity();
        payload.meshletVa = m_geometryBuffer->getGpuAddress();
        for (uint32_t i = 0; i < m_geometry->meshlets.size(); i++) {
          payload.meshletOffsets[i] = m_geometry->meshlets[i].headerOffset;
          payload.meshletPayloads[i] = 1 | (i << 6);
        }

        auto scratch = context->writeScratch(GfxUsage::eConstantBuffer, payload);
        context->bindDescriptor(0, 0, scratch.getDescriptor(GfxUsage::eConstantBuffer));

        m_sceneMaterialManager->dispatchDraws(context,
          *m_scenePassManager, *m_sceneInstanceManager, *m_sceneNodeManager,
          *m_scenePassGroup, *m_sceneDrawBuffer, GfxScenePassType::eMainOpaque,
          m_frameId);

        GfxSceneOcclusionTestArgs testArgs = { };
        testArgs.passInfoVa = m_scenePassManager->getGpuAddress();
        testArgs.passGroupVa = m_scenePassGroup->getGpuAddress();
        testArgs.sceneVa = m_sceneNodeManager->getGpuAddress();
        testArgs.passIndex = 0;
        testArgs.frameId = m_frameId;

        m_scenePipelines->testBvhOcclusion(context,
          m_scenePassGroup->getOcclusionTestDispatchDescriptor(),
          testArgs);

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

  GfxRenderState        m_renderState;

  GfxBuffer             m_geometryBuffer;
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

  std::chrono::high_resolution_clock::time_point m_animationStart =
    std::chrono::high_resolution_clock::now();

  Vector3D              m_eye = Vector3D(0.0f, 2.0f, 3.0f);
  Vector3D              m_dir = Vector3D(0.0f, 0.5f, 1.0f);

  float                 m_rotation = 0.0f;

  std::vector<GfxContext>                     m_contexts;
  GfxSemaphore                                m_semaphore;

  std::unique_ptr<IoArchive>                  m_archive;

  std::mutex                                  m_shaderMutex;
  std::unordered_map<std::string, GfxShader>  m_shaders;

  std::shared_ptr<GfxGeometry>                m_geometry;

  std::unique_ptr<GfxSceneNodeManager>        m_sceneNodeManager;
  std::unique_ptr<GfxScenePassManager>        m_scenePassManager;
  std::unique_ptr<GfxSceneInstanceManager>    m_sceneInstanceManager;
  std::unique_ptr<GfxScenePassGroupBuffer>    m_scenePassGroup;
  std::unique_ptr<GfxScenePipelines>          m_scenePipelines;
  std::unique_ptr<GfxSceneDrawBuffer>         m_sceneDrawBuffer;
  std::unique_ptr<GfxSceneMaterialManager>    m_sceneMaterialManager;

  uint32_t              m_sceneInstanceNode   = 0u;
  GfxSceneNodeRef       m_sceneInstanceRef    = { };
  GfxSceneNodeRef       m_sceneRootRef        = { };
  std::vector<GfxSceneNodeRef> m_instances = { };

  Jobs                  m_jobs;

  uint16_t              m_scenePassIndex      = { };

  void updateAnimation() {
    if (m_geometry->animations.empty())
      return;

    auto& animation = m_geometry->animations.at(m_animationIndex);

    auto t = std::chrono::high_resolution_clock::now();
    auto d = std::chrono::duration_cast<std::chrono::duration<float, std::ratio<1, 1>>>(t - m_animationStart);

    GfxSceneAnimationHeader animationMetadata = { };
    animationMetadata.activeAnimationCount = 1;

    GfxSceneAnimationParameters animationParameters = { };
    animationParameters.blendOp = GfxSceneAnimationBlendOp::eNone;
    animationParameters.blendChannel = 0;
    animationParameters.groupIndex = animation.groupIndex;
    animationParameters.groupCount = animation.groupCount;
    animationParameters.timestamp = d.count();

    m_sceneInstanceManager->updateAnimationMetadata(m_sceneInstanceRef, animationMetadata);
    m_sceneInstanceManager->updateAnimationParameters(m_sceneInstanceRef, 0, animationParameters);

    auto job = m_jobs->dispatch(m_jobs->create<BatchJob>(
      [this, cAnimationParameters = animationParameters, &animation, &animationMetadata] (uint32_t i) {
        GfxSceneAnimationParameters animationParameters = cAnimationParameters;
        animationParameters.timestamp += 0.2741f * float(i);

        float count = std::floor(animationParameters.timestamp / animation.duration);
        animationParameters.timestamp -= animation.duration * count;

        m_sceneInstanceManager->updateAnimationMetadata(m_instances[i], animationMetadata);
        m_sceneInstanceManager->updateAnimationParameters(m_instances[i], 0, animationParameters);
      }, m_instances.size(), 1024));

    m_jobs->wait(job);

    if (d.count() >= animation.duration) {
      m_animationIndex = (m_animationIndex + 1) % m_geometry->animations.size();
      m_animationStart = t;
    }
  }

  GfxContext getNextContext() {
    m_frameId += 1;

    if (m_frameId >= m_contexts.size())
      m_semaphore->wait(m_frameId - m_contexts.size());

    GfxContext context = m_contexts[m_frameId % m_contexts.size()];
    context->reset();
    return context;
  }

  void initContexts() {
    m_contexts.resize(3);

    for (size_t i = 0; i < m_contexts.size(); i++)
      m_contexts[i] = m_device->createContext(GfxQueue::eGraphics);

    GfxSemaphoreDesc semaphoreDesc = { };
    semaphoreDesc.debugName = "Semaphore";
    semaphoreDesc.initialValue = 0;

    m_semaphore = m_device->createSemaphore(semaphoreDesc);
  }

  void initScene() {
    GfxDeviceFeatures features = m_device->getFeatures();

    GfxSceneMaterialShaders materialShaders;
    materialShaders.passTypes = GfxScenePassType::eMainOpaque;
    materialShaders.task = findShader("ts_render");
    materialShaders.mesh = findShader("ms_material");
    materialShaders.fragment = findShader("fs_material");

    GfxSceneMaterialDesc materialDesc;
    materialDesc.debugName = "Shader pipeline";
    materialDesc.shaderCount = 1u;
    materialDesc.shaders = &materialShaders;

    if (!(features.shaderStages & GfxShaderStage::eTask)) {
      Log::err("Mesh and task shaders not supported, skipping rendering.");
      materialDesc.shaderCount = 0u;
    }

    uint32_t material = m_sceneMaterialManager->createMaterial(materialDesc);

    uint32_t rootNode = m_sceneNodeManager->createNode();

    GfxSceneBvhDesc bvhDesc = { };
    bvhDesc.nodeIndex = rootNode;

    GfxSceneNodeRef rootRef = m_sceneNodeManager->createBvhNode(bvhDesc);
    m_sceneNodeManager->updateNodeReference(rootNode, rootRef);

    std::vector<GfxSceneInstanceDrawDesc> draws;

    for (uint32_t i = 0; i < m_geometry->meshes.size(); i++) {
      auto& draw = draws.emplace_back();
      draw.materialIndex = material;
      draw.meshIndex = i;
      draw.meshInstanceCount = std::max(1u,
        uint32_t(m_geometry->meshes[i].info.instanceCount));
      draw.meshInstanceIndex = 0u;
    }

    uint32_t instanceNode = m_sceneNodeManager->createNode();

    GfxSceneInstanceResourceDesc instanceGeometryDesc = { };
    instanceGeometryDesc.name = "Geometry";
    instanceGeometryDesc.type = GfxSceneInstanceResourceType::eBufferAddress;

    GfxSceneInstanceDesc instanceDesc = { };
    instanceDesc.flags = GfxSceneInstanceFlag::eDeform;
    instanceDesc.drawCount = draws.size();
    instanceDesc.draws = draws.data();
    instanceDesc.jointCount = m_geometry->info.jointCount;
    instanceDesc.weightCount = m_geometry->info.morphTargetCount;
    instanceDesc.nodeIndex = instanceNode;
    instanceDesc.resourceCount = 1;
    instanceDesc.geometryResource = 0;
    instanceDesc.resources = &instanceGeometryDesc;
    instanceDesc.aabb = m_geometry->info.aabb;

    if (!m_geometry->animations.empty()) {
      instanceDesc.flags |= GfxSceneInstanceFlag::eAnimation;
      instanceDesc.animationCount = 1u;
    }

    GfxSceneNodeRef instanceRef = m_sceneInstanceManager->createInstance(instanceDesc);
    m_sceneNodeManager->updateNodeReference(instanceNode, instanceRef);
    m_sceneNodeManager->updateNodeTransform(instanceNode, QuatTransform::identity());
    m_sceneNodeManager->attachNodesToBvh(rootRef, 1, &instanceRef);

    m_sceneInstanceManager->updateResource(instanceRef, 0,
      GfxSceneInstanceResource::fromBufferAddress(m_geometryBuffer->getGpuAddress()));

    m_sceneMaterialManager->addInstanceDraws(*m_sceneInstanceManager, instanceRef);

    m_sceneInstanceNode = instanceNode;
    m_sceneInstanceRef = instanceRef;
    m_sceneRootRef = rootRef;

    GfxScenePassDesc passDesc;
    passDesc.flags = GfxScenePassFlag::eEnableLighting |
      GfxScenePassFlag::ePerformOcclusionTest;
    passDesc.typeMask = ~0u;

    m_scenePassIndex = m_scenePassManager->createRenderPass(passDesc);
    m_scenePassGroup->setPasses(1, &m_scenePassIndex);

    uint32_t r = 0;

    for (int32_t bz = -7; bz <= 7; bz++) {
      for (int32_t by = -7; by <= 7; by++) {
        for (int32_t bx = -7; bx <= 7; bx++) {
          if (!bz && !by && !bx)
            continue;

          uint32_t bvhNode = m_sceneNodeManager->createNode();
          bvhDesc.nodeIndex = bvhNode;
          bvhDesc.aabb.min = Vector<float16_t, 3>(-3.0_f16, -2.0_f16, -3.0_f16);
          bvhDesc.aabb.max = Vector<float16_t, 3>( 3.0_f16,  4.0_f16,  3.0_f16);

          GfxSceneNodeRef bvhRef = m_sceneNodeManager->createBvhNode(bvhDesc);
          m_sceneNodeManager->updateNodeReference(bvhNode, bvhRef);
          m_sceneNodeManager->updateNodeTransform(bvhNode,
            QuatTransform::translate(Vector3D(
              float(5 * bx), float(5 * by), float(5 * bz))));

          m_sceneNodeManager->attachNodesToBvh(rootRef, 1, &bvhRef);

          for (int32_t z = -2; z <= 2; z++) {
            for (int32_t y = -2; y <= 2; y++) {
              for (int32_t x = -2; x <= 2; x++) {
                uint32_t instanceNode = m_sceneNodeManager->createNode();
                instanceDesc.nodeIndex = instanceNode;

                GfxSceneNodeRef instanceRef = m_sceneInstanceManager->createInstance(instanceDesc);
                m_sceneNodeManager->updateNodeParent(instanceNode, bvhNode, -1);
                m_sceneNodeManager->updateNodeReference(instanceNode, instanceRef);
                m_sceneNodeManager->updateNodeTransform(instanceNode, QuatTransform(
                  computeRotationQuaternion(Vector3D(0.0f, 1.0f, 0.0f), float(r++)),
                  Vector4D(float(x), float(y), float(z), 0.0f)));
                m_sceneNodeManager->attachNodesToBvh(bvhRef, 1, &instanceRef);

                m_sceneInstanceManager->updateResource(instanceRef, 0,
                  GfxSceneInstanceResource::fromBufferAddress(m_geometryBuffer->getGpuAddress()));

                m_sceneMaterialManager->addInstanceDraws(*m_sceneInstanceManager, instanceRef);

                m_instances.push_back(instanceRef);
              }
            }
          }
        }
      }
    }
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
      GfxUsage::eDecompressionDst |
      GfxUsage::eTransferDst;

    GfxBuffer buffer = m_device->createBuffer(bufferDesc, GfxMemoryType::eAny);

    m_transfer->uploadBuffer(subFile, buffer, 0);
    m_transfer->waitForCompletion(m_transfer->flush());
    return buffer;
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
