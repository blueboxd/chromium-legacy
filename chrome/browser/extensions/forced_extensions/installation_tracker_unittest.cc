// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/forced_extensions/installation_tracker.h"

#include "base/memory/ptr_util.h"
#include "base/scoped_observer.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/timer/mock_timer.h"
#include "base/values.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/forced_extensions/installation_metrics.h"
#include "chrome/browser/extensions/forced_extensions/installation_reporter.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "extensions/common/value_builder.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/arc/arc_prefs.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#endif  // defined(OS_CHROMEOS)

namespace {

// The extension ids used here should be valid extension ids.
constexpr char kExtensionId1[] = "abcdefghijklmnopabcdefghijklmnop";
constexpr char kExtensionId2[] = "bcdefghijklmnopabcdefghijklmnopa";
constexpr char kExtensionId3[] = "cdefghijklmnopqrstuvwxyzabcdefgh";
constexpr char kExtensionName1[] = "name1";
constexpr char kExtensionName2[] = "name2";
constexpr char kExtensionUpdateUrl[] =
    "https://clients2.google.com/service/update2/crx";  // URL of Chrome Web
                                                        // Store backend.

const int kFetchTries = 5;
// HTTP_UNAUTHORIZED
const int kResponseCode = 401;

constexpr char kLoadTimeStats[] = "Extensions.ForceInstalledLoadTime";
constexpr char kTimedOutStats[] = "Extensions.ForceInstalledTimedOutCount";
constexpr char kTimedOutNotInstalledStats[] =
    "Extensions.ForceInstalledTimedOutAndNotInstalledCount";
constexpr char kInstallationFailureCacheStatus[] =
    "Extensions.ForceInstalledFailureCacheStatus";
constexpr char kFailureReasonsCWS[] =
    "Extensions.WebStore_ForceInstalledFailureReason3";
constexpr char kFailureReasonsSH[] =
    "Extensions.OffStore_ForceInstalledFailureReason3";
constexpr char kInstallationStages[] = "Extensions.ForceInstalledStage";
constexpr char kInstallationDownloadingStages[] =
    "Extensions.ForceInstalledDownloadingStage";
constexpr char kFailureCrxInstallErrorStats[] =
    "Extensions.ForceInstalledFailureCrxInstallError";
constexpr char kTotalCountStats[] =
    "Extensions.ForceInstalledTotalCandidateCount";
constexpr char kNetworkErrorCodeStats[] =
    "Extensions.ForceInstalledNetworkErrorCode";
constexpr char kHttpErrorCodeStats[] = "Extensions.ForceInstalledHttpErrorCode";
constexpr char kFetchRetriesStats[] = "Extensions.ForceInstalledFetchTries";
constexpr char kNetworkErrorCodeManifestFetchFailedStats[] =
    "Extensions.ForceInstalledManifestFetchFailedNetworkErrorCode";
constexpr char kHttpErrorCodeManifestFetchFailedStats[] =
    "Extensions.ForceInstalledManifestFetchFailedHttpErrorCode";
constexpr char kFetchRetriesManifestFetchFailedStats[] =
    "Extensions.ForceInstalledManifestFetchFailedFetchTries";
constexpr char kSandboxUnpackFailureReason[] =
    "Extensions.ForceInstalledFailureSandboxUnpackFailureReason";
#if defined(OS_CHROMEOS)
constexpr char kFailureSessionStats[] =
    "Extensions.ForceInstalledFailureSessionType";
#endif  // defined(OS_CHROMEOS)
constexpr char kPossibleNonMisconfigurationFailures[] =
    "Extensions.ForceInstalledSessionsWithNonMisconfigurationFailureOccured";
constexpr char kManifestUpdateCheckStatus[] =
    "Extensions.ForceInstalledFailureUpdateCheckStatus";
constexpr char kDisableReason[] =
    "Extensions.ForceInstalledNotLoadedDisableReason";
constexpr char kBlacklisted[] = "Extensions.ForceInstalledAndBlackListed";
}  // namespace

