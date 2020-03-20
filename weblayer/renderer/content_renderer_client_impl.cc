// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/renderer/content_renderer_client_impl.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/platform/platform.h"
#include "weblayer/common/features.h"
#include "weblayer/renderer/error_page_helper.h"
#include "weblayer/renderer/weblayer_render_frame_observer.h"

#if defined(OS_ANDROID)
#include "components/android_system_error_page/error_page_populator.h"
#include "components/cdm/renderer/android_key_systems.h"
#include "components/spellcheck/renderer/spellcheck.h"           // nogncheck
#include "components/spellcheck/renderer/spellcheck_provider.h"  // nogncheck
#include "content/public/renderer/render_thread.h"
#include "services/service_manager/public/cpp/local_interface_provider.h"
#include "weblayer/renderer/url_loader_throttle_provider.h"
#endif

namespace weblayer {

namespace {

#if defined(OS_ANDROID)
class SpellcheckInterfaceProvider
    : public service_manager::LocalInterfaceProvider {
 public:
  SpellcheckInterfaceProvider() = default;
  ~SpellcheckInterfaceProvider() override = default;

  // service_manager::LocalInterfaceProvider:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle interface_pipe) override {
    // A dirty hack to make SpellCheckHost requests work on WebLayer.
    // TODO(crbug.com/806394): Use a WebView-specific service for SpellCheckHost
    // and SafeBrowsing, instead of |content_browser|.
    content::RenderThread::Get()->BindHostReceiver(mojo::GenericPendingReceiver(
        interface_name, std::move(interface_pipe)));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SpellcheckInterfaceProvider);
};
#endif  // defined(OS_ANDROID)

}  // namespace

ContentRendererClientImpl::ContentRendererClientImpl() = default;
ContentRendererClientImpl::~ContentRendererClientImpl() = default;

void ContentRendererClientImpl::RenderThreadStarted() {
#if defined(OS_ANDROID)
  if (!spellcheck_) {
    local_interface_provider_ = std::make_unique<SpellcheckInterfaceProvider>();
    spellcheck_ = std::make_unique<SpellCheck>(local_interface_provider_.get());
  }
#endif

  browser_interface_broker_ =
      blink::Platform::Current()->GetBrowserInterfaceBroker();
}

void ContentRendererClientImpl::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  auto* render_frame_observer = new WebLayerRenderFrameObserver(render_frame);

  ErrorPageHelper::Create(render_frame);

  autofill::PasswordAutofillAgent* password_autofill_agent =
      new autofill::PasswordAutofillAgent(
          render_frame, render_frame_observer->associated_interfaces());
  new autofill::AutofillAgent(render_frame, password_autofill_agent, nullptr,
                              render_frame_observer->associated_interfaces());

#if defined(OS_ANDROID)
  // |SpellCheckProvider| manages its own lifetime (and destroys itself when the
  // RenderFrame is destroyed).
  new SpellCheckProvider(render_frame, spellcheck_.get(),
                         local_interface_provider_.get());
#endif
}

bool ContentRendererClientImpl::HasErrorPage(int http_status_code) {
  return http_status_code >= 400;
}

void ContentRendererClientImpl::PrepareErrorPage(
    content::RenderFrame* render_frame,
    const blink::WebURLError& error,
    const std::string& http_method,
    std::string* error_html) {
  auto* ssl_helper = ErrorPageHelper::GetForFrame(render_frame);
  if (ssl_helper)
    ssl_helper->PrepareErrorPage();

#if defined(OS_ANDROID)
  android_system_error_page::PopulateErrorPageHtml(error, error_html);
#endif
}

std::unique_ptr<content::URLLoaderThrottleProvider>
ContentRendererClientImpl::CreateURLLoaderThrottleProvider(
    content::URLLoaderThrottleProviderType provider_type) {
  if (base::FeatureList::IsEnabled(features::kWebLayerSafeBrowsing)) {
#if defined(OS_ANDROID)
    // Note: currently the throttle provider is only needed for safebrowsing.
    return std::make_unique<URLLoaderThrottleProvider>(
        browser_interface_broker_.get(), provider_type);
#else
    return nullptr;
#endif
  }

  return nullptr;
}

void ContentRendererClientImpl::AddSupportedKeySystems(
    std::vector<std::unique_ptr<::media::KeySystemProperties>>* key_systems) {
#if defined(OS_ANDROID)
  cdm::AddAndroidWidevine(key_systems);
  cdm::AddAndroidPlatformKeySystems(key_systems);
#endif
}

}  // namespace weblayer
