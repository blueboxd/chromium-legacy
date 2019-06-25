// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/browser/content_capture_receiver.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "components/content_capture/browser/content_capture_receiver_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace content_capture {

ContentCaptureReceiver::ContentCaptureReceiver(content::RenderFrameHost* rfh)
    : bindings_(this), rfh_(rfh), id_(GetIdFrom(rfh)) {}

ContentCaptureReceiver::~ContentCaptureReceiver() {
  auto* manager = ContentCaptureReceiverManager::FromWebContents(
      content::WebContents::FromRenderFrameHost(rfh_));
  DCHECK(manager);
  manager->DidRemoveSession(this);
}

int64_t ContentCaptureReceiver::GetIdFrom(content::RenderFrameHost* rfh) {
  return static_cast<int64_t>(rfh->GetProcess()->GetID()) << 32 |
         (rfh->GetRoutingID() & 0xFFFFFFFF);
}

void ContentCaptureReceiver::BindRequest(
    mojom::ContentCaptureReceiverAssociatedRequest request) {
  bindings_.Bind(std::move(request));
}

void ContentCaptureReceiver::DidCaptureContent(const ContentCaptureData& data,
                                               bool first_data) {
  auto* manager = ContentCaptureReceiverManager::FromWebContents(
      content::WebContents::FromRenderFrameHost(rfh_));

  if (first_data) {
    // The session id of this frame isn't changed for new document navigation,
    // so the previous session should be terminated.
    // The parent frame might be captured after child, we need to check if url
    // is changed, otherwise the child frame's session will be removed.
    if (frame_content_capture_data_.id != 0 &&
        frame_content_capture_data_.value != data.value) {
      manager->DidRemoveSession(this);
    }

    frame_content_capture_data_.id = id_;
    // Copies everything except id and children.
    frame_content_capture_data_.value = data.value;
    frame_content_capture_data_.bounds = data.bounds;
  }
  // We can't avoid copy the data here, because id need to be overridden.
  ContentCaptureData content(data);
  content.id = id_;
  // Always have frame URL attached, since the ContentCaptureConsumer will
  // be reset once activity is resumed, URL is needed to rebuild session.
  if (!first_data)
    content.value = frame_content_capture_data_.value;
  manager->DidCaptureContent(this, content);
}

void ContentCaptureReceiver::DidUpdateContent(const ContentCaptureData& data) {
  auto* manager = ContentCaptureReceiverManager::FromWebContents(
      content::WebContents::FromRenderFrameHost(rfh_));
  // We can't avoid copy the data here, because id need to be overridden.
  ContentCaptureData content(data);
  content.id = id_;
  content.value = frame_content_capture_data_.value;
  manager->DidUpdateContent(this, content);
}

void ContentCaptureReceiver::DidRemoveContent(
    const std::vector<int64_t>& data) {
  auto* manager = ContentCaptureReceiverManager::FromWebContents(
      content::WebContents::FromRenderFrameHost(rfh_));
  manager->DidRemoveContent(this, data);
}

void ContentCaptureReceiver::StartCapture() {
  if (content_capture_enabled_)
    return;

  if (auto& sender = GetContentCaptureSender()) {
    sender->StartCapture();
    content_capture_enabled_ = true;
  }
}

void ContentCaptureReceiver::StopCapture() {
  if (!content_capture_enabled_)
    return;

  if (auto& sender = GetContentCaptureSender()) {
    sender->StopCapture();
    content_capture_enabled_ = false;
  }
}

const mojom::ContentCaptureSenderAssociatedPtr&
ContentCaptureReceiver::GetContentCaptureSender() {
  if (!content_capture_sender_) {
    rfh_->GetRemoteAssociatedInterfaces()->GetInterface(
        mojo::MakeRequest(&content_capture_sender_));
  }
  return content_capture_sender_;
}

const ContentCaptureData& ContentCaptureReceiver::GetFrameContentCaptureData() {
  base::string16 url = base::UTF8ToUTF16(rfh_->GetLastCommittedURL().spec());
  if (url == frame_content_capture_data_.value)
    return frame_content_capture_data_;

  if (frame_content_capture_data_.id != 0) {
    auto* manager = ContentCaptureReceiverManager::FromWebContents(
        content::WebContents::FromRenderFrameHost(rfh_));
    manager->DidRemoveSession(this);
  }

  frame_content_capture_data_.id = id_;
  frame_content_capture_data_.value = url;
  const base::Optional<gfx::Size>& size = rfh_->GetFrameSize();
  if (size.has_value())
    frame_content_capture_data_.bounds = gfx::Rect(size.value());
  return frame_content_capture_data_;
}

}  // namespace content_capture
