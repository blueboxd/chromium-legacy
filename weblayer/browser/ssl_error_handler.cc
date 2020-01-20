// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/ssl_error_handler.h"

#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/security_interstitials/content/bad_clock_blocking_page.h"
#include "components/security_interstitials/content/captive_portal_blocking_page.h"
#include "components/security_interstitials/content/ssl_blocking_page.h"
#include "components/security_interstitials/content/ssl_cert_reporter.h"
#include "components/security_interstitials/content/ssl_error_navigation_throttle.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/security_interstitials/core/ssl_error_options_mask.h"
#include "components/security_interstitials/core/ssl_error_ui.h"
#include "components/ssl_errors/error_info.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/ssl_error_controller_client.h"
#include "weblayer/browser/weblayer_content_browser_overlay_manifest.h"

#if defined(OS_ANDROID)
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "net/android/network_library.h"
#include "ui/base/window_open_disposition.h"
#endif

namespace weblayer {

namespace {

bool g_is_behind_captive_portal_for_testing = false;

// Returns whether the user is behind a captive portal.
bool IsBehindCaptivePortal() {
  if (g_is_behind_captive_portal_for_testing)
    return true;

#if defined(OS_ANDROID)
  return net::android::GetIsCaptivePortal();
#else
  // WebLayer does not currently integrate CaptivePortalService, which Chrome
  // uses on non-Android platforms to detect the user being behind a captive
  // portal.
  return false;
#endif
}

#if defined(OS_ANDROID)
GURL GetCaptivePortalLoginPageUrlInternal() {
  // NOTE: This is taken from the default login URL in //chrome's
  // CaptivePortalHelper.java, which is used in the implementation referenced
  // in OpenLoginPage() below.
  return GURL("http://connectivitycheck.gstatic.com/generate_204");
}
#endif

void OpenLoginPage(content::WebContents* web_contents) {
  // TODO(https://crbug.com/1030692): Componentize and share the
  // Android implementation from //chrome's
  // ChromeSecurityBlockingPageFactory::OpenLoginPage(), from which this is
  // adapted.
#if defined(OS_ANDROID)
  // NOTE: In Chrome this opens in a new tab; however, as WebLayer doesn't have
  // the ability to open new tabs it must open in the current tab.
  content::OpenURLParams params(
      GetCaptivePortalLoginPageUrlInternal(), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_LINK, false);
  web_contents->OpenURL(params);
#else
  NOTIMPLEMENTED();
#endif
}

// Constructs and shows a captive portal interstitial. Adapted from //chrome's
// SSLErrorHandlerDelegateImpl::ShowCaptivePortalInterstitial().
void ShowCaptivePortalInterstitial(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    base::OnceCallback<
        void(std::unique_ptr<security_interstitials::SecurityInterstitialPage>)>
        blocking_page_ready_callback) {
  security_interstitials::MetricsHelper::ReportDetails report_details;
  report_details.metric_prefix = "captive_portal";
  auto metrics_helper = std::make_unique<security_interstitials::MetricsHelper>(
      request_url, report_details, /*history_service=*/nullptr);

  auto controller_client = std::make_unique<SSLErrorControllerClient>(
      web_contents, cert_error, ssl_info, request_url,
      std::move(metrics_helper));

  // When captive portals are detected by the underlying platform (the only
  // context in which captive portals are currently detected in WebLayer),
  // the login URL is not specified by the client but is determined internally.
  GURL login_url;

  auto* interstitial_page = new CaptivePortalBlockingPage(
      web_contents, request_url, login_url, std::move(ssl_cert_reporter),
      ssl_info, std::move(controller_client),
      base::BindRepeating(&OpenLoginPage));

  // Note: |blocking_page_ready_callback| must be posted due to
  // HandleSSLError()'s guarantee that it will not invoke this callback
  // synchronously.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(blocking_page_ready_callback),
                                base::WrapUnique(interstitial_page)));
}

