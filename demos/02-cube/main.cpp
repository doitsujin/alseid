#include <chrono>
#include <cmath>
#include <initializer_list>
#include <mutex>
#include <unordered_map>

#include "../../src/gfx/gfx.h"
#include "../../src/gfx/gfx_transfer.h"

#include "../../src/io/io.h"
#include "../../src/io/io_archive.h"

#include "../../src/util/util_error.h"
#include "../../src/util/util_log.h"
#include "../../src/util/util_math.h"

#include "../../src/wsi/wsi.h"

using namespace as;

struct Vertex {
  Vector3D position;
  Vector3D normal;
  Vector2D coord;
};

const std::array<Vertex, 24> g_vertexData = {
  Vertex { Vector3D(-1.0f, -1.0f, -1.0f), Vector3D(-1.0f,  0.0f,  0.0f), Vector2D(0.0f, 0.0f) },
  Vertex { Vector3D(-1.0f,  1.0f, -1.0f), Vector3D(-1.0f,  0.0f,  0.0f), Vector2D(1.0f, 0.0f) },
  Vertex { Vector3D(-1.0f,  1.0f,  1.0f), Vector3D(-1.0f,  0.0f,  0.0f), Vector2D(1.0f, 1.0f) },
  Vertex { Vector3D(-1.0f, -1.0f,  1.0f), Vector3D(-1.0f,  0.0f,  0.0f), Vector2D(0.0f, 1.0f) },

  Vertex { Vector3D( 1.0f, -1.0f, -1.0f), Vector3D( 1.0f,  0.0f,  0.0f), Vector2D(0.0f, 0.0f) },
  Vertex { Vector3D( 1.0f,  1.0f, -1.0f), Vector3D( 1.0f,  0.0f,  0.0f), Vector2D(1.0f, 0.0f) },
  Vertex { Vector3D( 1.0f,  1.0f,  1.0f), Vector3D( 1.0f,  0.0f,  0.0f), Vector2D(1.0f, 1.0f) },
  Vertex { Vector3D( 1.0f, -1.0f,  1.0f), Vector3D( 1.0f,  0.0f,  0.0f), Vector2D(0.0f, 1.0f) },

  Vertex { Vector3D(-1.0f, -1.0f, -1.0f), Vector3D( 0.0f, -1.0f,  0.0f), Vector2D(0.0f, 0.0f) },
  Vertex { Vector3D( 1.0f, -1.0f, -1.0f), Vector3D( 0.0f, -1.0f,  0.0f), Vector2D(1.0f, 0.0f) },
  Vertex { Vector3D( 1.0f, -1.0f,  1.0f), Vector3D( 0.0f, -1.0f,  0.0f), Vector2D(1.0f, 1.0f) },
  Vertex { Vector3D(-1.0f, -1.0f,  1.0f), Vector3D( 0.0f, -1.0f,  0.0f), Vector2D(0.0f, 1.0f) },

  Vertex { Vector3D(-1.0f,  1.0f, -1.0f), Vector3D( 0.0f,  1.0f,  0.0f), Vector2D(0.0f, 0.0f) },
  Vertex { Vector3D( 1.0f,  1.0f, -1.0f), Vector3D( 0.0f,  1.0f,  0.0f), Vector2D(1.0f, 0.0f) },
  Vertex { Vector3D( 1.0f,  1.0f,  1.0f), Vector3D( 0.0f,  1.0f,  0.0f), Vector2D(1.0f, 1.0f) },
  Vertex { Vector3D(-1.0f,  1.0f,  1.0f), Vector3D( 0.0f,  1.0f,  0.0f), Vector2D(0.0f, 1.0f) },

  Vertex { Vector3D(-1.0f, -1.0f, -1.0f), Vector3D( 0.0f,  0.0f, -1.0f), Vector2D(0.0f, 0.0f) },
  Vertex { Vector3D( 1.0f, -1.0f, -1.0f), Vector3D( 0.0f,  0.0f, -1.0f), Vector2D(1.0f, 0.0f) },
  Vertex { Vector3D( 1.0f,  1.0f, -1.0f), Vector3D( 0.0f,  0.0f, -1.0f), Vector2D(1.0f, 1.0f) },
  Vertex { Vector3D(-1.0f,  1.0f, -1.0f), Vector3D( 0.0f,  0.0f, -1.0f), Vector2D(0.0f, 1.0f) },

  Vertex { Vector3D(-1.0f, -1.0f,  1.0f), Vector3D( 0.0f,  0.0f,  1.0f), Vector2D(0.0f, 0.0f) },
  Vertex { Vector3D( 1.0f, -1.0f,  1.0f), Vector3D( 0.0f,  0.0f,  1.0f), Vector2D(1.0f, 0.0f) },
  Vertex { Vector3D( 1.0f,  1.0f,  1.0f), Vector3D( 0.0f,  0.0f,  1.0f), Vector2D(1.0f, 1.0f) },
  Vertex { Vector3D(-1.0f,  1.0f,  1.0f), Vector3D( 0.0f,  0.0f,  1.0f), Vector2D(0.0f, 1.0f) },
};