namespace extensions {

using testing::_;
using testing::Return;

class ForcedExtensionsInstallationTrackerTest
    : public testing::Test,
      public InstallationTracker::Observer {
 public:
  ForcedExtensionsInstallationTrackerTest() = default;

  void SetUp() override {
    EXPECT_CALL(policy_provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(false));

    auto policy_service = std::make_unique<policy::PolicyServiceImpl>(
        std::vector<policy::ConfigurationPolicyProvider*>{&policy_provider_});
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile(
        "p1", nullptr, base::UTF8ToUTF16("p1"), 0, "",
        TestingProfile::TestingFactories(), base::nullopt,
        std::move(policy_service));

    prefs_ = profile_->GetTestingPrefService();
    registry_ = ExtensionRegistry::Get(profile_);
    installation_reporter_ = InstallationReporter::Get(profile_);
    auto fake_timer = std::make_unique<base::MockOneShotTimer>();
    fake_timer_ = fake_timer.get();
    tracker_ = std::make_unique<InstallationTracker>(registry_, profile_);
    scoped_observer_.Add(tracker_.get());
    metrics_ = std::make_unique<InstallationMetrics>(
        registry_, profile_, tracker_.get(), std::move(fake_timer));
  }

  void SetupForceList() {
    auto list = std::make_unique<base::Value>(base::Value::Type::LIST);
    list->Append(base::StrCat({kExtensionId1, ";", kExtensionUpdateUrl}));
    list->Append(base::StrCat({kExtensionId2, ";", kExtensionUpdateUrl}));
    std::unique_ptr<base::Value> dict =
        DictionaryBuilder()
            .Set(kExtensionId1,
                 DictionaryBuilder()
                     .Set(ExternalProviderImpl::kExternalUpdateUrl,
                          kExtensionUpdateUrl)
                     .Build())
            .Set(kExtensionId2,
                 DictionaryBuilder()
                     .Set(ExternalProviderImpl::kExternalUpdateUrl,
                          kExtensionUpdateUrl)
                     .Build())
            .Build();
    prefs_->SetManagedPref(pref_names::kInstallForceList, std::move(dict));

    EXPECT_CALL(policy_provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    policy::PolicyMap map;
    map.Set("ExtensionInstallForcelist", policy::POLICY_LEVEL_MANDATORY,
            policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_PLATFORM,
            std::move(list), nullptr);
    policy_provider_.UpdateChromePolicy(map);
    base::RunLoop().RunUntilIdle();
  }

  void SetupEmptyForceList() {
    std::unique_ptr<base::Value> dict = DictionaryBuilder().Build();
    prefs_->SetManagedPref(pref_names::kInstallForceList, std::move(dict));

    EXPECT_CALL(policy_provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    policy::PolicyMap map;
    policy_provider_.UpdateChromePolicy(std::move(map));
    base::RunLoop().RunUntilIdle();
  }

  // InstallationTracker::Observer overrides:
  void OnForceInstalledExtensionsLoaded() override { loaded_call_count_++; }
  void OnForceInstalledExtensionsReady() override { ready_call_count_++; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  policy::MockConfigurationPolicyProvider policy_provider_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;
  sync_preferences::TestingPrefServiceSyncable* prefs_;
  ExtensionRegistry* registry_;
  InstallationReporter* installation_reporter_;
  base::HistogramTester histogram_tester_;

  base::MockOneShotTimer* fake_timer_;
  std::unique_ptr<InstallationTracker> tracker_;
  std::unique_ptr<InstallationMetrics> metrics_;

  ScopedObserver<InstallationTracker, InstallationTracker::Observer>
      scoped_observer_{this};
  size_t loaded_call_count_ = 0;
  size_t ready_call_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ForcedExtensionsInstallationTrackerTest);
};

TEST_F(ForcedExtensionsInstallationTrackerTest, EmptyForcelist) {
  SetupEmptyForceList();
  EXPECT_FALSE(fake_timer_->IsRunning());
  EXPECT_EQ(1u, loaded_call_count_);
  EXPECT_EQ(1u, ready_call_count_);
  // Don't report metrics when the Forcelist is empty.
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 0);
  histogram_tester_.ExpectTotalCount(kTimedOutStats, 0);
  histogram_tester_.ExpectTotalCount(kTimedOutNotInstalledStats, 0);
  histogram_tester_.ExpectTotalCount(kFailureReasonsCWS, 0);
  histogram_tester_.ExpectTotalCount(kFailureReasonsSH, 0);
  histogram_tester_.ExpectTotalCount(kInstallationStages, 0);
  histogram_tester_.ExpectTotalCount(kFailureCrxInstallErrorStats, 0);
  histogram_tester_.ExpectTotalCount(kTotalCountStats, 0);
}

TEST_F(ForcedExtensionsInstallationTrackerTest, ExtensionsInstalled) {
  SetupForceList();
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  auto ext2 = ExtensionBuilder(kExtensionName2).SetID(kExtensionId2).Build();

  EXPECT_EQ(0u, loaded_call_count_);
  EXPECT_EQ(0u, ready_call_count_);
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 0);
  tracker_->OnExtensionLoaded(profile_, ext1.get());
  EXPECT_EQ(0u, loaded_call_count_);
  EXPECT_EQ(0u, ready_call_count_);
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 0);
  tracker_->OnExtensionLoaded(profile_, ext2.get());
  EXPECT_EQ(1u, loaded_call_count_);
  EXPECT_EQ(0u, ready_call_count_);
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 1);
  histogram_tester_.ExpectTotalCount(kTimedOutStats, 0);
  histogram_tester_.ExpectTotalCount(kTimedOutNotInstalledStats, 0);
  histogram_tester_.ExpectTotalCount(kFailureReasonsCWS, 0);
  histogram_tester_.ExpectTotalCount(kFailureReasonsSH, 0);
  histogram_tester_.ExpectTotalCount(kInstallationStages, 0);
  histogram_tester_.ExpectTotalCount(kFailureCrxInstallErrorStats, 0);
  histogram_tester_.ExpectUniqueSample(
      kTotalCountStats,
      prefs_->GetManagedPref(pref_names::kInstallForceList)->DictSize(), 1);
  tracker_->OnExtensionReady(profile_, ext1.get());
  tracker_->OnExtensionReady(profile_, ext2.get());
  EXPECT_EQ(1u, loaded_call_count_);
  EXPECT_EQ(1u, ready_call_count_);
}

