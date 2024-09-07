#include <chrono>

#include "../../src/gfx/gfx.h"
#include "../../src/gfx/gfx_geometry.h"
#include "../../src/gfx/gfx_transfer.h"

#include "../../src/gfx/asset/gfx_asset_archive.h"

#include "../../src/gfx/common/gfx_common_hiz.h"
#include "../../src/gfx/common/gfx_common_pipelines.h"

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

    // Create state objects
    m_renderState = createRenderState();

    // Initialize scene objects
    m_assetManager = std::make_unique<GfxAssetManager>(m_device);
    m_sceneNodeManager = std::make_unique<GfxSceneNodeManager>(m_device);
    m_scenePassManager = std::make_unique<GfxScenePassManager>(m_device);
    m_sceneInstanceManager = std::make_unique<GfxSceneInstanceManager>(m_device);
    m_scenePassGroup = std::make_unique<GfxScenePassGroupBuffer>(m_device);
    m_scenePipelines = std::make_unique<GfxScenePipelines>(m_device);
    m_sceneDrawBufferPrimary = std::make_unique<GfxSceneDrawBuffer>(m_device);
    m_sceneDrawBufferSecondary = std::make_unique<GfxSceneDrawBuffer>(m_device);

    m_geometryAsset = m_assetManager->createAsset<GfxAssetGeometryFromArchive>(
      "Geometry", m_transfer, m_archive, m_archive->findFile("CesiumMan"));
    m_assetGroup = m_assetManager->createAssetGroup("Asset group", GfxAssetGroupType::eAppManaged, 1, &m_geometryAsset);
    m_assetManager->streamAssetGroup(m_assetGroup);

    GfxSceneMaterialManagerDesc materialManagerDesc = { };
    m_sceneMaterialManager = std::make_unique<GfxSceneMaterialManager>(m_device, materialManagerDesc);

    m_commonPipelines = std::make_unique<GfxCommonPipelines>(m_device);
    m_hizImage = std::make_unique<GfxCommonHizImage>(m_device);

    GfxComputePipelineDesc presentPipelineDesc;
    presentPipelineDesc.debugName = "Present blit";
    presentPipelineDesc.compute = findShader("cs_present");

    m_presentPipeline = m_device->createComputePipeline(presentPipelineDesc);

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

      if (!m_colorImage)
        createRenderTargets(context, m_window->getCurrentProperties().extent);

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

      m_rotation += m_frameDelta;

      m_sceneNodeManager->updateNodeTransform(m_sceneInstanceNode,
        QuatTransform(computeRotationQuaternion(Vector4D(0.0f, 1.0f, 0.0f, 0.0f), m_rotation), Vector4D(0.0f)));

      updateAnimation();

      Vector3D up = Vector3D(0.0f, 1.0f, 0.0f);

      m_scenePassManager->updateRenderPassProjection(m_scenePassIndex,
        computePerspectiveProjection(Vector2D(1280.0f, 720.0f), 2.0f, 0.001f));
      m_scenePassManager->updateRenderPassTransform(m_scenePassIndex,
        computeViewTransform(m_eye, normalize(m_dir), up), false);
      m_scenePassManager->updateRenderPassViewDistance(m_scenePassIndex, 30.0f);

      m_sceneMaterialManager->updateDrawBuffer(context, *m_sceneDrawBufferPrimary);
      m_sceneMaterialManager->updateDrawBuffer(context, *m_sceneDrawBufferSecondary);

      // Update scene buffers appropriately
      m_sceneNodeManager->commitUpdates(context,
        *m_scenePipelines, m_frameId, m_frameId - 1);
      m_sceneInstanceManager->commitUpdates(context,
        *m_scenePipelines, m_frameId, m_frameId - 1);
      m_scenePassManager->commitUpdates(context,
        *m_scenePipelines, m_frameId);
      m_scenePassGroup->commitUpdates(context,
        *m_sceneNodeManager);

      m_assetManager->commitUpdates(context, m_frameId, m_frameId - 1);

      context->memoryBarrier(
        GfxUsage::eShaderStorage | GfxUsage::eTransferDst, GfxShaderStage::eCompute,
        GfxUsage::eShaderStorage | GfxUsage::eShaderResource, GfxShaderStage::eCompute);

      m_scenePassManager->processPasses(context,
        *m_scenePipelines, *m_sceneNodeManager, m_frameId);

      // Perform initial BVH traversal pass
      m_sceneNodeManager->traverseBvh(context, *m_scenePipelines,
        *m_scenePassManager, *m_scenePassGroup, 1, &m_sceneRootRef,
        m_frameId, 0);

      m_sceneInstanceManager->processPassGroupInstances(context,
        *m_scenePipelines, *m_sceneNodeManager, *m_scenePassGroup,
        *m_assetManager, m_frameId);

      // Cull instances
      m_scenePassGroup->passBarrier(context);

      m_scenePassGroup->cullInstances(context, *m_scenePipelines,
        *m_sceneNodeManager, *m_sceneInstanceManager, *m_scenePassManager, m_frameId);

      // Generate initial set of draws
      m_scenePassGroup->passBarrier(context);

      m_sceneDrawBufferPrimary->generateDraws(context, *m_scenePipelines,
        m_scenePassManager->getGpuAddress(), *m_sceneNodeManager, *m_sceneInstanceManager,
        *m_scenePassGroup, m_frameId, 0x1, 0);

      // Perform initial render pass with objects visible in the previous frame.
      const GfxSceneDrawBuffer* drawBuffer = m_sceneDrawBufferPrimary.get();

      initRenderTargets(context);

      GfxImageViewDesc viewDesc;
      viewDesc.type = GfxImageViewType::e2D;
      viewDesc.format = m_colorImage->getDesc().format;
      viewDesc.subresource = m_colorImage->getAvailableSubresources();
      viewDesc.usage = GfxUsage::eRenderTarget;

      GfxRenderingInfo renderInfo;
      renderInfo.color[0].op = GfxRenderTargetOp::eClear;
      renderInfo.color[0].view = m_colorImage->createView(viewDesc);
      renderInfo.color[0].clearValue = GfxColorValue(1.0f, 1.0f, 1.0f, 1.0f);

      viewDesc.format = m_depthImage->getDesc().format;
      viewDesc.subresource = m_depthImage->getAvailableSubresources();

      renderInfo.depthStencil.depthOp = GfxRenderTargetOp::eClear;
      renderInfo.depthStencil.view = m_depthImage->createView(viewDesc);
      renderInfo.depthStencil.clearValue = GfxDepthStencilValue(0.0f, 0);

      context->beginRendering(renderInfo, 0);
      context->setViewport(GfxViewport(Offset2D(0, 0),
        Extent2D(m_colorImage->getDesc().extent)));

      context->setRenderState(m_renderState);

      m_sceneMaterialManager->dispatchDraws(context,
        *m_scenePassManager, *m_sceneInstanceManager, *m_sceneNodeManager,
        *m_scenePassGroup, 1, &drawBuffer, GfxScenePassType::eMainOpaque,
        m_frameId);

      context->endRendering();

      // Transition depth buffer and generate the Hi-Z image for occlusion testing
      context->imageBarrier(m_depthImage, m_depthImage->getAvailableSubresources(),
        GfxUsage::eRenderTarget, 0, GfxUsage::eShaderResource, GfxShaderStage::eCompute, 0);

      context->memoryBarrier(
        GfxUsage::eRenderTarget, 0,
        GfxUsage::eRenderTarget, 0);

      m_hizImage->generate(context, *m_commonPipelines, m_depthImage);

      // Perform occlusion tests and add any previously invisible
      // BVH nodes to the traversal week 
      m_scenePassGroup->performOcclusionTest(context, *m_scenePipelines,
        *m_hizImage, *m_sceneNodeManager, *m_scenePassManager, 0, m_frameId);

      // Traverse nodes made visible by the occlusion tests.
      m_sceneNodeManager->traverseBvh(context, *m_scenePipelines,
        *m_scenePassManager, *m_scenePassGroup, 0, nullptr, m_frameId, 0);

      // Process instances made visible by the secondary traversal pass
      m_sceneInstanceManager->processPassGroupInstances(context,
        *m_scenePipelines, *m_sceneNodeManager, *m_scenePassGroup,
        *m_assetManager, m_frameId);

      // Cull newly added instances
      m_scenePassGroup->passBarrier(context);

      m_scenePassGroup->cullInstances(context, *m_scenePipelines,
        *m_sceneNodeManager, *m_sceneInstanceManager, *m_scenePassManager, m_frameId);

      // Generate secondary list of draws
      m_scenePassGroup->passBarrier(context);

      m_sceneDrawBufferSecondary->generateDraws(context, *m_scenePipelines,
        m_scenePassManager->getGpuAddress(), *m_sceneNodeManager, *m_sceneInstanceManager,
        *m_scenePassGroup, m_frameId, 0x1, 0);

      // Perform secondary render pass with objects that became visible this frame
      context->imageBarrier(m_depthImage, m_depthImage->getAvailableSubresources(),
        GfxUsage::eShaderResource, GfxShaderStage::eCompute, GfxUsage::eRenderTarget, 0, 0);

      renderInfo.color[0].op = GfxRenderTargetOp::eLoad;
      renderInfo.depthStencil.depthOp = GfxRenderTargetOp::eLoad;

      context->beginRendering(renderInfo, 0);
      context->setViewport(GfxViewport(Offset2D(0, 0),
        Extent2D(m_colorImage->getDesc().extent)));

      context->setRenderState(m_renderState);

      drawBuffer = m_sceneDrawBufferSecondary.get();

      m_sceneMaterialManager->dispatchDraws(context,
        *m_scenePassManager, *m_sceneInstanceManager, *m_sceneNodeManager,
        *m_scenePassGroup, 1, &drawBuffer, GfxScenePassType::eMainOpaque,
        m_frameId);

      context->endRendering();

      // Transition rendered image so that we can read it in a shader
      context->imageBarrier(m_colorImage, m_colorImage->getAvailableSubresources(),
        GfxUsage::eRenderTarget, 0, GfxUsage::eShaderResource, GfxShaderStage::eCompute, 0);

      // Submit command list containing all the rendering work
      GfxCommandSubmission submission;
      submission.addSignalSemaphore(m_semaphore, m_frameId);
      submission.addCommandList(context->endCommandList());

      m_device->submit(GfxQueue::eGraphics, std::move(submission));

      // Present rendered frame
      m_presenter->present([this] (const GfxPresenterContext& args) {
        GfxContext context = args.getContext();
        GfxImage image = args.getImage();

        // Initialize swap chain image and prepare it for rendering
        context->imageBarrier(image, image->getAvailableSubresources(),
          0, 0, GfxUsage::eShaderStorage, GfxShaderStage::eCompute,
          GfxBarrierFlag::eDiscard);

        // Create depth image as necessary
        Extent2D extent(image->computeMipExtent(0));

        GfxImageViewDesc viewDesc;
        viewDesc.type = GfxImageViewType::e2D;
        viewDesc.format = image->getDesc().format;
        viewDesc.subresource = image->getAvailableSubresources();
        viewDesc.usage = GfxUsage::eShaderStorage;

        GfxImageView dstView = image->createView(viewDesc);

        viewDesc.format = m_colorImage->getDesc().format;
        viewDesc.subresource = m_colorImage->getAvailableSubresources();
        viewDesc.usage = GfxUsage::eShaderResource;

        GfxImageView srcView = m_colorImage->createView(viewDesc);

        context->bindPipeline(m_presentPipeline);

        context->bindDescriptor(0, 0, dstView->getDescriptor());
        context->bindDescriptor(0, 1, srcView->getDescriptor());

        context->dispatch(m_presentPipeline->computeWorkgroupCount(Extent3D(extent, 1u)));

        // Prepare the swap chain image for presentation
        context->imageBarrier(image, image->getAvailableSubresources(),
          GfxUsage::eShaderStorage, GfxShaderStage::eCompute,
          GfxUsage::ePresent, 0, 0);

        createRenderTargets(context, extent);
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

  GfxComputePipeline    m_presentPipeline;

  GfxRenderState        m_renderState;

  GfxImage              m_colorImage;
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

  std::shared_ptr<IoArchive>                  m_archive;

  std::mutex                                  m_shaderMutex;
  std::unordered_map<std::string, GfxShader>  m_shaders;

  std::unique_ptr<GfxAssetManager>            m_assetManager;
  std::unique_ptr<GfxSceneNodeManager>        m_sceneNodeManager;
  std::unique_ptr<GfxScenePassManager>        m_scenePassManager;
  std::unique_ptr<GfxSceneInstanceManager>    m_sceneInstanceManager;
  std::unique_ptr<GfxScenePassGroupBuffer>    m_scenePassGroup;
  std::unique_ptr<GfxScenePipelines>          m_scenePipelines;
  std::unique_ptr<GfxSceneDrawBuffer>         m_sceneDrawBufferPrimary;
  std::unique_ptr<GfxSceneDrawBuffer>         m_sceneDrawBufferSecondary;
  std::unique_ptr<GfxSceneMaterialManager>    m_sceneMaterialManager;
  std::unique_ptr<GfxCommonPipelines>         m_commonPipelines;
  std::unique_ptr<GfxCommonHizImage>          m_hizImage;

  GfxAssetGroup                               m_assetGroup;
  GfxAsset                                    m_geometryAsset;

  uint32_t              m_sceneInstanceNode   = 0u;
  GfxSceneNodeRef       m_sceneInstanceRef    = { };
  GfxSceneNodeRef       m_sceneRootRef        = { };
  std::vector<GfxSceneNodeRef> m_instances = { };

  Jobs                  m_jobs;

  uint16_t              m_scenePassIndex      = { };

  void updateAnimation() {
    auto geometry = m_assetManager->getAssetAs<GfxAssetGeometryIface>(m_geometryAsset)->getGeometry();

    if (geometry->animations.empty())
      return;

    auto& animation = geometry->animations.at(m_animationIndex);

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

    if (d.count() >= animation.duration) {
      m_animationIndex = (m_animationIndex + 1) % geometry->animations.size();
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
    auto geometry = m_assetManager->getAssetAs<GfxAssetGeometryIface>(m_geometryAsset)->getGeometry();

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

    for (uint32_t i = 0; i < geometry->meshes.size(); i++) {
      auto& draw = draws.emplace_back();
      draw.materialIndex = material;
      draw.meshIndex = i;
      draw.meshInstanceCount = std::max(1u,
        uint32_t(geometry->meshes[i].info.instanceCount));
      draw.maxMeshletCount = geometry->meshes[i].info.maxMeshletCount;
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
    instanceDesc.jointCount = geometry->info.jointCount;
    instanceDesc.weightCount = geometry->info.morphTargetCount;
    instanceDesc.nodeIndex = instanceNode;
    instanceDesc.resourceCount = 1;
    instanceDesc.geometryResource = 0;
    instanceDesc.resources = &instanceGeometryDesc;
    instanceDesc.aabb = geometry->info.aabb;

    if (!geometry->animations.empty()) {
      instanceDesc.flags |= GfxSceneInstanceFlag::eAnimation;
      instanceDesc.animationCount = 1u;
    }

    GfxSceneNodeRef instanceRef = m_sceneInstanceManager->createInstance(instanceDesc);
    m_sceneNodeManager->updateNodeReference(instanceNode, instanceRef);
    m_sceneNodeManager->updateNodeTransform(instanceNode, QuatTransform::identity());
    m_sceneNodeManager->attachNodesToBvh(rootRef, 1, &instanceRef);

    m_sceneInstanceManager->updateAssetList(instanceRef, m_assetManager->getAssetGroupGpuAddress(m_assetGroup));
    m_sceneInstanceManager->updateResource(instanceRef, 0,
      GfxSceneInstanceResource::fromAssetIndex(0));

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
  }


  GfxRenderState createRenderState() {
    GfxRenderStateDesc desc;
    desc.flags = GfxRenderStateFlag::eAll;
    desc.cullMode = GfxCullMode::eBack;
    desc.frontFace = GfxFrontFace::eCcw;
    desc.conservativeRaster = false;
    desc.depthTest.enableDepthWrite = true;
    desc.depthTest.depthCompareOp = GfxCompareOp::eGreater;

    return m_device->createRenderState(desc);
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
    presenterDesc.imageUsage = GfxUsage::eShaderStorage;

    return m_device->createPresenter(presenterDesc);
  }


  std::shared_ptr<IoArchive> loadArchive() {
    std::filesystem::path archivePath = "resources/demo_04_meshlet_resources.asa";

    IoFile file = m_io->open(archivePath, IoOpenMode::eRead);

    if (!file) {
      Log::err("Failed to open ", archivePath);
      return nullptr;
    }

    return std::make_shared<IoArchive>(file);
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


  void initRenderTargets(const GfxContext& context) {
    context->imageBarrier(m_depthImage, m_depthImage->getAvailableSubresources(),
      GfxUsage::eRenderTarget | GfxUsage::eShaderResource, GfxShaderStage::eCompute,
      GfxUsage::eRenderTarget, 0, GfxBarrierFlag::eDiscard);
    context->imageBarrier(m_colorImage, m_colorImage->getAvailableSubresources(),
      GfxUsage::eRenderTarget | GfxUsage::eShaderResource, GfxShaderStage::eCompute,
      GfxUsage::eRenderTarget, 0, GfxBarrierFlag::eDiscard);
  }


  void createRenderTargets(const GfxContext& context, Extent2D extent) {
    if (m_colorImage && m_colorImage->getDesc().extent.get<0, 1>() == extent)
      return;

    if (m_depthImage)
      context->trackObject(m_depthImage);

    if (m_colorImage)
      context->trackObject(m_colorImage);

    GfxImageDesc desc;
    desc.debugName = "Depth image";
    desc.type = GfxImageType::e2D;
    desc.format = GfxFormat::eD32;
    desc.usage = GfxUsage::eRenderTarget | GfxUsage::eShaderResource;
    desc.extent = Extent3D(extent, 1u);
    desc.samples = 1;

    m_depthImage = m_device->createImage(desc, GfxMemoryType::eAny);

    desc.debugName = "Color image";
    desc.format = GfxFormat::eR16G16B16A16f;

    m_colorImage = m_device->createImage(desc, GfxMemoryType::eAny);
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