// Constructs and shows an SSL interstitial. Adapted from //chrome's
// SSLErrorHandlerDelegateImpl::ShowSSLInterstitial().
void ShowSSLInterstitial(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
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

// Constructs and shows a bad clock interstitial. Adapted from //chrome's
// SSLErrorHandlerDelegateImpl::ShowCaptivePortalInterstitial().
void ShowBadClockInterstitial(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    ssl_errors::ClockState clock_state,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    base::OnceCallback<
        void(std::unique_ptr<security_interstitials::SecurityInterstitialPage>)>
        blocking_page_ready_callback) {
  security_interstitials::MetricsHelper::ReportDetails report_details;
  report_details.metric_prefix = "bad_clock";
  auto metrics_helper = std::make_unique<security_interstitials::MetricsHelper>(
      request_url, report_details, /*history_service=*/nullptr);

  auto controller_client = std::make_unique<SSLErrorControllerClient>(
      web_contents, cert_error, ssl_info, request_url,
      std::move(metrics_helper));

  auto* interstitial_page = new BadClockBlockingPage(
      web_contents, cert_error, ssl_info, request_url,
      base::Time::NowFromSystemTime(), clock_state,
      std::move(ssl_cert_reporter), std::move(controller_client));

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
    network_time::NetworkTimeTracker* network_time_tracker,
    base::OnceCallback<
        void(std::unique_ptr<security_interstitials::SecurityInterstitialPage>)>
        blocking_page_ready_callback) {
  // Check for a clock error.
  if (ssl_errors::ErrorInfo::NetErrorToErrorType(cert_error) ==
      ssl_errors::ErrorInfo::CERT_DATE_INVALID) {
    // This implementation is adapted from //chrome's
    // SSLErrorHandler::HandleCertDateInvalidErrorImpl(). Note that we did not
    // port the fetch of NetworkTimeTracker's time made in //chrome's
    // SSLErrorHandler::HandleCertDateInvalidError() into //weblayer: this
    // fetch introduces a fair degree of complexity into the flow by making it
    // asynchronous, and it is not relevant on Android, where such fetches are
    // not supported. This fetch will be incorporated when WebLayer shares
    // //chrome's SSLErrorHandler implementation as part of crbug.com/1026547.

    const base::Time now = base::Time::NowFromSystemTime();

    ssl_errors::ClockState clock_state =
        ssl_errors::GetClockState(now, network_time_tracker);

    if (clock_state == ssl_errors::CLOCK_STATE_FUTURE ||
        clock_state == ssl_errors::CLOCK_STATE_PAST) {
      ShowBadClockInterstitial(web_contents, cert_error, ssl_info, request_url,
                               clock_state, std::move(ssl_cert_reporter),
                               std::move(blocking_page_ready_callback));
      return;
    }
  }

  // Next check for a captive portal.

  // TODO(https://crbug.com/1030692): Share the check for known captive
  // portal certificates from //chrome's SSLErrorHandler:757.
  if (IsBehindCaptivePortal()) {
    // TODO(https://crbug.com/1030692): Share the reporting of network
    // connectivity and tracking UMA from //chrome's SSLErrorHandler:743.
    ShowCaptivePortalInterstitial(web_contents, cert_error, ssl_info,
                                  request_url, std::move(ssl_cert_reporter),
                                  std::move(blocking_page_ready_callback));
    return;
  }

  // Handle all remaining errors by showing SSL interstitials. If this needs to
  // get more refined in the short-term, can adapt logic from
  // SSLErrorHandler::StartHandlingError() as needed (in the long-term,
  // WebLayer will most likely share a componentized version of //chrome's
  // SSLErrorHandler).

  // NOTE: In Chrome hard overrides can be disabled for the Profile by setting
  // the kSSLErrorOverrideAllowed preference (which defaults to true) to false.
  // However, in WebLayer there is currently no way for the user to set this
  // preference.
  bool hard_override_disabled = false;
  int options_mask = security_interstitials::CalculateSSLErrorOptionsMask(
      cert_error, hard_override_disabled, ssl_info.is_fatal_cert_error);

  ShowSSLInterstitial(web_contents, cert_error, ssl_info, request_url,
                      std::move(ssl_cert_reporter),
                      std::move(blocking_page_ready_callback), options_mask);
}

void SetDiagnoseSSLErrorsAsCaptivePortalForTesting(bool enabled) {
  g_is_behind_captive_portal_for_testing = enabled;
}

#if defined(OS_ANDROID)
GURL GetCaptivePortalLoginPageUrlForTesting() {
  return GetCaptivePortalLoginPageUrlInternal();
}
#endif

}  // namespace weblayer
