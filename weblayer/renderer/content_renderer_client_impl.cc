// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/renderer/content_renderer_client_impl.h"

#include "weblayer/renderer/ssl_error_helper.h"

namespace weblayer {

ContentRendererClientImpl::ContentRendererClientImpl() = default;
ContentRendererClientImpl::~ContentRendererClientImpl() = default;

void ContentRendererClientImpl::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  SSLErrorHelper::Create(render_frame);
}

void ContentRendererClientImpl::PrepareErrorPage(
    content::RenderFrame* render_frame,
    const blink::WebURLError& error,
    const std::string& http_method,
    std::string* error_html) {
  auto* ssl_helper = SSLErrorHelper::GetForFrame(render_frame);
  if (ssl_helper)
    ssl_helper->PrepareErrorPage();
}

}  // namespace weblayer
