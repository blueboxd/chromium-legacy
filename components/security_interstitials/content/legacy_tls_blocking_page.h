// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_LEGACY_TLS_BLOCKING_PAGE_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_LEGACY_TLS_BLOCKING_PAGE_H_

#include "base/macros.h"
#include "components/security_interstitials/content/ssl_blocking_page_base.h"
#include "components/security_interstitials/content/ssl_cert_reporter.h"
#include "components/security_interstitials/core/legacy_tls_ui.h"
#include "net/ssl/ssl_info.h"

// This class is responsible for showing/hiding the interstitial page that is
// shown then a connection uses an obsolete TLS version (i.e., TLS 1.0 or 1.1).
// It deletes itself when the interstitial page is closed.
class LegacyTLSBlockingPage : public SSLBlockingPageBase {
 public:
  // Interstitial type, used in tests.
  static const InterstitialPageDelegate::TypeID kTypeForTesting;

  LegacyTLSBlockingPage(
      content::WebContents* web_contents,
      int cert_error,
      const GURL& request_url,
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
      const net::SSLInfo& ssl_info,
      std::unique_ptr<
          security_interstitials::SecurityInterstitialControllerClient>
          controller_client);
  ~LegacyTLSBlockingPage() override;

  // InterstitialPageDelegate method:
  InterstitialPageDelegate::TypeID GetTypeForTesting() override;

 protected:
  // InterstitialPageDelegate implementation:
  void CommandReceived(const std::string& command) override;
  void OverrideEntry(content::NavigationEntry* entry) override;

  // SecurityInterstitialPage implementation:
  bool ShouldCreateNewNavigation() const override;
  void PopulateInterstitialStrings(
      base::DictionaryValue* load_time_data) override;

 private:
  const net::SSLInfo ssl_info_;

  const std::unique_ptr<security_interstitials::LegacyTLSUI> legacy_tls_ui_;

  DISALLOW_COPY_AND_ASSIGN(LegacyTLSBlockingPage);
};

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_LEGACY_TLS_BLOCKING_PAGE_H_
