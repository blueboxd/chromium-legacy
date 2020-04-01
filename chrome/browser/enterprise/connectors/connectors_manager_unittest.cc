// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/connectors_manager.h"

#include "base/optional.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

constexpr char kTestUrlMatchingPattern[] = "google.com";

constexpr char kTestUrlNotMatchingPattern[] = "chromium.org";

constexpr AnalysisConnector kAllAnalysisConnectors[] = {
    AnalysisConnector::FILE_DOWNLOADED, AnalysisConnector::FILE_ATTACHED,
    AnalysisConnector::BULK_DATA_ENTRY};

constexpr safe_browsing::BlockLargeFileTransferValues
    kAllBlockLargeFilesPolicyValues[] = {
        safe_browsing::BlockLargeFileTransferValues::BLOCK_NONE,
        safe_browsing::BlockLargeFileTransferValues::BLOCK_LARGE_DOWNLOADS,
        safe_browsing::BlockLargeFileTransferValues::BLOCK_LARGE_UPLOADS,
        safe_browsing::BlockLargeFileTransferValues::
            BLOCK_LARGE_UPLOADS_AND_DOWNLOADS};

constexpr safe_browsing::BlockUnsupportedFiletypesValues
    kAllBlockUnsupportedFileTypesValues[] = {
        safe_browsing::BlockUnsupportedFiletypesValues::
            BLOCK_UNSUPPORTED_FILETYPES_NONE,
        safe_browsing::BlockUnsupportedFiletypesValues::
            BLOCK_UNSUPPORTED_FILETYPES_DOWNLOADS,
        safe_browsing::BlockUnsupportedFiletypesValues::
            BLOCK_UNSUPPORTED_FILETYPES_UPLOADS,
        safe_browsing::BlockUnsupportedFiletypesValues::
            BLOCK_UNSUPPORTED_FILETYPES_UPLOADS_AND_DOWNLOADS,
};

constexpr safe_browsing::AllowPasswordProtectedFilesValues
    kAllAllowEncryptedPolicyValues[] = {
        safe_browsing::AllowPasswordProtectedFilesValues::ALLOW_NONE,
        safe_browsing::AllowPasswordProtectedFilesValues::ALLOW_DOWNLOADS,
        safe_browsing::AllowPasswordProtectedFilesValues::ALLOW_UPLOADS,
        safe_browsing::AllowPasswordProtectedFilesValues::
            ALLOW_UPLOADS_AND_DOWNLOADS};

constexpr safe_browsing::DelayDeliveryUntilVerdictValues
    kAllDelayDeliveryUntilVerdictValues[] = {
        safe_browsing::DelayDeliveryUntilVerdictValues::DELAY_NONE,
        safe_browsing::DelayDeliveryUntilVerdictValues::DELAY_DOWNLOADS,
        safe_browsing::DelayDeliveryUntilVerdictValues::DELAY_UPLOADS,
        safe_browsing::DelayDeliveryUntilVerdictValues::
            DELAY_UPLOADS_AND_DOWNLOADS,
};

}  // namespace

