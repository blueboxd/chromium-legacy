// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/ssl_error_handler.h"

#include "base/threading/thread_task_runner_handle.h"
#include "components/security_interstitials/content/ssl_blocking_page.h"
#include "components/security_interstitials/content/ssl_cert_reporter.h"
#include "components/security_interstitials/content/ssl_error_navigation_throttle.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/security_interstitials/core/ssl_error_options_mask.h"
#include "components/security_interstitials/core/ssl_error_ui.h"
#include "weblayer/browser/ssl_error_controller_client.h"
#include "weblayer/browser/weblayer_content_browser_overlay_manifest.h"

namespace weblayer {

namespace {

// Constructs and shows an SSL interstitial. Adapted from //chrome's
// SSLErrorHandlerDelegateImpl::ShowSSLInterstitial().
void ShowSSLInterstitial(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    const base::Callback<void(content::CertificateRequestResultType)>&
        decision_callback,
    base::OnceCallback<
        void(std::unique_ptr<security_interstitials::SecurityInterstitialPage>)>
        blocking_page_ready_callback,
    int options_mask) {
  bool overridable = SSLBlockingPage::IsOverridable(options_mask);

  security_interstitials::MetricsHelper::ReportDetails report_details;
  report_details.metric_prefix =
      overridable ? "ssl_overridable" : "ssl_nonoverridable";
  auto metrics_helper = std::make_unique<security_interstitials::MetricsHelper>(
      request_url, report_details, /*history_service=*/nullptr);

  auto controller_client = std::make_unique<SSLErrorControllerClient>(
      web_contents, cert_error, ssl_info, request_url,
      std::move(metrics_helper));

  auto* interstitial_page = new SSLBlockingPage(
      web_contents, cert_error, ssl_info, request_url, options_mask,
      base::Time::NowFromSystemTime(), /*support_url=*/GURL(),
      std::move(ssl_cert_reporter), overridable, std::move(controller_client));

  // Note: |blocking_page_ready_callback| must be posted due to
  // HandleSSLError()'s guarantee that it will not invoke this callback
  // synchronously.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(blocking_page_ready_callback),
                                base::WrapUnique(interstitial_page)));
}

}  // namespace

void HandleSSLError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    const base::Callback<void(content::CertificateRequestResultType)>&
        decision_callback,
    base::OnceCallback<
        void(std::unique_ptr<security_interstitials::SecurityInterstitialPage>)>
        blocking_page_ready_callback) {
  // NOTE: In Chrome hard overrides can be disabled for the Profile by setting
  // the kSSLErrorOverrideAllowed preference (which defaults to true) to false.
  // However, in WebLayer there is currently no way for the user to set this
  // preference.
  bool hard_override_disabled = false;
  int options_mask = security_interstitials::CalculateSSLErrorOptionsMask(
      cert_error, hard_override_disabled, ssl_info.is_fatal_cert_error);

  // Handle all errors by showing SSL interstitials. If this needs to get more
  // refined in the short-term, can adapt logic from
  // SSLErrorHandler::StartHandlingError() as needed (in the long-term, WebLayer
  // will most likely share a componentized version of //chrome's
  // SSLErrorHandler).
  ShowSSLInterstitial(web_contents, cert_error, ssl_info, request_url,
                      std::move(ssl_cert_reporter),
                      std::move(decision_callback),
                      std::move(blocking_page_ready_callback), options_mask);
}

}  // namespace weblayer
