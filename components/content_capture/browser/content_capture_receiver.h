// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CAPTURE_BROWSER_CONTENT_CAPTURE_RECEIVER_H_
#define COMPONENTS_CONTENT_CAPTURE_BROWSER_CONTENT_CAPTURE_RECEIVER_H_

#include <vector>

#include "components/content_capture/common/content_capture.mojom.h"
#include "components/content_capture/common/content_capture_data.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

namespace content {
class RenderFrameHost;
}

namespace content_capture {

// This class has an instance per RenderFrameHost, it receives messages from
// renderer and forward them to ContentCaptureReceiverManager for further
// processing.
class ContentCaptureReceiver : public mojom::ContentCaptureReceiver {
 public:
  static int64_t GetIdFrom(content::RenderFrameHost* rfh);
  explicit ContentCaptureReceiver(content::RenderFrameHost* rfh);
  ~ContentCaptureReceiver() override;

  // Binds to mojom.
  void BindRequest(mojom::ContentCaptureReceiverAssociatedRequest request);

  // mojom::ContentCaptureReceiver
  void DidCaptureContent(const ContentCaptureData& data,
                         bool first_data) override;
  void DidUpdateContent(const ContentCaptureData& data) override;
  void DidRemoveContent(const std::vector<int64_t>& data) override;
  void StartCapture();
  void StopCapture();

  content::RenderFrameHost* rfh() const { return rfh_; }

  // Return ContentCaptureData of the associated frame.
  const ContentCaptureData& GetFrameContentCaptureData();
  const ContentCaptureData& GetFrameContentCaptureDataLastSeen() const {
    return frame_content_capture_data_;
  }

 private:
  const mojom::ContentCaptureSenderAssociatedPtr& GetContentCaptureSender();

  mojo::AssociatedBinding<mojom::ContentCaptureReceiver> bindings_;
  content::RenderFrameHost* rfh_;
  ContentCaptureData frame_content_capture_data_;

  // The content id of the associated frame, it is composed of RenderProcessHost
  // unique ID and frame routing ID, and is unique in a WebContents.
  // The ID is always generated in receiver because neither does the parent
  // frame always have content, nor is its content always captured before child
  // frame's; if the Id is generated in sender, the
  // ContentCaptureReceiverManager can't get parent frame id in both cases.
  int64_t id_;
  bool content_capture_enabled_ = false;
  mojom::ContentCaptureSenderAssociatedPtr content_capture_sender_ = nullptr;
  DISALLOW_COPY_AND_ASSIGN(ContentCaptureReceiver);
};

}  // namespace content_capture

#endif  // COMPONENTS_CONTENT_CAPTURE_BROWSER_CONTENT_CAPTURE_RECEIVER_H_
