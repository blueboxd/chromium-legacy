// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_URL_LOADER_THROTTLE_PROVIDER_IMPL_H_
#define CHROME_RENDERER_URL_LOADER_THROTTLE_PROVIDER_IMPL_H_

#include <memory>
#include <vector>

#include "base/threading/thread_checker.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy.mojom.h"
#include "components/safe_browsing/common/safe_browsing.mojom.h"
#include "content/public/renderer/url_loader_throttle_provider.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/renderer/extension_throttle_manager.h"
#endif

namespace data_reduction_proxy {
class DataReductionProxyThrottleManager;
}

namespace service_manager {
class Connector;
}

class ChromeContentRendererClient;

// Instances must be constructed on the render thread, and then used and
// destructed on a single thread, which can be different from the render thread.
class URLLoaderThrottleProviderImpl
    : public content::URLLoaderThrottleProvider {
 public:
  URLLoaderThrottleProviderImpl(
      service_manager::Connector* connector,
      content::URLLoaderThrottleProviderType type,
      ChromeContentRendererClient* chrome_content_renderer_client);

  ~URLLoaderThrottleProviderImpl() override;

  // content::URLLoaderThrottleProvider implementation.
  std::unique_ptr<content::URLLoaderThrottleProvider> Clone() override;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> CreateThrottles(
      int render_frame_id,
      const blink::WebURLRequest& request,
      content::ResourceType resource_type) override;
  void SetOnline(bool is_online) override;

 private:
  // This copy constructor works in conjunction with Clone(), not intended for
  // general use.
  URLLoaderThrottleProviderImpl(const URLLoaderThrottleProviderImpl& other);

  content::URLLoaderThrottleProviderType type_;
  ChromeContentRendererClient* const chrome_content_renderer_client_;

  safe_browsing::mojom::SafeBrowsingPtrInfo safe_browsing_info_;
  safe_browsing::mojom::SafeBrowsingPtr safe_browsing_;

  data_reduction_proxy::mojom::DataReductionProxyPtrInfo
      data_reduction_proxy_info_;
  data_reduction_proxy::mojom::DataReductionProxyPtr data_reduction_proxy_;
  std::unique_ptr<data_reduction_proxy::DataReductionProxyThrottleManager>
      data_reduction_proxy_manager_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::unique_ptr<extensions::ExtensionThrottleManager>
      extension_throttle_manager_;
#endif

  THREAD_CHECKER(thread_checker_);

  DISALLOW_ASSIGN(URLLoaderThrottleProviderImpl);
};

#endif  // CHROME_RENDERER_URL_LOADER_THROTTLE_PROVIDER_IMPL_H_
