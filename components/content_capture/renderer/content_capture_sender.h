// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CAPTURE_RENDERER_CONTENT_CAPTURE_SENDER_H_
#define COMPONENTS_CONTENT_CAPTURE_RENDERER_CONTENT_CAPTURE_SENDER_H_

#include <vector>

#include "components/content_capture/common/content_capture.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "third_party/blink/public/web/web_content_capture_client.h"

namespace blink {
class AssociatedInterfaceRegistry;
}

namespace content_capture {

struct ContentCaptureData;

// This class has one instance per RenderFrame, and implements
// WebConetentCaptureClient to get the captured content and the removed
// content from blink, then forward them to browser process; it enables
// the ContentCapture in blink by setting WebContentCaptureClient to
// WebLocalFrame.
class ContentCaptureSender : public content::RenderFrameObserver,
                             public blink::WebContentCaptureClient,
                             public mojom::ContentCaptureSender {
 public:
  explicit ContentCaptureSender(content::RenderFrame* render_frame,
                                blink::AssociatedInterfaceRegistry* registry);
  ~ContentCaptureSender() override;

  void BindRequest(mojom::ContentCaptureSenderAssociatedRequest request);

  // blink::WebContentCaptureClient:
  cc::NodeHolder::Type GetNodeHolderType() const override;
  void GetTaskTimingParameters(base::TimeDelta& short_delay,
                               base::TimeDelta& long_delay) const override;
  void DidCaptureContent(
      const blink::WebVector<scoped_refptr<blink::WebContentHolder>>& data,
      bool first_data) override;
  void DidUpdateContent(
      const blink::WebVector<scoped_refptr<blink::WebContentHolder>>& data)
      override;
  void DidRemoveContent(blink::WebVector<int64_t> data) override;

  // mojom::ContentCaptureSender:
  void StartCapture() override;
  void StopCapture() override;

  // content::RenderFrameObserver:
  void OnDestruct() override;

 private:
  void FillContentCaptureData(
      const blink::WebVector<scoped_refptr<blink::WebContentHolder>>&
          node_holders,
      ContentCaptureData* data,
      bool set_url);
  const mojom::ContentCaptureReceiverAssociatedPtr& GetContentCaptureReceiver();

  mojom::ContentCaptureReceiverAssociatedPtr content_capture_receiver_ =
      nullptr;
  mojo::AssociatedBinding<mojom::ContentCaptureSender> binding_;

  DISALLOW_COPY_AND_ASSIGN(ContentCaptureSender);
};

}  // namespace content_capture

#endif  // COMPONENTS_CONTENT_CAPTURE_RENDERER_CONTENT_CAPTURE_SENDER_H_
