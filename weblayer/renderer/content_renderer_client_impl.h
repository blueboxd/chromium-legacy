// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_RENDERER_CONTENT_RENDERER_CLIENT_IMPL_H_
#define WEBLAYER_RENDERER_CONTENT_RENDERER_CLIENT_IMPL_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "content/public/renderer/content_renderer_client.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"

class SpellCheck;

namespace service_manager {
class LocalInterfaceProvider;
}  // namespace service_manager

namespace weblayer {

class ContentRendererClientImpl : public content::ContentRendererClient {
 public:
  ContentRendererClientImpl();
  ~ContentRendererClientImpl() override;

  // content::ContentRendererClient:
  void RenderThreadStarted() override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  bool HasErrorPage(int http_status_code) override;
  void PrepareErrorPage(content::RenderFrame* render_frame,
                        const blink::WebURLError& error,
                        const std::string& http_method,
                        std::string* error_html) override;
  std::unique_ptr<content::URLLoaderThrottleProvider>
  CreateURLLoaderThrottleProvider(
      content::URLLoaderThrottleProviderType provider_type) override;

 private:
#if defined(OS_ANDROID)
  std::unique_ptr<service_manager::LocalInterfaceProvider>
      local_interface_provider_;
  std::unique_ptr<SpellCheck> spellcheck_;
#endif

  scoped_refptr<blink::ThreadSafeBrowserInterfaceBrokerProxy>
      browser_interface_broker_;

  DISALLOW_COPY_AND_ASSIGN(ContentRendererClientImpl);
};

}  // namespace weblayer

#endif  // WEBLAYER_RENDERER_CONTENT_RENDERER_CLIENT_IMPL_H_