TEST_F(ForcedExtensionsInstallationTrackerTest, ObserversOnlyCalledOnce) {
  // Start with a non-empty force-list, and install them, which triggers
  // observer.
  SetupForceList();
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  auto ext2 = ExtensionBuilder(kExtensionName2).SetID(kExtensionId2).Build();
  tracker_->OnExtensionLoaded(profile_, ext1.get());
  tracker_->OnExtensionLoaded(profile_, ext2.get());
  EXPECT_EQ(1u, loaded_call_count_);
  tracker_->OnExtensionReady(profile_, ext1.get());
  tracker_->OnExtensionReady(profile_, ext2.get());
  EXPECT_EQ(1u, ready_call_count_);

  // Then apply a new set of policies, which shouldn't trigger observers again.
  SetupEmptyForceList();
  EXPECT_EQ(1u, loaded_call_count_);
  EXPECT_EQ(1u, ready_call_count_);
}

TEST_F(ForcedExtensionsInstallationTrackerTest,
       ExtensionsInstallationTimedOut) {
  SetupForceList();
  EXPECT_EQ(0u, loaded_call_count_);
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  registry_->AddEnabled(ext1.get());
  EXPECT_TRUE(fake_timer_->IsRunning());
  EXPECT_EQ(0u, loaded_call_count_);
  fake_timer_->Fire();
  // Metrics are reported due to timeout, but InstallationTracker::Observer
  // never fired.
  EXPECT_EQ(0u, loaded_call_count_);
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 0);
  histogram_tester_.ExpectUniqueSample(kTimedOutStats, 2, 1);
  histogram_tester_.ExpectUniqueSample(kTimedOutNotInstalledStats, 1, 1);
  histogram_tester_.ExpectTotalCount(kFailureReasonsCWS, 1);
  histogram_tester_.ExpectUniqueSample(
      kFailureReasonsCWS, InstallationReporter::FailureReason::UNKNOWN, 1);
  histogram_tester_.ExpectTotalCount(kInstallationStages, 0);
  histogram_tester_.ExpectTotalCount(kFailureCrxInstallErrorStats, 0);
  histogram_tester_.ExpectUniqueSample(
      kTotalCountStats,
      prefs_->GetManagedPref(pref_names::kInstallForceList)->DictSize(), 1);
}

// Reporting disable reason for the force installed extensions which are
// installed but not loaded when extension is disable due to single reason.
TEST_F(ForcedExtensionsInstallationTrackerTest,
       ExtensionsInstalledButNotLoadedUniqueDisableReason) {
  SetupForceList();
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  registry_->AddDisabled(ext1.get());
  ExtensionPrefs::Get(profile_)->AddDisableReason(
      kExtensionId1, disable_reason::DisableReason::DISABLE_NOT_VERIFIED);
  auto ext2 = ExtensionBuilder(kExtensionName2).SetID(kExtensionId2).Build();
  registry_->AddEnabled(ext2.get());
  tracker_->OnExtensionLoaded(profile_, ext2.get());
  // InstallationTracker should still keep running as kExtensionId1 is installed
  // but not loaded.
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  histogram_tester_.ExpectUniqueSample(
      kDisableReason, disable_reason::DisableReason::DISABLE_NOT_VERIFIED, 1);
}

// Reporting disable reasons for the force installed extensions which are
// installed but not loaded when extension is disable due to multiple reasons.
TEST_F(ForcedExtensionsInstallationTrackerTest,
       ExtensionsInstalledButNotLoadedMultipleDisableReason) {
  SetupForceList();
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  registry_->AddDisabled(ext1.get());
  ExtensionPrefs::Get(profile_)->AddDisableReasons(
      kExtensionId1,
      disable_reason::DisableReason::DISABLE_NOT_VERIFIED |
          disable_reason::DisableReason::DISABLE_UNSUPPORTED_REQUIREMENT);
  auto ext2 = ExtensionBuilder(kExtensionName2).SetID(kExtensionId2).Build();
  registry_->AddEnabled(ext2.get());
  tracker_->OnExtensionLoaded(profile_, ext2.get());
  // InstallationTracker should still keep running as kExtensionId1 is installed
  // but not loaded.
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  // Verifies that only one disable reason is reported;
  histogram_tester_.ExpectUniqueSample(
      kDisableReason,
      disable_reason::DisableReason::DISABLE_UNSUPPORTED_REQUIREMENT, 1);
}

