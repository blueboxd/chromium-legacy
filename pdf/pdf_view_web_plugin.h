// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_VIEW_WEB_PLUGIN_H_
#define PDF_PDF_VIEW_WEB_PLUGIN_H_

#include "third_party/blink/public/web/web_plugin.h"

namespace blink {
class WebPluginContainer;
struct WebPluginParams;
}  // namespace blink

namespace chrome_pdf {

// Skeleton for a `blink::WebPlugin` to replace `OutOfProcessInstance`.
class PdfViewWebPlugin final : public blink::WebPlugin {
 public:
  explicit PdfViewWebPlugin(const blink::WebPluginParams& params);
  PdfViewWebPlugin(const PdfViewWebPlugin& other) = delete;
  PdfViewWebPlugin& operator=(const PdfViewWebPlugin& other) = delete;

  // blink::WebPlugin:
  bool Initialize(blink::WebPluginContainer* container) override;
  void Destroy() override;
  blink::WebPluginContainer* Container() const override;
  void UpdateAllLifecyclePhases(blink::DocumentUpdateReason reason) override;
  void Paint(cc::PaintCanvas* canvas, const blink::WebRect& rect) override;
  void UpdateGeometry(const blink::WebRect& window_rect,
                      const blink::WebRect& clip_rect,
                      const blink::WebRect& unobscured_rect,
                      bool is_visible) override;
  void UpdateFocus(bool focused, blink::mojom::FocusType focus_type) override;
  void UpdateVisibility(bool visibility) override;
  blink::WebInputEventResult HandleInputEvent(
      const blink::WebCoalescedInputEvent& event,
      ui::Cursor* cursor) override;
  void DidReceiveResponse(const blink::WebURLResponse& response) override;
  void DidReceiveData(const char* data, size_t data_length) override;
  void DidFinishLoading() override;
  void DidFailLoading(const blink::WebURLError& error) override;

 private:
  // Call `Destroy()` instead.
  ~PdfViewWebPlugin() override;

  blink::WebPluginContainer* container_ = nullptr;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_VIEW_WEB_PLUGIN_H_