const std::array<uint16_t, 36> g_indexData = {
   0,  1,  2,  2,  3,  0,
   4,  5,  6,  6,  7,  4,
   8,  9, 10, 10, 11,  8,
  12, 13, 14, 14, 15, 12,
  16, 17, 18, 18, 19, 16,
  20, 21, 22, 22, 23, 20,
};

struct VertexGlobalConstants {
  Matrix4x4 projMatrix;
  Matrix4x4 viewMatrix;
};

struct VertexModelConstants {
  Matrix4x4 modelMatrix;
};

struct SubresourceInfo {
  uint64_t srcOffset;
  uint64_t srcSize;
  uint64_t dstOffset;
  uint64_t dstSize;
  uint32_t mipIndex;
  uint32_t mipCount;
  bool isCompressed;
};

class CubeApp {

public:

  CubeApp()
  : m_io  (IoBackend::eDefault, std::thread::hardware_concurrency())
  , m_wsi (WsiBackend::eDefault)
  , m_gfx (GfxBackend::eDefault, m_wsi,
    GfxInstanceFlag::eDebugValidation |
    GfxInstanceFlag::eDebugMarkers |
    GfxInstanceFlag::eApiValidation) {
    // Create application window
    WsiWindowDesc windowDesc;
    windowDesc.title = "Cube";
    windowDesc.surfaceType = m_gfx->getBackendType();
    m_window = m_wsi->createWindow(windowDesc);

    // Create device. Always pick the first available adapter for now.
    m_device = m_gfx->createDevice(m_gfx->enumAdapters(0));

    // Create presenter for the given window. We'll
    // perform presentation on the compute queue.
    GfxPresenterDesc presenterDesc;
    presenterDesc.window = m_window;
    presenterDesc.queue = GfxQueue::eCompute;
    presenterDesc.imageUsage = GfxUsage::eShaderStorage;

    m_presenter = m_device->createPresenter(presenterDesc);

    // Open archive file and load resources
    m_archive = std::make_unique<IoArchive>(m_io->open(m_archivePath, IoOpenMode::eRead));

    if (!(*m_archive))
      throw Error(strcat(m_archivePath, " not found").c_str());

    // Load shaders from archive file
    IoRequest shaderRequest = loadShaders();

    if (!shaderRequest || shaderRequest->wait() != IoStatus::eSuccess)
      throw Error("Failed to load shaders");

    // Create transfer manager with a 4 MB staging buffer.
    // This is tiny, but we only load one texture.
    m_transfer = GfxTransferManager(m_io, m_device, 4ull << 20);
    m_textureBatchId = loadTexture();

    // Create presentation pipeline
    GfxComputePipelineDesc computeDesc;
    computeDesc.compute = findShader("cs_present");

    m_presentPipeline = m_device->createComputePipeline(computeDesc);

    // Create one pipeline for the depth pre-pass
    GfxGraphicsPipelineDesc graphicsDesc;
    graphicsDesc.vertex = findShader("vs_cube");

    m_depthPassPipeline = m_device->createGraphicsPipeline(graphicsDesc);

    // And one pipeline for the shading pass
    graphicsDesc.vertex = findShader("vs_cube");
    graphicsDesc.fragment = findShader("fs_cube");

    m_colorPassPipeline = m_device->createGraphicsPipeline(graphicsDesc);

    // Create samplers for presentation
    m_samplerLinear = createSampler("Linear", GfxFilter::eLinear);
    m_samplerNearest = createSampler("Nearest", GfxFilter::eNearest);

    // Create render targets
    createRenderTargets();

    // Create and upload geometry
    m_geometryBuffer = createGeometryBuffer();
    m_vertexDescriptor = m_geometryBuffer->getDescriptor(GfxUsage::eVertexBuffer, 0, sizeof(g_vertexData));
    m_indexDescriptor = m_geometryBuffer->getDescriptor(GfxUsage::eVertexBuffer, sizeof(g_vertexData), sizeof(g_indexData));

    std::memcpy(m_geometryBuffer->map(GfxUsage::eCpuWrite, 0), g_vertexData.data(), sizeof(g_vertexData));
    std::memcpy(m_geometryBuffer->map(GfxUsage::eCpuWrite, sizeof(g_vertexData)), g_indexData.data(), sizeof(g_indexData));

    // Create vertex input state object
    GfxVertexInputStateDesc vertexInputDesc;
    vertexInputDesc.primitiveTopology = GfxPrimitiveType::eTriangleList;
    vertexInputDesc.attributes[0].binding = 0;
    vertexInputDesc.attributes[0].format = GfxFormat::eR32G32B32f;
    vertexInputDesc.attributes[0].offset = offsetof(Vertex, position);

    vertexInputDesc.attributes[1].binding = 0;
    vertexInputDesc.attributes[1].format = GfxFormat::eR32G32B32f;
    vertexInputDesc.attributes[1].offset = offsetof(Vertex, normal);

    vertexInputDesc.attributes[2].binding = 0;
    vertexInputDesc.attributes[2].format = GfxFormat::eR32G32f;
    vertexInputDesc.attributes[2].offset = offsetof(Vertex, coord);

    m_viState = m_device->createVertexInputState(vertexInputDesc);

    // Create depth-stencil state objects
    GfxDepthStencilStateDesc depthStencilDesc;
    depthStencilDesc.enableDepthWrite = true;
    depthStencilDesc.depthCompareOp = GfxCompareOp::eGreater;

    m_dsDepthPass = m_device->createDepthStencilState(depthStencilDesc);

    depthStencilDesc.enableDepthWrite = false;
    depthStencilDesc.depthCompareOp = GfxCompareOp::eEqual;

    m_dsColorPass = m_device->createDepthStencilState(depthStencilDesc);

    // Create the global descriptor array
    GfxDescriptorArrayDesc descriptorArrayDesc;
    descriptorArrayDesc.debugName = "Bindless set";
    descriptorArrayDesc.bindingType = GfxShaderBindingType::eResourceImageView;
    descriptorArrayDesc.descriptorCount = 1024;

    m_descriptorArray = m_device->createDescriptorArray(descriptorArrayDesc);

    // Create texture view and write it to the array. We do not
    // need to wait for the data upload to finish to do this.
    GfxImageViewDesc textureViewDesc;
    textureViewDesc.type = GfxImageViewType::e2D;
    textureViewDesc.format = m_texture->getDesc().format;
    textureViewDesc.usage = GfxUsage::eShaderResource;
    textureViewDesc.subresource = m_texture->getAvailableSubresources();

    GfxImageView textureView = m_texture->createView(textureViewDesc);
    m_descriptorArray->setDescriptor(m_textureIndex, textureView->getDescriptor());

    // Create context objects
    for (auto& context : m_contexts)
      context = m_device->createContext(GfxQueue::eGraphics);

    // Create timeline semaphores for GPU->CPU synchronization
    m_graphicsSemaphore = createSemaphore("Graphics timeline", m_graphicsTimeline);
    m_computeSemaphore = createSemaphore("Compute timeline", m_computeTimeline);
  }