// Reporting DisableReason::DISABLE_NONE for the force installed extensions
// which are installed but not loaded when extension is enabled.
TEST_F(ForcedExtensionsInstallationTrackerTest,
       ExtensionsInstalledButNotLoadedNoDisableReason) {
  SetupForceList();
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  registry_->AddEnabled(ext1.get());
  auto ext2 = ExtensionBuilder(kExtensionName2).SetID(kExtensionId2).Build();
  registry_->AddEnabled(ext2.get());
  tracker_->OnExtensionLoaded(profile_, ext2.get());
  // InstallationTracker should still keep running as kExtensionId1 is installed
  // but not loaded.
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  histogram_tester_.ExpectUniqueSample(
      kDisableReason, disable_reason::DisableReason::DISABLE_NONE, 1);
}

TEST_F(ForcedExtensionsInstallationTrackerTest,
       ExtensionForceInstalledAndBlacklisted) {
  SetupForceList();
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  registry_->AddBlacklisted(ext1.get());
  auto ext2 = ExtensionBuilder(kExtensionName2).SetID(kExtensionId2).Build();
  registry_->AddEnabled(ext2.get());
  tracker_->OnExtensionLoaded(profile_, ext2.get());
  // InstallationTracker should still keep running as kExtensionId1 is installed
  // but not loaded.
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  histogram_tester_.ExpectUniqueSample(kBlacklisted, 1, 1);
}

TEST_F(ForcedExtensionsInstallationTrackerTest,
       ExtensionsInstallationCancelled) {
  SetupForceList();
  SetupEmptyForceList();
  // InstallationTracker does not shut down the timer, because it's still
  // waiting for the initial extensions to install.
  EXPECT_TRUE(fake_timer_->IsRunning());
  EXPECT_EQ(0u, loaded_call_count_);
  EXPECT_EQ(0u, ready_call_count_);
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 0);
  histogram_tester_.ExpectTotalCount(kTimedOutStats, 0);
  histogram_tester_.ExpectTotalCount(kTimedOutNotInstalledStats, 0);
  histogram_tester_.ExpectTotalCount(kFailureReasonsCWS, 0);
  histogram_tester_.ExpectTotalCount(kInstallationStages, 0);
  histogram_tester_.ExpectTotalCount(kFailureCrxInstallErrorStats, 0);
  histogram_tester_.ExpectTotalCount(kTotalCountStats, 0);
}

TEST_F(ForcedExtensionsInstallationTrackerTest,
       ForcedExtensionsAddedAfterManualExtensions) {
  // Report failure for an extension which is not in forced list.
  installation_reporter_->ReportFailure(
      kExtensionId3, InstallationReporter::FailureReason::INVALID_ID);
  // InstallationTracker should keep running as the forced extensions are still
  // not loaded.
  EXPECT_TRUE(fake_timer_->IsRunning());
  EXPECT_EQ(0u, loaded_call_count_);
  EXPECT_EQ(0u, ready_call_count_);
  SetupForceList();

  auto ext = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  tracker_->OnExtensionLoaded(profile_, ext.get());
  tracker_->OnExtensionReady(profile_, ext.get());
  installation_reporter_->ReportFailure(
      kExtensionId2, InstallationReporter::FailureReason::INVALID_ID);
  // InstallationTracker shuts down timer because kExtensionId1 was loaded and
  // kExtensionId2 was failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  EXPECT_EQ(1u, loaded_call_count_);
  EXPECT_EQ(1u, ready_call_count_);
  histogram_tester_.ExpectBucketCount(
      kFailureReasonsCWS, InstallationReporter::FailureReason::INVALID_ID, 1);
}

TEST_F(ForcedExtensionsInstallationTrackerTest,
       ExtensionsInstallationTimedOutDifferentReasons) {
  SetupForceList();
  installation_reporter_->ReportFailure(
      kExtensionId1, InstallationReporter::FailureReason::INVALID_ID);
  installation_reporter_->ReportCrxInstallError(
      kExtensionId2,
      InstallationReporter::FailureReason::CRX_INSTALL_ERROR_OTHER,
      CrxInstallErrorDetail::UNEXPECTED_ID);
  // InstallationTracker shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 0);
  histogram_tester_.ExpectUniqueSample(kTimedOutStats, 2, 1);
  histogram_tester_.ExpectUniqueSample(kTimedOutNotInstalledStats, 2, 1);
  histogram_tester_.ExpectTotalCount(kFailureReasonsCWS, 2);
  histogram_tester_.ExpectBucketCount(
      kFailureReasonsCWS, InstallationReporter::FailureReason::INVALID_ID, 1);
  histogram_tester_.ExpectBucketCount(
      kFailureReasonsCWS,
      InstallationReporter::FailureReason::CRX_INSTALL_ERROR_OTHER, 1);
  histogram_tester_.ExpectTotalCount(kInstallationStages, 0);
  histogram_tester_.ExpectUniqueSample(kFailureCrxInstallErrorStats,
                                       CrxInstallErrorDetail::UNEXPECTED_ID, 1);
  histogram_tester_.ExpectUniqueSample(
      kTotalCountStats,
      prefs_->GetManagedPref(pref_names::kInstallForceList)->DictSize(), 1);
}

