#include "gfx_presenter.h"

namespace as {

GfxPresenterContext::GfxPresenterContext() {

}


GfxPresenterContext::GfxPresenterContext(
        GfxContext                    context,
        GfxImage                      image,
        GfxCommandSubmission&         submission)
: m_context       (std::move(context))
, m_image         (std::move(image))
, m_submission    (&submission) {

}


GfxPresenterContext::~GfxPresenterContext() {

}

}
