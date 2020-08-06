// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_view_web_plugin.h"

#include <stddef.h>

#include "base/check_op.h"
#include "cc/paint/paint_canvas.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-shared.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "ui/base/cursor/cursor.h"

namespace chrome_pdf {

PdfViewWebPlugin::PdfViewWebPlugin(const blink::WebPluginParams& params) {}

PdfViewWebPlugin::~PdfViewWebPlugin() = default;

bool PdfViewWebPlugin::Initialize(blink::WebPluginContainer* container) {
  DCHECK_EQ(container->Plugin(), this);
  container_ = container;
  return true;
}

void PdfViewWebPlugin::Destroy() {
  container_ = nullptr;
  delete this;
}

blink::WebPluginContainer* PdfViewWebPlugin::Container() const {
  return container_;
}

void PdfViewWebPlugin::UpdateAllLifecyclePhases(
    blink::DocumentUpdateReason reason) {}

void PdfViewWebPlugin::Paint(cc::PaintCanvas* canvas,
                             const blink::WebRect& rect) {}

void PdfViewWebPlugin::UpdateGeometry(const blink::WebRect& window_rect,
                                      const blink::WebRect& clip_rect,
                                      const blink::WebRect& unobscured_rect,
                                      bool is_visible) {}

void PdfViewWebPlugin::UpdateFocus(bool focused,
                                   blink::mojom::FocusType focus_type) {}

void PdfViewWebPlugin::UpdateVisibility(bool visibility) {}

blink::WebInputEventResult PdfViewWebPlugin::HandleInputEvent(
    const blink::WebCoalescedInputEvent& event,
    ui::Cursor* cursor) {
  return blink::WebInputEventResult::kNotHandled;
}

void PdfViewWebPlugin::DidReceiveResponse(
    const blink::WebURLResponse& response) {}

void PdfViewWebPlugin::DidReceiveData(const char* data, size_t data_length) {}

void PdfViewWebPlugin::DidFinishLoading() {}

void PdfViewWebPlugin::DidFailLoading(const blink::WebURLError& error) {}

}  // namespace chrome_pdf