// Reporting SandboxedUnpackerFailureReason when the force installed extension
// fails to install with error CRX_INSTALL_ERROR_SANDBOXED_UNPACKER_FAILURE.
TEST_F(ForcedExtensionsInstallationTrackerTest,
       ExtensionsCrxInstallErrorSandboxUnpackFailure) {
  SetupForceList();
  installation_reporter_->ReportSandboxedUnpackerFailureReason(
      kExtensionId1, SandboxedUnpackerFailureReason::CRX_FILE_NOT_READABLE);
  installation_reporter_->ReportSandboxedUnpackerFailureReason(
      kExtensionId2, SandboxedUnpackerFailureReason::UNZIP_FAILED);
  // InstallationTracker shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectTotalCount(kSandboxUnpackFailureReason, 2);
  histogram_tester_.ExpectBucketCount(
      kSandboxUnpackFailureReason,
      SandboxedUnpackerFailureReason::CRX_FILE_NOT_READABLE, 1);
  histogram_tester_.ExpectBucketCount(
      kSandboxUnpackFailureReason, SandboxedUnpackerFailureReason::UNZIP_FAILED,
      1);
}

// Reporting SandboxedUnpackerFailureReason when the force installed extension
// fails to install with error CRX_INSTALL_ERROR_SANDBOXED_UNPACKER_FAILURE.
TEST_F(ForcedExtensionsInstallationTrackerTest,
       ExtensionsUpdateCheckStatusReporting) {
  SetupForceList();
  installation_reporter_->ReportManifestUpdateCheckStatus(kExtensionId1, "ok");
  installation_reporter_->ReportFailure(
      kExtensionId1, InstallationReporter::FailureReason::CRX_FETCH_URL_EMPTY);
  installation_reporter_->ReportManifestUpdateCheckStatus(kExtensionId2,
                                                          "noupdate");
  installation_reporter_->ReportFailure(
      kExtensionId2, InstallationReporter::FailureReason::CRX_FETCH_URL_EMPTY);
  // InstallationTracker shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectTotalCount(kManifestUpdateCheckStatus, 2);
  histogram_tester_.ExpectBucketCount(
      kManifestUpdateCheckStatus, InstallationReporter::UpdateCheckStatus::kOk,
      1);
  histogram_tester_.ExpectBucketCount(
      kManifestUpdateCheckStatus,
      InstallationReporter::UpdateCheckStatus::kNoUpdate, 1);
}

// Regression test to check if the metrics are collected properly for the
// extensions which are already installed and loaded and then fail with error
// ALREADY_INSTALLED.
TEST_F(ForcedExtensionsInstallationTrackerTest,
       ExtensionLoadedThenFailedWithAlreadyInstalledError) {
  SetupForceList();
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  tracker_->OnExtensionLoaded(profile_, ext1.get());
  installation_reporter_->ReportFailure(
      kExtensionId1, InstallationReporter::FailureReason::ALREADY_INSTALLED);
  auto ext2 = ExtensionBuilder(kExtensionName2).SetID(kExtensionId2).Build();
  tracker_->OnExtensionLoaded(profile_, ext2.get());
  // InstallationTracker shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 1);
  histogram_tester_.ExpectTotalCount(kTimedOutStats, 0);
  histogram_tester_.ExpectTotalCount(kTimedOutNotInstalledStats, 0);
}

TEST_F(ForcedExtensionsInstallationTrackerTest, ExtensionsStuck) {
  SetupForceList();
  installation_reporter_->ReportInstallationStage(
      kExtensionId1, InstallationReporter::Stage::PENDING);
  installation_reporter_->ReportInstallationStage(
      kExtensionId2, InstallationReporter::Stage::DOWNLOADING);
  installation_reporter_->ReportDownloadingStage(
      kExtensionId2, ExtensionDownloaderDelegate::Stage::PENDING);
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 0);
  histogram_tester_.ExpectUniqueSample(kTimedOutStats, 2, 1);
  histogram_tester_.ExpectUniqueSample(kTimedOutNotInstalledStats, 2, 1);
  histogram_tester_.ExpectUniqueSample(
      kFailureReasonsCWS, InstallationReporter::FailureReason::IN_PROGRESS, 2);
  histogram_tester_.ExpectBucketCount(kInstallationStages,
                                      InstallationReporter::Stage::PENDING, 1);
  histogram_tester_.ExpectBucketCount(
      kInstallationStages, InstallationReporter::Stage::DOWNLOADING, 1);
  histogram_tester_.ExpectTotalCount(kFailureCrxInstallErrorStats, 0);
  histogram_tester_.ExpectUniqueSample(
      kTotalCountStats,
      prefs_->GetManagedPref(pref_names::kInstallForceList)->DictSize(), 1);
}