  ~CubeApp() {
    // Wait for the GPU to finish all work before
    // any objects get destroyed.
    m_device->waitIdle();
  }

  int run() {
    m_startTime = std::chrono::high_resolution_clock::now();

    bool quit = false;

    while (!quit) {
      m_wsi->processEvents([&quit] (const WsiEvent& e) {
        quit |= e.type == WsiEventType::eQuitApp
             || e.type == WsiEventType::eWindowClose;
      });

      beginFrame();
      renderDepthPass();
      renderColorPass();
      present();
    }

    return 0;
  }

  void beginFrame() {
    // Compute frame time for animation purposes
    auto curTime = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::duration<float, std::ratio<1>>>(curTime - m_startTime).count();

    // Wait for GPU work on the current frame's context to complete.
    GfxContext context = m_contexts[m_contextId];
    m_graphicsSemaphore->wait(m_graphicsTimeline - m_contexts.size() + 1);

    // Now that it's safe to do so, reset the context so we can use it.
    context->reset();

    // Initialize the texture once it has finished loading.
    m_textureInitialized = initTexture(m_contexts[m_contextId]);

    // Compute view and projection matrixes and allocate constant buffer
    // data for them. This works because allocated scratch memory remains
    // valid until the context gets reset.
    float f = 5.0f / 3.141592654f;
    float zNear = 0.001f;

    float th = 3.141592654f / 6.0f;

    VertexGlobalConstants constants = { };
    constants.projMatrix = computeProjectionMatrix(
      Vector2D(m_renderTargetSize), f, zNear);
    constants.viewMatrix = computeViewMatrix(
      Vector3D(0.0f, 2.0f, 3.0f),
      normalize(Vector3D(0.0f, 0.5f, 1.0f)),
      Vector3D(0.0f, 1.0f, 0.0f));

    GfxScratchBuffer vertexGlobalBuffer = context->writeScratch(GfxUsage::eConstantBuffer, constants);
    m_vertexGlobalConstants = vertexGlobalBuffer.getDescriptor(GfxUsage::eConstantBuffer);

    // Compute model matrix. We'll allocate UBO data inside
    // the render functions for demonstration purposes.
    th = elapsed * 3.141592654f / 2.0f;

    m_modelMatrix = computeRotationMatrix(normalize(Vector3D(0.5f, 1.0f, 0.2f)), th);
  }

