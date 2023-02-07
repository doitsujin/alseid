#include <fstream>
#include <iostream>
#include <chrono>

#include "../../src/gfx/gfx.h"
#include "../../src/gfx/gfx_spirv.h"

#include "../../src/util/util_bitstream.h"
#include "../../src/util/util_error.h"
#include "../../src/util/util_huffman.h"
#include "../../src/util/util_log.h"

#include "../../src/wsi/wsi.h"

using namespace as;

struct UboData {
  float color[4];
  int size[2];
  int dummy[2];
};

std::vector<char> loadFile(const char* path) {
  std::ifstream file(path, std::ios::binary);
  std::vector<char> data;
  char ch = '\0';
  while (file.get(ch))
    data.push_back(ch);
  return data;
}

void run_app() {
  Wsi wsi(WsiBackend::eDefault);

  Gfx gfx(GfxBackend::eDefault, wsi,
    GfxInstanceFlag::eDebugValidation |
    GfxInstanceFlag::eDebugMarkers |
    GfxInstanceFlag::eApiValidation);

  WsiWindowDesc desc;
  desc.title = "Window title";
  desc.surfaceType = gfx->getBackendType();

  WsiWindow window = wsi->createWindow(desc);
  window->setKeyboardMode(WsiKeyboardMode::eText);

  GfxDevice device = gfx->createDevice(gfx->enumAdapters(0));

  GfxPresenterDesc presenterDesc;
  presenterDesc.window = window;
  presenterDesc.queue = GfxQueue::eGraphics;
  presenterDesc.imageUsage = GfxUsage::eRenderTarget;

  GfxPresenter presenter = device->createPresenter(presenterDesc);

  GfxShaderBinaryDesc binaryDesc;
  binaryDesc.format = GfxShaderFormat::eVulkanSpirv;
  binaryDesc.data = loadFile("/home/philip/vert.spv");
  GfxShaderDesc shaderDesc = *reflectSpirvBinary(binaryDesc.data.size(), binaryDesc.data.data());
  GfxShader vs(std::move(shaderDesc), std::move(binaryDesc));

  binaryDesc.format = GfxShaderFormat::eVulkanSpirv;
  binaryDesc.data = loadFile("/home/philip/frag.spv");
  shaderDesc = *reflectSpirvBinary(binaryDesc.data.size(), binaryDesc.data.data());
  GfxShader fs(std::move(shaderDesc), std::move(binaryDesc));

  GfxGraphicsPipelineDesc graphicsDesc;
  graphicsDesc.vertex = vs;
  graphicsDesc.fragment = fs;

  GfxGraphicsPipeline graphics = device->createGraphicsPipeline(graphicsDesc);

  GfxVertexInputStateDesc vertexDesc;
  vertexDesc.primitiveTopology = GfxPrimitiveType::eTriangleList;
  vertexDesc.attributes[0].binding = 0;
  vertexDesc.attributes[0].format = GfxFormat::eR32G32f;
  vertexDesc.attributes[0].offset = 0;
  vertexDesc.attributes[0].inputRate = GfxInputRate::ePerVertex;

  GfxVertexInputState vertexState = device->createVertexInputState(vertexDesc);

  std::array<float, 6> vertexData = {{
    0.3f, 0.3f,
    0.7f, 0.3f,
    0.5f, 0.7f,
  }};

  std::array<uint32_t, 3> indexData = {{
    0, 1, 2,
  }};

  GfxBufferDesc bufferDesc;
  bufferDesc.debugName = "Vertex buffer";
  bufferDesc.usage = GfxUsage::eVertexBuffer | GfxUsage::eCpuWrite;
  bufferDesc.size = sizeof(vertexData);

  GfxBuffer vertexBuffer = device->createBuffer(bufferDesc, GfxMemoryType::eAny);
  std::memcpy(vertexBuffer->map(GfxUsage::eCpuWrite, 0), vertexData.data(), sizeof(vertexData));

  bufferDesc.debugName = "Index buffer";
  bufferDesc.usage = GfxUsage::eIndexBuffer | GfxUsage::eCpuWrite;
  bufferDesc.size = sizeof(indexData);

  GfxBuffer indexBuffer = device->createBuffer(bufferDesc, GfxMemoryType::eAny);
  std::memcpy(indexBuffer->map(GfxUsage::eCpuWrite, 0), indexData.data(), sizeof(indexData));

  GfxDescriptorArrayDesc samplerArrayDesc;
  samplerArrayDesc.debugName = "Samplers";
  samplerArrayDesc.bindingType = GfxShaderBindingType::eSampler;
  samplerArrayDesc.descriptorCount = 256;

  GfxDescriptorArray samplerArray = device->createDescriptorArray(samplerArrayDesc);

  GfxDescriptorArrayDesc textureArrayDesc;
  textureArrayDesc.debugName = "Textures";
  textureArrayDesc.bindingType = GfxShaderBindingType::eResourceImageView;
  textureArrayDesc.descriptorCount = 65536;

  GfxDescriptorArray textureArray = device->createDescriptorArray(textureArrayDesc);

  GfxSamplerDesc samplerDesc;
  samplerDesc.debugName = "hello";

  GfxSampler sampler = device->createSampler(samplerDesc);
  samplerArray->setDescriptor(0, sampler->getDescriptor());

  bool quit = false;

  while (!quit) {
    wsi->processEvents([&quit] (const WsiEvent& e) {
      quit |= e.type == WsiEventType::eQuitApp
           || e.type == WsiEventType::eWindowClose;
    });

    presenter->present([graphics, vertexState, vertexBuffer, indexBuffer, samplerArray, textureArray] (const GfxPresenterContext& args) {
      GfxContext context = args.getContext();
      GfxImage image = args.getImage();

      // Initialize swap chain image and prepare it for rendering
      context->imageBarrier(image, image->getAvailableSubresources(),
        0, 0, GfxUsage::eRenderTarget, 0, GfxBarrierFlag::eDiscard);

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

      Extent2D imageExtent = Extent2D(image->getDesc().extent);

      GfxViewport viewport;
      viewport.extent = Vector2D(imageExtent);
      viewport.scissor.extent = imageExtent;

      context->beginRendering(renderInfo, 0);
      context->bindPipeline(graphics);
      context->bindDescriptorArray(0, samplerArray);
      context->bindDescriptorArray(1, textureArray);
      context->bindIndexBuffer(indexBuffer->getDescriptor(
        GfxUsage::eIndexBuffer, 0, indexBuffer->getDesc().size),
        GfxFormat::eR32ui);
      context->bindVertexBuffer(0, vertexBuffer->getDescriptor(
        GfxUsage::eVertexBuffer, 0, vertexBuffer->getDesc().size), 8);
      context->setVertexInputState(vertexState);
      context->setViewport(viewport);
      context->drawIndexed(3, 1, 0, 0, 0);
      context->endRendering();

      // Prepare the swap chain image for presentation
      context->imageBarrier(image, image->getAvailableSubresources(),
        GfxUsage::eRenderTarget, 0, GfxUsage::ePresent, 0, 0);
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
