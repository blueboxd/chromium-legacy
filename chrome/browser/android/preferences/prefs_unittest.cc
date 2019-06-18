// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/prefs.h"

#include "base/stl_util.h"
#include "chrome/browser/android/preferences/pref_service_bridge.h"
#include "chrome/common/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class PrefsTest : public ::testing::Test {
 protected:
  const char* GetPrefName(Pref pref) {
    pref_count_++;
    return PrefServiceBridge::GetPrefNameExposedToJava(pref);
  }

  int pref_count_;
};

TEST_F(PrefsTest, TestIndex) {
  pref_count_ = 0;

  // If one of these checks fails, most likely the Pref enum and
  // |kPrefExposedToJava| are out of sync.
  EXPECT_EQ(Pref::PREF_NUM_PREFS, base::size(kPrefsExposedToJava));

  EXPECT_EQ(prefs::kAllowDeletingBrowserHistory,
            GetPrefName(ALLOW_DELETING_BROWSER_HISTORY));
  EXPECT_EQ(prefs::kIncognitoModeAvailability,
            GetPrefName(INCOGNITO_MODE_AVAILABILITY));

#if BUILDFLAG(ENABLE_FEED_IN_CHROME)
  EXPECT_EQ(feed::prefs::kEnableSnippets,
            GetPrefName(NTP_ARTICLES_SECTION_ENABLED));
  EXPECT_EQ(feed::prefs::kArticlesListVisible,
            GetPrefName(NTP_ARTICLES_LIST_VISIBLE));
#else   // BUILDFLAG(ENABLE_FEED_IN_CHROME)
  EXPECT_EQ(ntp_snippets::prefs::kEnableSnippets,
            GetPrefName(NTP_ARTICLES_SECTION_ENABLED));
  EXPECT_EQ(ntp_snippets::prefs::kArticlesListVisible,
            GetPrefName(NTP_ARTICLES_LIST_VISIBLE));
#endif  // BUILDFLAG(ENABLE_FEED_IN_CHROME)

  EXPECT_EQ(prefs::kPromptForDownloadAndroid,
            GetPrefName(PROMPT_FOR_DOWNLOAD_ANDROID));
  EXPECT_EQ(dom_distiller::prefs::kReaderForAccessibility,
            GetPrefName(READER_FOR_ACCESSIBILITY_ENABLED));
  EXPECT_EQ(prefs::kShowMissingSdCardErrorAndroid,
            GetPrefName(SHOW_MISSING_SD_CARD_ERROR_ANDROID));
  EXPECT_EQ(payments::kCanMakePaymentEnabled,
            GetPrefName(CAN_MAKE_PAYMENT_ENABLED));
  EXPECT_EQ(prefs::kContextualSearchEnabled,
            GetPrefName(CONTEXTUAL_SEARCH_ENABLED));
  EXPECT_EQ(autofill::prefs::kAutofillProfileEnabled,
            GetPrefName(AUTOFILL_PROFILE_ENABLED));
  EXPECT_EQ(autofill::prefs::kAutofillCreditCardEnabled,
            GetPrefName(AUTOFILL_CREDIT_CARD_ENABLED));
  EXPECT_EQ(prefs::kUsageStatsEnabled, GetPrefName(USAGE_STATS_ENABLED));
  EXPECT_EQ(offline_pages::prefetch_prefs::kUserSettingEnabled,
            GetPrefName(OFFLINE_PREFETCH_USER_SETTING_ENABLED));
  EXPECT_EQ(prefs::kSafeBrowsingExtendedReportingOptInAllowed,
            GetPrefName(SAFE_BROWSING_EXTENDED_REPORTING_OPT_IN_ALLOWED));

  // If this check fails, a pref is missing a test case above.
  EXPECT_EQ(Pref::PREF_NUM_PREFS, pref_count_);
}

}  // namespace