  void renderDepthPass() {
    GfxContext context = m_contexts[m_contextId];
    context->beginDebugLabel("Depth pass", GfxColorValue(0.5f, 0.8f, 1.0f, 1.0f));

    // Initialize depth image and clear to zero.
    // Transition depth image to read-only mode
    context->imageBarrier(m_depthImageMs,
      m_depthImageMs->getAvailableSubresources(),
      0, 0, GfxUsage::eRenderTarget, 0,
      GfxBarrierFlag::eDiscard);

    // Depth image view properties
    GfxImageViewDesc depthViewDesc;
    depthViewDesc.type = GfxImageViewType::e2D;
    depthViewDesc.format = m_depthImageMs->getDesc().format;
    depthViewDesc.subresource = m_depthImageMs->getAvailableSubresources();
    depthViewDesc.usage = GfxUsage::eRenderTarget;

    GfxRenderingInfo renderInfo;
    renderInfo.depthStencil.depthOp = GfxRenderTargetOp::eClear;
    renderInfo.depthStencil.view = m_depthImageMs->createView(depthViewDesc);
    renderInfo.depthStencil.readOnlyAspects = GfxImageAspect::eDepth;
    renderInfo.depthStencil.clearValue = GfxDepthStencilValue(0.0f, 0);

    context->beginRendering(renderInfo, 0);

    // Render actual geometry
    context->setViewport(GfxViewport(Offset2D(0, 0), m_renderTargetSize));

    context->bindPipeline(m_depthPassPipeline);
    context->setVertexInputState(m_viState);
    context->setDepthStencilState(m_dsDepthPass);

    VertexModelConstants vertexModelConstantData = { };
    vertexModelConstantData.modelMatrix = m_modelMatrix;

    GfxScratchBuffer vertexModelBuffer = context->writeScratch(GfxUsage::eConstantBuffer, vertexModelConstantData);
    GfxDescriptor vertexModelConstants = vertexModelBuffer.getDescriptor(GfxUsage::eConstantBuffer);

    context->bindDescriptor(1, 0, m_vertexGlobalConstants);
    context->bindDescriptor(1, 1, vertexModelConstants);

    context->bindIndexBuffer(m_indexDescriptor, GfxFormat::eR16ui);
    context->bindVertexBuffer(0, m_vertexDescriptor, sizeof(Vertex));

    if (m_textureInitialized)
      context->drawIndexed(36, 1, 0, 0, 0);

    context->endRendering();

    // Transition depth aspect to read-only mode for the color pass
    context->imageBarrier(m_depthImageMs,
      m_depthImageMs->getAvailableSubresources().pickAspects(GfxImageAspect::eDepth),
      GfxUsage::eRenderTarget, 0,
      GfxUsage::eRenderTarget | GfxUsage::eShaderResource, 0, 0);

    context->endDebugLabel();

    // Submit command list
    GfxCommandSubmission submission;
    submission.addCommandList(context->endCommandList());

    m_device->submit(GfxQueue::eGraphics, std::move(submission));
  }

