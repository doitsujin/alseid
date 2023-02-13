#include "../../src/gfx/gfx.h"

#include "../../src/util/util_error.h"
#include "../../src/util/util_log.h"

#include "../../src/wsi/wsi.h"

using namespace as;

void run_app() {
  Wsi wsi(WsiBackend::eDefault);

  Gfx gfx(GfxBackend::eDefault, wsi,
    GfxInstanceFlag::eDebugValidation |
    GfxInstanceFlag::eDebugMarkers |
    GfxInstanceFlag::eApiValidation);

  WsiWindowDesc windowDesc;
  windowDesc.title = "Initialization";
  windowDesc.surfaceType = gfx->getBackendType();

  WsiWindow window = wsi->createWindow(windowDesc);
  GfxDevice device = gfx->createDevice(gfx->enumAdapters(0));

  GfxPresenterDesc presenterDesc;
  presenterDesc.window = window;
  presenterDesc.queue = GfxQueue::eGraphics;
  presenterDesc.imageUsage = GfxUsage::eRenderTarget;

  GfxPresenter presenter = device->createPresenter(presenterDesc);

  bool quit = false;

  while (!quit) {
    wsi->processEvents([&quit] (const WsiEvent& e) {
      quit |= e.type == WsiEventType::eQuitApp
           || e.type == WsiEventType::eWindowClose;
    });

    presenter->present([] (const GfxPresenterContext& args) {
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

      GfxImageView view = image->createView(viewDesc);

      GfxRenderingInfo renderInfo;
      renderInfo.color[0].op = GfxRenderTargetOp::eClear;
      renderInfo.color[0].view = view;
      renderInfo.color[0].clearValue = GfxColorValue(1.0f, 1.0f, 1.0f, 1.0f);

      context->beginRendering(renderInfo, 0);
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