#if defined(OS_CHROMEOS)
TEST_F(ForcedExtensionsInstallationTrackerTest,
       ReportManagedGuestSessionOnExtensionFailure) {
  chromeos::FakeChromeUserManager* fake_user_manager =
      new chromeos::FakeChromeUserManager();
  user_manager::ScopedUserManager scoped_user_manager(
      base::WrapUnique(fake_user_manager));
  const AccountId account_id =
      AccountId::FromUserEmail(profile_->GetProfileUserName());
  user_manager::User* user =
      fake_user_manager->AddPublicAccountUser(account_id);
  fake_user_manager->UserLoggedIn(account_id, user->username_hash(),
                                  false /* browser_restart */,
                                  false /* is_child */);
  chromeos::ProfileHelper::Get()->SetProfileToUserMappingForTesting(user);
  SetupForceList();
  installation_reporter_->ReportFailure(
      kExtensionId1, InstallationReporter::FailureReason::INVALID_ID);
  installation_reporter_->ReportCrxInstallError(
      kExtensionId2,
      InstallationReporter::FailureReason::CRX_INSTALL_ERROR_OTHER,
      CrxInstallErrorDetail::UNEXPECTED_ID);
  // InstallationTracker shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(
      kFailureSessionStats,
      InstallationMetrics::SessionType::SESSION_TYPE_PUBLIC_ACCOUNT, 2);
}

TEST_F(ForcedExtensionsInstallationTrackerTest,
       ReportGuestSessionOnExtensionFailure) {
  chromeos::FakeChromeUserManager* fake_user_manager =
      new chromeos::FakeChromeUserManager();
  user_manager::ScopedUserManager scoped_user_manager(
      base::WrapUnique(fake_user_manager));
  user_manager::User* user = fake_user_manager->AddGuestUser();
  chromeos::ProfileHelper::Get()->SetProfileToUserMappingForTesting(user);
  SetupForceList();
  installation_reporter_->ReportFailure(
      kExtensionId1, InstallationReporter::FailureReason::INVALID_ID);
  installation_reporter_->ReportCrxInstallError(
      kExtensionId2,
      InstallationReporter::FailureReason::CRX_INSTALL_ERROR_OTHER,
      CrxInstallErrorDetail::UNEXPECTED_ID);
  // InstallationTracker shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(
      kFailureSessionStats,
      InstallationMetrics::SessionType::SESSION_TYPE_GUEST, 2);
}
#endif  // defined(OS_CHROMEOS)

TEST_F(ForcedExtensionsInstallationTrackerTest, ExtensionsAreDownloading) {
  SetupForceList();
  installation_reporter_->ReportInstallationStage(
      kExtensionId1, InstallationReporter::Stage::DOWNLOADING);
  installation_reporter_->ReportDownloadingStage(
      kExtensionId1, ExtensionDownloaderDelegate::Stage::DOWNLOADING_MANIFEST);
  installation_reporter_->ReportInstallationStage(
      kExtensionId2, InstallationReporter::Stage::DOWNLOADING);
  installation_reporter_->ReportDownloadingStage(
      kExtensionId2, ExtensionDownloaderDelegate::Stage::DOWNLOADING_CRX);
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 0);
  histogram_tester_.ExpectUniqueSample(kTimedOutStats, 2, 1);
  histogram_tester_.ExpectUniqueSample(kTimedOutNotInstalledStats, 2, 1);
  histogram_tester_.ExpectUniqueSample(
      kFailureReasonsCWS, InstallationReporter::FailureReason::IN_PROGRESS, 2);
  histogram_tester_.ExpectUniqueSample(
      kInstallationStages, InstallationReporter::Stage::DOWNLOADING, 2);
  histogram_tester_.ExpectTotalCount(kInstallationDownloadingStages, 2);
  histogram_tester_.ExpectBucketCount(
      kInstallationDownloadingStages,
      ExtensionDownloaderDelegate::Stage::DOWNLOADING_MANIFEST, 1);
  histogram_tester_.ExpectBucketCount(
      kInstallationDownloadingStages,
      ExtensionDownloaderDelegate::Stage::DOWNLOADING_CRX, 1);
  histogram_tester_.ExpectUniqueSample(
      kTotalCountStats,
      prefs_->GetManagedPref(pref_names::kInstallForceList)->DictSize(), 1);
}

// Error Codes in case of CRX_FETCH_FAILED.
TEST_F(ForcedExtensionsInstallationTrackerTest, ExtensionCrxFetchFailed) {
  SetupForceList();
  ExtensionDownloaderDelegate::FailureData data1(net::Error::OK, kResponseCode,
                                                 kFetchTries);
  ExtensionDownloaderDelegate::FailureData data2(
      -net::Error::ERR_INVALID_ARGUMENT, kFetchTries);
  installation_reporter_->ReportFetchError(
      kExtensionId1, InstallationReporter::FailureReason::CRX_FETCH_FAILED,
      data1);
  installation_reporter_->ReportFetchError(
      kExtensionId2, InstallationReporter::FailureReason::CRX_FETCH_FAILED,
      data2);
  // InstallationTracker shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(kNetworkErrorCodeStats, net::Error::OK,
                                      1);
  histogram_tester_.ExpectBucketCount(kHttpErrorCodeStats, kResponseCode, 1);
  histogram_tester_.ExpectBucketCount(kNetworkErrorCodeStats,
                                      -net::Error::ERR_INVALID_ARGUMENT, 1);
  histogram_tester_.ExpectBucketCount(kFetchRetriesStats, kFetchTries, 2);
}