  void renderColorPass() {
    GfxContext context = m_contexts[m_contextId];
    context->beginDebugLabel("Color pass", GfxColorValue(1.0f, 0.8f, 0.5f, 1.0f));

    // Initialize the color images. We do not need to
    // acquire the resolve image from the compute queue
    // since we'll discard its contents.
    context->imageBarrier(m_colorImageMs,
      m_colorImageMs->getAvailableSubresources(),
      0, 0, GfxUsage::eRenderTarget, 0,
      GfxBarrierFlag::eDiscard);

    context->imageBarrier(m_colorImage,
      m_colorImage->getAvailableSubresources(),
      0, 0, GfxUsage::eRenderTarget, 0,
      GfxBarrierFlag::eDiscard);

    // Color and resolve image view properties
    GfxImageViewDesc colorViewDesc;
    colorViewDesc.type = GfxImageViewType::e2D;
    colorViewDesc.format = m_colorImage->getDesc().format;
    colorViewDesc.subresource = m_colorImage->getAvailableSubresources();
    colorViewDesc.usage = GfxUsage::eRenderTarget;

    // Depth image view properties
    GfxImageViewDesc depthViewDesc;
    depthViewDesc.type = GfxImageViewType::e2D;
    depthViewDesc.format = m_depthImageMs->getDesc().format;
    depthViewDesc.subresource = m_depthImageMs->getAvailableSubresources();
    depthViewDesc.usage = GfxUsage::eRenderTarget;

    // Begin rendering and clear the color image to grey.
    GfxRenderingInfo renderInfo;
    renderInfo.color[0].op = GfxRenderTargetOp::eClear;
    renderInfo.color[0].view = m_colorImageMs->createView(colorViewDesc);
    renderInfo.color[0].resolveView = m_colorImage->createView(colorViewDesc);
    renderInfo.color[0].clearValue = GfxColorValue(0.5f, 0.5f, 0.5f, 0.5f);

    renderInfo.depthStencil.depthOp = GfxRenderTargetOp::eLoad;
    renderInfo.depthStencil.view = m_depthImageMs->createView(depthViewDesc);
    renderInfo.depthStencil.readOnlyAspects = GfxImageAspect::eDepth;

    context->beginRendering(renderInfo, 0);

    // Render actual geometry
    context->setViewport(GfxViewport(Offset2D(0, 0), m_renderTargetSize));

    context->bindPipeline(m_colorPassPipeline);
    context->setVertexInputState(m_viState);
    context->setDepthStencilState(m_dsColorPass);

    VertexModelConstants vertexModelConstantData = { };
    vertexModelConstantData.modelMatrix = m_modelMatrix;

    GfxScratchBuffer vertexModelBuffer = context->writeScratch(GfxUsage::eConstantBuffer, vertexModelConstantData);
    GfxDescriptor vertexModelConstants = vertexModelBuffer.getDescriptor(GfxUsage::eConstantBuffer);

    context->bindDescriptorArray(0, m_descriptorArray);
    context->bindDescriptor(1, 0, m_vertexGlobalConstants);
    context->bindDescriptor(1, 1, vertexModelConstants);
    context->bindDescriptor(1, 2, m_samplerLinear->getDescriptor());

    context->bindIndexBuffer(m_indexDescriptor, GfxFormat::eR16ui);
    context->bindVertexBuffer(0, m_vertexDescriptor, sizeof(Vertex));

    context->setShaderConstants(0, m_textureIndex);

    if (m_textureInitialized)
      context->drawIndexed(36, 1, 0, 0, 0);

    context->endRendering();

    // Release resolve image so we an use it on the compute queue
    context->releaseImage(m_colorImage,
      m_colorImage->getAvailableSubresources(),
      GfxUsage::eRenderTarget, 0,
      GfxQueue::eCompute, GfxUsage::eShaderResource);

    context->endDebugLabel();

    // Prepare command submission
    GfxCommandSubmission submission;
    submission.addCommandList(context->endCommandList());

    // We actually need for compute work from the previous frame
    // to complete here since this accesses the color image
    submission.addWaitSemaphore(m_computeSemaphore, m_computeTimeline - 1);

    // And also signal the graphics semaphore since subsequent
    // compute queue work needs for rendering to complete
    submission.addSignalSemaphore(m_graphicsSemaphore, ++m_graphicsTimeline);

    m_device->submit(GfxQueue::eGraphics, std::move(submission));
  }

