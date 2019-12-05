// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/renderer/content_renderer_client_impl.h"

#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/renderer/render_thread.h"
#include "net/base/escape.h"
#include "third_party/blink/public/platform/platform.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "weblayer/common/features.h"
#include "weblayer/renderer/ssl_error_helper.h"

#if defined(OS_ANDROID)
#include "android_webview/grit/aw_resources.h"
#include "android_webview/grit/aw_strings.h"
#include "components/spellcheck/renderer/spellcheck.h"           // nogncheck
#include "components/spellcheck/renderer/spellcheck_provider.h"  // nogncheck
#include "content/public/renderer/render_thread.h"
#include "services/service_manager/public/cpp/local_interface_provider.h"
#include "weblayer/renderer/url_loader_throttle_provider.h"
#endif

namespace weblayer {

namespace {

#if defined(OS_ANDROID)
constexpr char kThrottledErrorDescription[] =
    "Request throttled. Visit http://dev.chromium.org/throttling for more "
    "information.";

// Populates |error_html| (if it is not null), based on |error|.
// NOTE: This function is taken from
// AWContentRendererClient::PrepareErrorPage().
// TODO(1024326): If this implementation becomes the long-term
// implementation, this code should be shared rather than copied.
void PopulateErrorPageHTML(const blink::WebURLError& error,
                           std::string* error_html) {
  std::string err;
  if (error.reason() == net::ERR_TEMPORARILY_THROTTLED)
    err = kThrottledErrorDescription;
  else
    err = net::ErrorToString(error.reason());

  if (!error_html)
    return;

  // Create the error page based on the error reason.
  GURL gurl(error.url());
  std::string url_string = gurl.possibly_invalid_spec();
  int reason_id = IDS_AW_WEBPAGE_CAN_NOT_BE_LOADED;

  if (err.empty())
    reason_id = IDS_AW_WEBPAGE_TEMPORARILY_DOWN;

  std::string escaped_url = net::EscapeForHTML(url_string);
  std::vector<std::string> replacements;
  replacements.push_back(
      l10n_util::GetStringUTF8(IDS_AW_WEBPAGE_NOT_AVAILABLE));
  replacements.push_back(
      l10n_util::GetStringFUTF8(reason_id, base::UTF8ToUTF16(escaped_url)));

  // Having chosen the base reason, chose what extra information to add.
  if (reason_id == IDS_AW_WEBPAGE_TEMPORARILY_DOWN) {
    replacements.push_back(
        l10n_util::GetStringUTF8(IDS_AW_WEBPAGE_TEMPORARILY_DOWN_SUGGESTIONS));
  } else {
    replacements.push_back(err);
  }
  if (base::i18n::IsRTL())
    replacements.push_back("direction: rtl;");
  else
    replacements.push_back("");
  *error_html = base::ReplaceStringPlaceholders(
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_AW_LOAD_ERROR_HTML),
      replacements, nullptr);
}
#endif  // OS_ANDROID

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
  SSLErrorHelper::Create(render_frame);

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
  auto* ssl_helper = SSLErrorHelper::GetForFrame(render_frame);
  if (ssl_helper)
    ssl_helper->PrepareErrorPage();

#if defined(OS_ANDROID)
  PopulateErrorPageHTML(error, error_html);
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

}  // namespace weblayer