// Tests that permutations of legacy policies produce expected settings from a
// ConnectorsManager instance. T is a type used to iterate over policies with a
// {NONE, DOWNLOADS, UPLOADS, UPLOADS_AND_DOWNLOADS} pattern without testing
// every single permutation since these settings are independent.
template <typename T>
class ConnectorsManagerLegacyPoliciesTest
    : public testing::TestWithParam<std::tuple<AnalysisConnector, T>> {
 public:
  ConnectorsManagerLegacyPoliciesTest<T>()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    scoped_feature_list_.InitWithFeatures(
        {safe_browsing::kContentComplianceEnabled,
         safe_browsing::kMalwareScanEnabled},
        {});

    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

  AnalysisConnector connector() const { return std::get<0>(this->GetParam()); }
  T tested_policy() const { return std::get<1>(this->GetParam()); }

  bool upload_scan() const {
    return connector() != AnalysisConnector::FILE_DOWNLOADED;
  }

  void ValidateSettings(const ConnectorsManager::AnalysisSettings& settings) {
    ASSERT_EQ(settings.block_until_verdict, expected_block_until_verdict_);
    ASSERT_EQ(settings.block_password_protected_files,
              expected_block_password_protected_files_);
    ASSERT_EQ(settings.block_large_files, expected_block_large_files_);
    ASSERT_EQ(settings.block_unsupported_file_types,
              expected_block_unsupported_file_types_);
    ASSERT_EQ(settings.tags, expected_tags_);
  }

  base::Optional<ConnectorsManager::AnalysisSettings> GetAnalysisSettingsSync(
      const GURL& url,
      AnalysisConnector connector) {
    // This helper only works when the result is known to be available
    // synchronously. Do not use it for async tests.
    base::Optional<ConnectorsManager::AnalysisSettings> settings(base::nullopt);
    auto callback = base::BindLambdaForTesting(
        [&settings](
            base::Optional<ConnectorsManager::AnalysisSettings> tmp_settings) {
          settings = std::move(tmp_settings);
        });
    connectors_manager_.GetAnalysisSettings(url, connector, callback);
    return settings;
  }

  void TestPolicy() {
    upload_scan() ? TestPolicyOnUpload() : TestPolicyOnDownload();
  }

  void TestPolicyOnDownload() {
    // DLP only checks uploads by default and malware only checks downloads by
    // default. Overriding the appropriate policies subsequently will change the
    // tags matching the pattern.
    auto default_settings = GetAnalysisSettingsSync(url_, connector());
    ASSERT_TRUE(default_settings.has_value());
    expected_tags_ = {"malware"};
    ValidateSettings(default_settings.value());

    // The DLP tag is still absent if the patterns don't match it.
    ListPrefUpdate(TestingBrowserProcess::GetGlobal()->local_state(),
                   prefs::kURLsToCheckComplianceOfDownloadedContent)
        ->Append(kTestUrlNotMatchingPattern);
    auto exempt_pattern_dlp_settings =
        GetAnalysisSettingsSync(url_, connector());
    ASSERT_TRUE(exempt_pattern_dlp_settings.has_value());
    ValidateSettings(exempt_pattern_dlp_settings.value());

    // The DLP tag is added once the patterns do match it.
    ListPrefUpdate(TestingBrowserProcess::GetGlobal()->local_state(),
                   prefs::kURLsToCheckComplianceOfDownloadedContent)
        ->Append(kTestUrlMatchingPattern);
    auto scan_pattern_dlp_settings = GetAnalysisSettingsSync(url_, connector());
    ASSERT_TRUE(scan_pattern_dlp_settings.has_value());
    expected_tags_ = {"dlp", "malware"};
    ValidateSettings(scan_pattern_dlp_settings.value());

    // The malware tag is removed once exempt patterns match it.
    ListPrefUpdate(TestingBrowserProcess::GetGlobal()->local_state(),
                   prefs::kURLsToNotCheckForMalwareOfDownloadedContent)
        ->Append(kTestUrlMatchingPattern);
    auto exempt_pattern_malware_settings =
        GetAnalysisSettingsSync(url_, connector());
    ASSERT_TRUE(exempt_pattern_malware_settings.has_value());
    expected_tags_ = {"dlp"};
    ValidateSettings(exempt_pattern_malware_settings.value());

    // Both tags are removed once the patterns don't match them, resulting in no
    // settings.
    ListPrefUpdate(TestingBrowserProcess::GetGlobal()->local_state(),
                   prefs::kURLsToCheckComplianceOfDownloadedContent)
        ->Remove(1, nullptr);
    auto no_settings = GetAnalysisSettingsSync(url_, connector());
    ASSERT_FALSE(no_settings.has_value());
  }

  void TestPolicyOnUpload() {
    // DLP only checks uploads by default and malware only checks downloads by
    // default. Overriding the appropriate policies subsequently will change the
    // tags matching the pattern.
    auto default_settings = GetAnalysisSettingsSync(url_, connector());
    ASSERT_TRUE(default_settings.has_value());
    expected_tags_ = {"dlp"};
    ValidateSettings(default_settings.value());

    // The malware tag is still absent if the patterns don't match it.
    ListPrefUpdate(TestingBrowserProcess::GetGlobal()->local_state(),
                   prefs::kURLsToCheckForMalwareOfUploadedContent)
        ->Append(kTestUrlNotMatchingPattern);
    auto exempt_pattern_malware_settings =
        GetAnalysisSettingsSync(url_, connector());
    ASSERT_TRUE(exempt_pattern_malware_settings.has_value());
    ValidateSettings(exempt_pattern_malware_settings.value());

    // The malware tag is added once the patterns do match it.
    ListPrefUpdate(TestingBrowserProcess::GetGlobal()->local_state(),
                   prefs::kURLsToCheckForMalwareOfUploadedContent)
        ->Append(kTestUrlMatchingPattern);
    auto scan_pattern_malware_settings =
        GetAnalysisSettingsSync(url_, connector());
    ASSERT_TRUE(scan_pattern_malware_settings.has_value());
    expected_tags_ = {"dlp", "malware"};
    ValidateSettings(scan_pattern_malware_settings.value());

    // The DLP tag is removed once exempt patterns match it.
    ListPrefUpdate(TestingBrowserProcess::GetGlobal()->local_state(),
                   prefs::kURLsToNotCheckComplianceOfUploadedContent)
        ->Append(kTestUrlMatchingPattern);
    auto exempt_pattern_dlp_settings =
        GetAnalysisSettingsSync(url_, connector());
    ASSERT_TRUE(exempt_pattern_dlp_settings.has_value());
    expected_tags_ = {"malware"};
    ValidateSettings(exempt_pattern_dlp_settings.value());

    // Both tags are removed once the patterns don't match them, resulting in no
    // settings.
    ListPrefUpdate(TestingBrowserProcess::GetGlobal()->local_state(),
                   prefs::kURLsToCheckForMalwareOfUploadedContent)
        ->Remove(1, nullptr);
    auto no_settings = GetAnalysisSettingsSync(url_, connector());
    ASSERT_FALSE(no_settings.has_value());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
  ConnectorsManager connectors_manager_;
  GURL url_ = GURL("https://google.com");

  // Set to the default value of their legacy policy.
  std::set<std::string> expected_tags_ = {};
  BlockUntilVerdict expected_block_until_verdict_ = BlockUntilVerdict::NO_BLOCK;
  bool expected_block_password_protected_files_ = false;
  bool expected_block_large_files_ = false;
  bool expected_block_unsupported_file_types_ = false;
};

class ConnectorsManagerBlockLargeFileTest
    : public ConnectorsManagerLegacyPoliciesTest<
          safe_browsing::BlockLargeFileTransferValues> {
 public:
  ConnectorsManagerBlockLargeFileTest() {
    TestingBrowserProcess::GetGlobal()->local_state()->SetInteger(
        prefs::kBlockLargeFileTransfer, tested_policy());
    expected_block_large_files_ = [this]() {
      if (tested_policy() == safe_browsing::BLOCK_LARGE_UPLOADS_AND_DOWNLOADS)
        return true;
      if (tested_policy() == safe_browsing::BLOCK_NONE)
        return false;
      return upload_scan()
                 ? tested_policy() == safe_browsing::BLOCK_LARGE_UPLOADS
                 : tested_policy() == safe_browsing::BLOCK_LARGE_DOWNLOADS;
    }();
  }
};

TEST_P(ConnectorsManagerBlockLargeFileTest, Test) {
  TestPolicy();
}

INSTANTIATE_TEST_SUITE_P(
    ConnectorsManagerBlockLargeFileTest,
    ConnectorsManagerBlockLargeFileTest,
    testing::Combine(testing::ValuesIn(kAllAnalysisConnectors),
                     testing::ValuesIn(kAllBlockLargeFilesPolicyValues)));

class ConnectorsManagerBlockUnsupportedFileTypesTest
    : public ConnectorsManagerLegacyPoliciesTest<
          safe_browsing::BlockUnsupportedFiletypesValues> {
 public:
  ConnectorsManagerBlockUnsupportedFileTypesTest() {
    TestingBrowserProcess::GetGlobal()->local_state()->SetInteger(
        prefs::kBlockUnsupportedFiletypes, tested_policy());
    expected_block_unsupported_file_types_ = [this]() {
      if (tested_policy() ==
          safe_browsing::BLOCK_UNSUPPORTED_FILETYPES_UPLOADS_AND_DOWNLOADS)
        return true;
      if (tested_policy() == safe_browsing::BLOCK_UNSUPPORTED_FILETYPES_NONE)
        return false;
      return upload_scan()
                 ? tested_policy() ==
                       safe_browsing::BLOCK_UNSUPPORTED_FILETYPES_UPLOADS
                 : tested_policy() ==
                       safe_browsing::BLOCK_UNSUPPORTED_FILETYPES_DOWNLOADS;
    }();
  }
};

TEST_P(ConnectorsManagerBlockUnsupportedFileTypesTest, Test) {
  TestPolicy();
}

INSTANTIATE_TEST_SUITE_P(
    ConnectorsManagerBlockUnsupportedFileTypesTest,
    ConnectorsManagerBlockUnsupportedFileTypesTest,
    testing::Combine(testing::ValuesIn(kAllAnalysisConnectors),
                     testing::ValuesIn(kAllBlockUnsupportedFileTypesValues)));

class ConnectorsManagerAllowPasswordProtectedFilesTest
    : public ConnectorsManagerLegacyPoliciesTest<
          safe_browsing::AllowPasswordProtectedFilesValues> {
 public:
  ConnectorsManagerAllowPasswordProtectedFilesTest() {
    TestingBrowserProcess::GetGlobal()->local_state()->SetInteger(
        prefs::kAllowPasswordProtectedFiles, tested_policy());
    expected_block_password_protected_files_ = [this]() {
      if (tested_policy() == safe_browsing::ALLOW_UPLOADS_AND_DOWNLOADS)
        return false;
      if (tested_policy() == safe_browsing::ALLOW_NONE)
        return true;
      return upload_scan() ? tested_policy() != safe_browsing::ALLOW_UPLOADS
                           : tested_policy() != safe_browsing::ALLOW_DOWNLOADS;
    }();
  }
};

TEST_P(ConnectorsManagerAllowPasswordProtectedFilesTest, Test) {
  TestPolicy();
}

INSTANTIATE_TEST_SUITE_P(
    ConnectorsManagerAllowPasswordProtectedFilesTest,
    ConnectorsManagerAllowPasswordProtectedFilesTest,
    testing::Combine(testing::ValuesIn(kAllAnalysisConnectors),
                     testing::ValuesIn(kAllAllowEncryptedPolicyValues)));

class ConnectorsManagerDelayDeliveryUntilVerdictTest
    : public ConnectorsManagerLegacyPoliciesTest<
          safe_browsing::DelayDeliveryUntilVerdictValues> {
 public:
  ConnectorsManagerDelayDeliveryUntilVerdictTest() {
    TestingBrowserProcess::GetGlobal()->local_state()->SetInteger(
        prefs::kDelayDeliveryUntilVerdict, tested_policy());
    expected_block_until_verdict_ = [this]() {
      if (tested_policy() == safe_browsing::DELAY_UPLOADS_AND_DOWNLOADS)
        return BlockUntilVerdict::BLOCK;
      if (tested_policy() == safe_browsing::DELAY_NONE)
        return BlockUntilVerdict::NO_BLOCK;
      bool delay =
          (upload_scan() && tested_policy() == safe_browsing::DELAY_UPLOADS) ||
          (!upload_scan() && tested_policy() == safe_browsing::DELAY_DOWNLOADS);
      return delay ? BlockUntilVerdict::BLOCK : BlockUntilVerdict::NO_BLOCK;
    }();
  }
};

TEST_P(ConnectorsManagerDelayDeliveryUntilVerdictTest, Test) {
  TestPolicy();
}

INSTANTIATE_TEST_SUITE_P(
    ConnectorsManagerDelayDeliveryUntilVerdictTest,
    ConnectorsManagerDelayDeliveryUntilVerdictTest,
    testing::Combine(testing::ValuesIn(kAllAnalysisConnectors),
                     testing::ValuesIn(kAllDelayDeliveryUntilVerdictValues)));

}  // namespace enterprise_connectors