  void present() {
    Extent2D swapchainSize = m_renderTargetSize;

    m_presenter->present([this, &swapchainSize] (const GfxPresenterContext& args) {
      GfxContext context = args.getContext();
      GfxImage swapImage = args.getImage();

      // Wait for graphics queue operations to complete before executing any
      // present operations, then signal the compute semaphore afterwards.
      args.addWaitSemaphore(m_graphicsSemaphore, m_graphicsTimeline);
      args.addSignalSemaphore(m_computeSemaphore, ++m_computeTimeline);

      // We need to acquire the color image from the graphics queue
      // before we can read from it in the presentation shader
      context->beginDebugLabel("Presentation", GfxColorValue(1.0f, 0.5f, 0.5f, 1.0f));

      context->acquireImage(m_colorImage,
        m_colorImage->getAvailableSubresources(),
        GfxQueue::eGraphics, GfxUsage::eRenderTarget,
        GfxUsage::eShaderResource, GfxShaderStage::eCompute);

      // Initialize swap chain image and prepare it for rendering
      context->imageBarrier(swapImage, swapImage->getAvailableSubresources(),
        0, 0, GfxUsage::eShaderStorage, GfxShaderStage::eCompute,
        GfxBarrierFlag::eDiscard);

      // Create swap image view
      GfxImageViewDesc dstViewDesc;
      dstViewDesc.type = GfxImageViewType::e2D;
      dstViewDesc.format = swapImage->getDesc().format;
      dstViewDesc.subresource = swapImage->getAvailableSubresources();
      dstViewDesc.usage = GfxUsage::eShaderStorage;

      GfxImageView dstView = swapImage->createView(dstViewDesc);

      // Create source image view to read from
      GfxImageViewDesc srcViewDesc;
      srcViewDesc.type = GfxImageViewType::e2D;
      srcViewDesc.format = m_colorImage->getDesc().format;
      srcViewDesc.subresource = m_colorImage->getAvailableSubresources();
      srcViewDesc.usage = GfxUsage::eShaderResource;

      GfxImageView srcView = m_colorImage->createView(srcViewDesc);

      // Figure out which sampler to use
      swapchainSize = args.getExtent();

      GfxSampler sampler = m_renderTargetSize == swapchainSize
        ? m_samplerNearest
        : m_samplerLinear;

      // Execute the blit operation
      std::array<GfxDescriptor, 3> descriptors;
      descriptors[0] = dstView->getDescriptor();
      descriptors[1] = srcView->getDescriptor();
      descriptors[2] = sampler->getDescriptor();

      context->bindPipeline(m_presentPipeline);
      context->bindDescriptors(0, 0, descriptors.size(), descriptors.data());
      context->setShaderConstants(0, swapchainSize);
      context->dispatch(gfxComputeWorkgroupCount(
        Extent3D(swapchainSize, 1),
        m_presentPipeline->getWorkgroupSize()));

      // Prepare the swap chain image for presentation
      context->imageBarrier(swapImage, swapImage->getAvailableSubresources(),
        GfxUsage::eShaderStorage, GfxShaderStage::eCompute,
        GfxUsage::ePresent, 0, 0);

      context->endDebugLabel();
    });

    // If the swap chain has been resized, wait for all work
    // to complete and resize the render targets accordingly.
    if (m_renderTargetSize != swapchainSize) {
      m_computeSemaphore->wait(m_computeTimeline);
      m_renderTargetSize = swapchainSize;

      createRenderTargets();
    }

    // Advance to next context and color image
    m_contextId = (m_contextId + 1) % m_contexts.size();
  }

private:

  Io                        m_io;
  Wsi                       m_wsi;
  Gfx                       m_gfx;

  WsiWindow                 m_window;
  GfxDevice                 m_device;
  GfxPresenter              m_presenter;
  GfxTransferManager        m_transfer;

  Extent2D                  m_renderTargetSize = Extent2D(1280, 720);

  GfxComputePipeline        m_presentPipeline;
  GfxGraphicsPipeline       m_depthPassPipeline;
  GfxGraphicsPipeline       m_colorPassPipeline;

  GfxBuffer                 m_geometryBuffer;

  GfxDescriptor             m_indexDescriptor = { };
  GfxDescriptor             m_vertexDescriptor = { };

  GfxDescriptor             m_vertexGlobalConstants = { };

  GfxDescriptorArray        m_descriptorArray;

  Matrix4x4                 m_modelMatrix = { };

  GfxImage                  m_depthImageMs;
  GfxImage                  m_colorImageMs;
  GfxImage                  m_colorImage;

  GfxImage                  m_texture;
  uint32_t                  m_textureIndex = 0;
  uint64_t                  m_textureBatchId = 0;
  bool                      m_textureInitialized = false;

  GfxSampler                m_samplerLinear;
  GfxSampler                m_samplerNearest;

  GfxVertexInputState       m_viState;
  GfxDepthStencilState      m_dsDepthPass;
  GfxDepthStencilState      m_dsColorPass;

  std::array<GfxContext, 3> m_contexts;
  uint32_t                  m_contextId = 0;

  std::chrono::high_resolution_clock::time_point m_startTime = { };

  std::filesystem::path     m_archivePath = "resources/demo_02_cube_resources.asa";
  std::unique_ptr<IoArchive> m_archive;

  // Initialize to context count
  GfxSemaphore              m_graphicsSemaphore;
  uint64_t                  m_graphicsTimeline = uint64_t(m_contexts.size());

  // Initialize to 1 for one frame of overlap
  GfxSemaphore              m_computeSemaphore;
  uint64_t                  m_computeTimeline = 1;

  std::mutex                                  m_shaderMutex;
  std::unordered_map<std::string, GfxShader>  m_shaders;


