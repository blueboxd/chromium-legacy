// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_RENDERER_WEBLAYER_RENDER_FRAME_OBSERVER_H_
#define WEBLAYER_RENDERER_WEBLAYER_RENDER_FRAME_OBSERVER_H_

#include "base/macros.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace translate {
class TranslateAgent;
}

namespace weblayer {

// This class holds the WebLayer-specific parts of RenderFrame, and has the
// same lifetime. It is analogous to //chrome's ChromeRenderFrameObserver.
class WebLayerRenderFrameObserver : public content::RenderFrameObserver {
 public:
  explicit WebLayerRenderFrameObserver(content::RenderFrame* render_frame);

  blink::AssociatedInterfaceRegistry* associated_interfaces() {
    return &associated_interfaces_;
  }

 private:
  ~WebLayerRenderFrameObserver() override;

  // RenderFrameObserver:
  bool OnAssociatedInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle* handle) override;
  void ReadyToCommitNavigation(
      blink::WebDocumentLoader* document_loader) override;
  void DidMeaningfulLayout(blink::WebMeaningfulLayout layout_type) override;
  void OnDestruct() override;

  void CapturePageText();

  blink::AssociatedInterfaceRegistry associated_interfaces_;

  // Has the same lifetime as this object.
  translate::TranslateAgent* translate_agent_;

  DISALLOW_COPY_AND_ASSIGN(WebLayerRenderFrameObserver);
};

}  // namespace weblayer

#endif  // WEBLAYER_RENDERER_WEBLAYER_RENDER_FRAME_OBSERVER_H_