// Error Codes in case of MANIFEST_FETCH_FAILED.
TEST_F(ForcedExtensionsInstallationTrackerTest, ExtensionManifestFetchFailed) {
  SetupForceList();
  ExtensionDownloaderDelegate::FailureData data1(net::Error::OK, kResponseCode,
                                                 kFetchTries);
  ExtensionDownloaderDelegate::FailureData data2(
      -net::Error::ERR_INVALID_ARGUMENT, kFetchTries);
  installation_reporter_->ReportFetchError(
      kExtensionId1, InstallationReporter::FailureReason::MANIFEST_FETCH_FAILED,
      data1);
  installation_reporter_->ReportFetchError(
      kExtensionId2, InstallationReporter::FailureReason::MANIFEST_FETCH_FAILED,
      data2);
  // InstallationTracker shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(kNetworkErrorCodeManifestFetchFailedStats,
                                      net::Error::OK, 1);
  histogram_tester_.ExpectBucketCount(kHttpErrorCodeManifestFetchFailedStats,
                                      kResponseCode, 1);
  histogram_tester_.ExpectBucketCount(kNetworkErrorCodeManifestFetchFailedStats,
                                      -net::Error::ERR_INVALID_ARGUMENT, 1);
  histogram_tester_.ExpectBucketCount(kFetchRetriesManifestFetchFailedStats,
                                      kFetchTries, 2);
}

// Session in which either all the extensions installed successfully, or all
// failures are admin-side misconfigurations. This test verifies that failure
// CRX_INSTALL_ERROR with detailed error KIOSK_MODE_ONLY is considered as
// misconfiguration.
TEST_F(ForcedExtensionsInstallationTrackerTest,
       NonMisconfigurationFailureNotPresentKioskModeOnlyError) {
  SetupForceList();
  auto extension =
      ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  tracker_->OnExtensionLoaded(profile_, extension.get());
  installation_reporter_->ReportCrxInstallError(
      kExtensionId2,
      InstallationReporter::FailureReason::CRX_INSTALL_ERROR_DECLINED,
      CrxInstallErrorDetail::KIOSK_MODE_ONLY);
  // InstallationTracker shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(kPossibleNonMisconfigurationFailures, 0,
                                      1);
}

// Session in which either all the extensions installed successfully, or all
// failures are admin-side misconfigurations. This test verifies that failure
// CRX_INSTALL_ERROR with detailed error DISALLOWED_BY_POLICY and when extension
// type which is not allowed to install according to policy
// kExtensionAllowedTypes is considered as misconfiguration.
TEST_F(ForcedExtensionsInstallationTrackerTest,
       NonMisconfigurationFailureNotPresentDisallowedByPolicyTypeError) {
  SetupForceList();
  // Set TYPE_EXTENSION and TYPE_THEME as the allowed extension types.
  std::unique_ptr<base::Value> list =
      ListBuilder().Append("extension").Append("theme").Build();
  prefs_->SetManagedPref(pref_names::kAllowedTypes, std::move(list));

  auto extension =
      ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  tracker_->OnExtensionLoaded(profile_, extension.get());
  // Hosted app is not a valid extension type, so this should report an error.
  installation_reporter_->ReportExtensionTypeForPolicyDisallowedExtension(
      kExtensionId2, Manifest::Type::TYPE_HOSTED_APP);
  installation_reporter_->ReportCrxInstallError(
      kExtensionId2,
      InstallationReporter::FailureReason::CRX_INSTALL_ERROR_DECLINED,
      CrxInstallErrorDetail::DISALLOWED_BY_POLICY);
  // InstallationTracker shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(
      kPossibleNonMisconfigurationFailures,
      0 /*Misconfiguration failure not present*/, 1 /*Count of the sample*/);
}

// Session in which at least one non misconfiguration failure occurred. One of
// the extension fails to install with DISALLOWED_BY_POLICY error but has
// extension type which is allowed by policy ExtensionAllowedTypes. This is not
// a misconfiguration failure.
TEST_F(ForcedExtensionsInstallationTrackerTest,
       NonMisconfigurationFailurePresentDisallowedByPolicyError) {
  SetupForceList();

  // Set TYPE_EXTENSION and TYPE_THEME as the allowed extension types.
  std::unique_ptr<base::Value> list =
      ListBuilder().Append("extension").Append("theme").Build();
  prefs_->SetManagedPref(pref_names::kAllowedTypes, std::move(list));

  auto extension =
      ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  tracker_->OnExtensionLoaded(profile_, extension.get());
  installation_reporter_->ReportExtensionTypeForPolicyDisallowedExtension(
      kExtensionId2, Manifest::Type::TYPE_EXTENSION);
  installation_reporter_->ReportCrxInstallError(
      kExtensionId2,
      InstallationReporter::FailureReason::CRX_INSTALL_ERROR_DECLINED,
      CrxInstallErrorDetail::DISALLOWED_BY_POLICY);

  // InstallationTracker shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(kPossibleNonMisconfigurationFailures,
                                      1 /*Misconfiguration failure present*/,
                                      1 /*Count of the sample*/);
}