  IoRequest loadShaders() {
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

        return IoStatus::eSuccess;
      });
    }

    m_io->submit(request);
    return request;
  }


  uint64_t loadTexture() {
    const IoArchiveFile* file = m_archive->findFile("texture");

    if (!file) {
      Log::err("File 'texture' not found in ", m_archivePath);
      return 0;
    }

    // Read texture metadata and create texture
    GfxTextureDesc textureDesc;

    if (!textureDesc.deserialize(file->getInlineData())) {
      Log::err("Failed to read texture inline data");
      return 0;
    }

    if (!m_texture) {
      GfxImageDesc imageDesc;
      imageDesc.debugName = "Texture";
      imageDesc.usage = GfxUsage::eShaderResource | GfxUsage::eTransferDst;
      imageDesc.flags = GfxImageFlag::eSimultaneousAccess;
      textureDesc.fillImageDesc(imageDesc);

      m_texture = m_device->createImage(imageDesc, GfxMemoryType::eAny);
    }

    // Pick an arbitrary, non-zero descriptor index
    m_textureIndex = 10;

    // Assume that the texture only has one array layer for simplicity.
    for (uint32_t i = 0; i < file->getSubFileCount(); i++) {
      const IoArchiveSubFile* subFile = file->getSubFile(i);

      uint64_t mipCount = i < textureDesc.mipTailStart ? 1 : textureDesc.mips - i;

      m_transfer->uploadImage(subFile, m_texture,
        m_texture->getAvailableSubresources().pickMips(i, mipCount));
    }

    return m_transfer->flush();
  }


  bool initTexture(
    const GfxContext&                   context) {
    if (m_textureInitialized)
      return true;

    if (m_transfer->getCompletedBatchId() < m_textureBatchId)
      return false;

    // Issue a barrier so that we can use the image in the fragment shader
    context->imageBarrier(m_texture, m_texture->getAvailableSubresources(),
      GfxUsage::eTransferDst, 0,
      GfxUsage::eShaderResource, GfxShaderStage::eFragment, 0);

    return true;
  }


  GfxShader findShader(
    const char*                         name) {
    auto entry = m_shaders.find(name);

    if (entry != m_shaders.end())
      return entry->second;

    return GfxShader();
  }


  GfxFormat findFormat(
          GfxFormatFeatures             features,
    const std::initializer_list<GfxFormat>& formats) {
    for (auto f : formats) {
      if (m_device->getFormatFeatures(f).all(features))
        return f;
    }

    return GfxFormat::eUnknown;
  }


  void createRenderTargets() {
    // Free existing render targets first
    m_depthImageMs = GfxImage();
    m_colorImageMs = GfxImage();
    m_colorImage = GfxImage();

    // Find suitable formats for our use case
    GfxFormat depthFormat = findFormat(
      GfxFormatFeature::eRenderTarget,
      { GfxFormat::eD32, GfxFormat::eD24 });

    GfxFormat colorFormat = findFormat(
      GfxFormatFeature::eRenderTarget | GfxFormatFeature::eResourceImage,
      { GfxFormat::eR9G9B9E5f, GfxFormat::eR11G11B10f, GfxFormat::eR16G16B16A16f });

    // Create multisampled render targets
    GfxImageDesc desc;
    desc.debugName = "Depth image";
    desc.type = GfxImageType::e2D;
    desc.format = depthFormat;
    desc.usage = GfxUsage::eRenderTarget;
    desc.extent = Extent3D(m_renderTargetSize, 1u);
    desc.samples = 4;

    m_depthImageMs = m_device->createImage(desc, GfxMemoryType::eAny);

    desc.debugName = "Color image";
    desc.format = colorFormat;

    m_colorImageMs = m_device->createImage(desc, GfxMemoryType::eAny);

    // Create resolve image. Resolves count as render target usage.
    desc.debugName = "Resolve image";
    desc.usage = GfxUsage::eRenderTarget | GfxUsage::eShaderResource;
    desc.samples = 1;

    m_colorImage = m_device->createImage(desc, GfxMemoryType::eAny);
  }


  GfxBuffer createGeometryBuffer() {
    GfxBufferDesc desc;
    desc.debugName = "Geometry buffer";
    desc.usage = GfxUsage::eIndexBuffer | GfxUsage::eVertexBuffer | GfxUsage::eCpuWrite;
    desc.size = sizeof(g_indexData) + sizeof(g_vertexData);

    return m_device->createBuffer(desc, GfxMemoryType::eAny);
  }


  GfxSampler createSampler(
    const char*                         debugName,
          GfxFilter                     filter) {
    GfxSamplerDesc desc;
    desc.debugName = debugName;
    desc.magFilter = filter;
    desc.minFilter = filter;
    desc.addressModeU = GfxAddressMode::eClampToEdge;
    desc.addressModeV = GfxAddressMode::eClampToEdge;
    desc.addressModeW = GfxAddressMode::eClampToEdge;

    if (filter == GfxFilter::eLinear)
      desc.anisotropy = 16;

    return m_device->createSampler(desc);
  }


  GfxSemaphore createSemaphore(
    const char*                         debugName,
          uint64_t                      initialValue) {
    GfxSemaphoreDesc desc;
    desc.debugName = debugName;
    desc.initialValue = initialValue;

    return m_device->createSemaphore(desc);
  }

};

int main(int argc, char** argv) {
  try {
    CubeApp app;
    return app.run();
  } catch (const Error& e) {
    Log::err(e.what());
    return 1;
  }
}
