// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PAGE_PROPERTIES_H_
#define CONTENT_RENDERER_PAGE_PROPERTIES_H_

#include "content/common/content_export.h"
#include "content/public/common/screen_info.h"

namespace blink {
struct WebFloatRect;
struct WebRect;
}  // namespace blink

namespace content {

class CompositorDependencies;
class RenderWidgetScreenMetricsEmulator;

// This interface exposes "page global" concepts to child frames. Historically
// such state was managed by the RenderView API. However RenderView also has
// to be have as the WebViewClient as well as the IPC anchor for content. As
// such it is hard to separate what APIs are needed by blink, IPC, and other
// content renderer objects.
//
// Splitting out an explicit concept aligned with a "page" better fits the
// abstractions and clarifies which APIs can be coupled.
class CONTENT_EXPORT PageProperties {
 public:
  explicit PageProperties(CompositorDependencies* compositor_deps);
  ~PageProperties();

  const ScreenInfo& GetScreenInfo() const { return screen_info_; }
  void SetScreenInfo(const ScreenInfo& screen_info) {
    screen_info_ = screen_info;
  }

  // Convenience method for reading the device scale factor from
  // ScreenInfo. This is probably the most commonly used field in ScreenInfo
  // so having the added accessor clarifies information coupling and
  // reduces boilerplate code.
  float GetDeviceScaleFactor() { return screen_info_.device_scale_factor; }

  RenderWidgetScreenMetricsEmulator* ScreenMetricsEmulator() {
    return screen_metrics_emulator_.get();
  }
  const RenderWidgetScreenMetricsEmulator* ScreenMetricsEmulator() const {
    return screen_metrics_emulator_.get();
  }
  void SetScreenMetricsEmulator(
      std::unique_ptr<RenderWidgetScreenMetricsEmulator> emulator);

  void ConvertViewportToWindow(blink::WebRect* rect);
  void ConvertViewportToWindow(blink::WebFloatRect* rect);
  void ConvertWindowToViewport(blink::WebFloatRect* rect);

  // When emulated, this returns the original (non-emulated) ScreenInfo.
  const ScreenInfo& GetOriginalScreenInfo() const;

  CompositorDependencies* GetCompositorDependencies();

 private:
  // Properties of the screen hosting the page.
  ScreenInfo screen_info_;

  // TODO(https://crbug.com/1006052): Rename RenderWidgetScreenMetricsEmulator
  // to RenderViewScreenMetricsEmulator and reroute IPCs through RenderView
  // instead of RenderWidget.
  std::unique_ptr<RenderWidgetScreenMetricsEmulator> screen_metrics_emulator_;

  CompositorDependencies* compositor_deps_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PAGE_PROPERTIES_H_
