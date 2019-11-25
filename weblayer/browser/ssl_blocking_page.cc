// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/ssl_blocking_page.h"

#include <memory>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/security_interstitials/core/ssl_error_options_mask.h"
#include "components/security_interstitials/core/ssl_error_ui.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "weblayer/browser/ssl_error_controller_client.h"

namespace weblayer {

// static
const content::InterstitialPageDelegate::TypeID
    SSLBlockingPage::kTypeForTesting = &SSLBlockingPage::kTypeForTesting;

// static
SSLBlockingPage* SSLBlockingPage::Create(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    int options_mask,
    const base::Time& time_triggered,
    const base::Callback<void(content::CertificateRequestResultType)>&
        callback) {
  bool overridable = IsOverridable(options_mask);

  security_interstitials::MetricsHelper::ReportDetails report_details;
  report_details.metric_prefix =
      overridable ? "ssl_overridable" : "ssl_nonoverridable";
  auto metrics_helper = std::make_unique<security_interstitials::MetricsHelper>(
      request_url, report_details, /*history_service=*/nullptr);

  return new SSLBlockingPage(web_contents, cert_error, ssl_info, request_url,
                             options_mask, time_triggered, overridable,
                             std::move(metrics_helper), callback);
}

bool SSLBlockingPage::ShouldCreateNewNavigation() const {
  return true;
}

content::InterstitialPageDelegate::TypeID SSLBlockingPage::GetTypeForTesting() {
  return SSLBlockingPage::kTypeForTesting;
}

SSLBlockingPage::~SSLBlockingPage() = default;

void SSLBlockingPage::PopulateInterstitialStrings(
    base::DictionaryValue* load_time_data) {
  ssl_error_ui_->PopulateStringsForHTML(load_time_data);
}

// Note that we always create a navigation entry with SSL errors.
// No error happening loading a sub-resource triggers an interstitial so far.
SSLBlockingPage::SSLBlockingPage(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    int options_mask,
    const base::Time& time_triggered,
    bool overridable,
    std::unique_ptr<security_interstitials::MetricsHelper> metrics_helper,
    const base::Callback<void(content::CertificateRequestResultType)>& callback)
    : security_interstitials::SecurityInterstitialPage(
          web_contents,
          request_url,
          std::make_unique<SSLErrorControllerClient>(
              web_contents,
              cert_error,
              ssl_info,
              request_url,
              std::move(metrics_helper))),
      ssl_info_(ssl_info),
      ssl_error_ui_(std::make_unique<security_interstitials::SSLErrorUI>(
          request_url,
          cert_error,
          ssl_info,
          options_mask,
          time_triggered,
          /*support_url=*/GURL(),
          controller())) {
  DCHECK(callback.is_null());
}

void SSLBlockingPage::OverrideEntry(content::NavigationEntry* entry) {
  entry->GetSSL() = content::SSLStatus(ssl_info_);
}

// This handles the commands sent from the interstitial JavaScript.
void SSLBlockingPage::CommandReceived(const std::string& command) {
  // content::WaitForRenderFrameReady sends this message when the page
  // load completes. Ignore it.
  if (command == "\"pageLoadComplete\"")
    return;

  int cmd = 0;
  bool retval = base::StringToInt(command, &cmd);
  DCHECK(retval);

  ssl_error_ui_->HandleCommand(
      static_cast<security_interstitials::SecurityInterstitialCommand>(cmd));
}

void SSLBlockingPage::OnInterstitialClosing() {
  // TODO(blundell): Does this need to track metrics analogously to //chrome's
  // SSLBlockingPageBase::OnInterstitialClosing()?
}

// static
bool SSLBlockingPage::IsOverridable(int options_mask) {
  const bool is_overridable =
      (options_mask &
       security_interstitials::SSLErrorOptionsMask::SOFT_OVERRIDE_ENABLED) &&
      !(options_mask &
        security_interstitials::SSLErrorOptionsMask::STRICT_ENFORCEMENT) &&
      !(options_mask &
        security_interstitials::SSLErrorOptionsMask::HARD_OVERRIDE_DISABLED);
  return is_overridable;
}

}  // namespace weblayer
