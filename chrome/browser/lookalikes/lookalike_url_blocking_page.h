// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_BLOCKING_PAGE_H_
#define CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_BLOCKING_PAGE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class GURL;

// This class is responsible for showing/hiding the interstitial page that
// occurs when a new domain is visited and it looks suspiciously like another
// more popular domain.
class LookalikeUrlBlockingPage
    : public security_interstitials::SecurityInterstitialPage {
 public:
  // Interstitial type, used in tests.
  static const security_interstitials::SecurityInterstitialPage::TypeID
      kTypeForTesting;

  LookalikeUrlBlockingPage(
      content::WebContents* web_contents,
      const GURL& request_url,
      ukm::SourceId source_id,
      LookalikeUrlMatchType match_type,
      std::unique_ptr<
          security_interstitials::SecurityInterstitialControllerClient>
          controller);

  ~LookalikeUrlBlockingPage() override;

  // SecurityInterstitialPage method:
  security_interstitials::SecurityInterstitialPage::TypeID GetTypeForTesting()
      override;

  // Allow easier reporting of UKM when no interstitial is shown.
  static void RecordUkmEvent(ukm::SourceId source_id,
                             LookalikeUrlMatchType match_type,
                             LookalikeUrlBlockingPageUserAction user_action);

 protected:
  // SecurityInterstitialPage implementation:
  void CommandReceived(const std::string& command) override;
  bool ShouldCreateNewNavigation() const override;
  void PopulateInterstitialStrings(
      base::DictionaryValue* load_time_data) override;
  void OnInterstitialClosing() override;
  bool ShouldDisplayURL() const override;
  int GetHTMLTemplateId() override;

 private:
  friend class LookalikeUrlNavigationThrottleBrowserTest;

  // Values added to get our shared interstitial HTML to play nice.
  void PopulateStringsForSharedHTML(base::DictionaryValue* load_time_data);

  // Record UKM iff we haven't already reported for this page.
  void ReportUkmIfNeeded(LookalikeUrlBlockingPageUserAction action);

  ukm::SourceId source_id_;
  LookalikeUrlMatchType match_type_;

  DISALLOW_COPY_AND_ASSIGN(LookalikeUrlBlockingPage);
};

#endif  // CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_BLOCKING_PAGE_H_