// Session in which at least one non misconfiguration failure occurred.
// Misconfiguration failure includes error KIOSK_MODE_ONLY, when force installed
// extension fails to install with failure reason CRX_INSTALL_ERROR.
TEST_F(ForcedExtensionsInstallationTrackerTest,
       NonMisconfigurationFailurePresent) {
  SetupForceList();
  installation_reporter_->ReportFailure(
      kExtensionId1, InstallationReporter::FailureReason::INVALID_ID);
  installation_reporter_->ReportCrxInstallError(
      kExtensionId2,
      InstallationReporter::FailureReason::CRX_INSTALL_ERROR_DECLINED,
      CrxInstallErrorDetail::KIOSK_MODE_ONLY);
  // InstallationTracker shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(kPossibleNonMisconfigurationFailures, 1,
                                      1);
}

#if defined(OS_CHROMEOS)
// Session in which either all the extensions installed successfully, or all
// failures are admin-side misconfigurations. This test verifies that failure
// REPLACED_BY_ARC_APP is not considered as misconfiguration when ARC++ is
// enabled for the profile.
TEST_F(ForcedExtensionsInstallationTrackerTest,
       NonMisconfigurationFailureNotPresentReplacedByArcAppErrorArcEnabled) {
  // Enable ARC++ for this profile.
  prefs_->SetManagedPref(arc::prefs::kArcEnabled,
                         std::make_unique<base::Value>(true));
  SetupForceList();
  auto extension =
      ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  tracker_->OnExtensionLoaded(profile_, extension.get());
  installation_reporter_->ReportFailure(
      kExtensionId2, InstallationReporter::FailureReason::REPLACED_BY_ARC_APP);
  // InstallationTracker shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(kPossibleNonMisconfigurationFailures, 0,
                                      1);
}

// Session in which at least one non misconfiguration failure occurred. This
// test verifies that failure REPLACED_BY_ARC_APP is not considered as
// misconfiguration when ARC++ is disabled for the profile.
TEST_F(ForcedExtensionsInstallationTrackerTest,
       NonMisconfigurationFailureNotPresentReplacedByArcAppErrorArcDisabled) {
  // Enable ARC++ for this profile.
  prefs_->SetManagedPref(arc::prefs::kArcEnabled,
                         std::make_unique<base::Value>(false));
  SetupForceList();
  auto extension =
      ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  tracker_->OnExtensionLoaded(profile_, extension.get());
  installation_reporter_->ReportFailure(
      kExtensionId2, InstallationReporter::FailureReason::REPLACED_BY_ARC_APP);
  // InstallationTracker shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(kPossibleNonMisconfigurationFailures, 1,
                                      1);
}
#endif  // defined(OS_CHROMEOS)

// Session in which either all the extensions installed successfully, or all
// failures are admin-side misconfigurations. This test verifies that failure
// NOT_PERFORMING_NEW_INSTALL is considered as misconfiguration.
TEST_F(ForcedExtensionsInstallationTrackerTest,
       NonMisconfigurationFailureNotPresentNotPerformingNewInstallError) {
  SetupForceList();
  auto extension =
      ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  tracker_->OnExtensionLoaded(profile_, extension.get());
  installation_reporter_->ReportFailure(
      kExtensionId2,
      InstallationReporter::FailureReason::NOT_PERFORMING_NEW_INSTALL);
  // InstallationTracker shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(kPossibleNonMisconfigurationFailures, 0,
                                      1);
}

TEST_F(ForcedExtensionsInstallationTrackerTest, NoExtensionsConfigured) {
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 0);
  histogram_tester_.ExpectTotalCount(kTimedOutStats, 0);
  histogram_tester_.ExpectTotalCount(kTimedOutNotInstalledStats, 0);
  histogram_tester_.ExpectTotalCount(kFailureReasonsCWS, 0);
  histogram_tester_.ExpectTotalCount(kInstallationStages, 0);
  histogram_tester_.ExpectTotalCount(kFailureCrxInstallErrorStats, 0);
  histogram_tester_.ExpectTotalCount(kTotalCountStats, 0);
}

TEST_F(ForcedExtensionsInstallationTrackerTest, CachedExtensions) {
  SetupForceList();
  installation_reporter_->ReportDownloadingCacheStatus(
      kExtensionId1, ExtensionDownloaderDelegate::CacheStatus::CACHE_HIT);
  installation_reporter_->ReportDownloadingCacheStatus(
      kExtensionId2, ExtensionDownloaderDelegate::CacheStatus::CACHE_MISS);
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  registry_->AddEnabled(ext1.get());
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  // If an extension was installed successfully, don't mention it in statistics.
  histogram_tester_.ExpectUniqueSample(
      kInstallationFailureCacheStatus,
      ExtensionDownloaderDelegate::CacheStatus::CACHE_MISS, 1);
}

}  // namespace extensions
