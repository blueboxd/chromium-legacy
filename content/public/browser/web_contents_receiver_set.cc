// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents_receiver_set.h"

#include <utility>

#include "base/notreached.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {

void WebContentsReceiverSet::Binder::OnReceiverForFrame(
    RenderFrameHost* render_frame_host,
    mojo::ScopedInterfaceEndpointHandle handle) {
  NOTREACHED();
}

void WebContentsReceiverSet::Binder::CloseAllReceivers() {
  NOTREACHED();
}

WebContentsReceiverSet::WebContentsReceiverSet(
    WebContents* web_contents,
    const std::string& interface_name)
    : remove_callback_(static_cast<WebContentsImpl*>(web_contents)
                           ->AddReceiverSet(interface_name, this)) {}

WebContentsReceiverSet::~WebContentsReceiverSet() {
  std::move(remove_callback_).Run();
}

// static
WebContentsReceiverSet* WebContentsReceiverSet::GetForWebContents(
    WebContents* web_contents,
    const char* interface_name) {
  return static_cast<WebContentsImpl*>(web_contents)
      ->GetReceiverSet(interface_name);
}

void WebContentsReceiverSet::CloseAllReceivers() {
  binder_->CloseAllReceivers();
  binder_ = nullptr;
}

void WebContentsReceiverSet::OnReceiverForFrame(
    RenderFrameHost* render_frame_host,
    mojo::ScopedInterfaceEndpointHandle handle) {
  if (binder_)
    binder_->OnReceiverForFrame(render_frame_host, std::move(handle));
}

}  // namespace content
