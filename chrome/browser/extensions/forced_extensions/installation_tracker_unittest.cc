// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/forced_extensions/installation_tracker.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/timer/mock_timer.h"
#include "base/values.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/forced_extensions/installation_reporter.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#endif  // defined(OS_CHROMEOS)

namespace {
constexpr char kExtensionId1[] = "id1";
constexpr char kExtensionId2[] = "id2";
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
    "Extensions.WebStore_ForceInstalledFailureReason2";
constexpr char kFailureReasonsSH[] =
    "Extensions.OffStore_ForceInstalledFailureReason2";
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
}  // namespace

namespace extensions {

class ForcedExtensionsInstallationTrackerTest : public testing::Test {
 public:
  ForcedExtensionsInstallationTrackerTest()
      : prefs_(profile_.GetTestingPrefService()),
        registry_(ExtensionRegistry::Get(&profile_)),
        installation_reporter_(InstallationReporter::Get(&profile_)) {
    auto fake_timer = std::make_unique<base::MockOneShotTimer>();
    fake_timer_ = fake_timer.get();
    tracker_ = std::make_unique<InstallationTracker>(registry_, &profile_,
                                                     std::move(fake_timer));
  }

  void SetupForceList() {
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
  }

  void SetupEmptyForceList() {
    std::unique_ptr<base::Value> dict = DictionaryBuilder().Build();
    prefs_->SetManagedPref(pref_names::kInstallForceList, std::move(dict));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  sync_preferences::TestingPrefServiceSyncable* prefs_;
  ExtensionRegistry* registry_;
  InstallationReporter* installation_reporter_;
  base::HistogramTester histogram_tester_;

  base::MockOneShotTimer* fake_timer_;
  std::unique_ptr<InstallationTracker> tracker_;

  DISALLOW_COPY_AND_ASSIGN(ForcedExtensionsInstallationTrackerTest);
};

TEST_F(ForcedExtensionsInstallationTrackerTest, ExtensionsInstalled) {
  SetupForceList();
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  auto ext2 = ExtensionBuilder(kExtensionName2).SetID(kExtensionId2).Build();

  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 0);
  tracker_->OnExtensionLoaded(&profile_, ext1.get());
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 0);
  tracker_->OnExtensionLoaded(&profile_, ext2.get());
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
}

TEST_F(ForcedExtensionsInstallationTrackerTest,
       ExtensionsInstallationTimedOut) {
  SetupForceList();
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  registry_->AddEnabled(ext1.get());
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
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

TEST_F(ForcedExtensionsInstallationTrackerTest,
       ExtensionsInstallationCancelled) {
  SetupForceList();
  SetupEmptyForceList();
  // InstallationTracker shuts down timer because there is nothing to do more.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 0);
  histogram_tester_.ExpectTotalCount(kTimedOutStats, 0);
  histogram_tester_.ExpectTotalCount(kTimedOutNotInstalledStats, 0);
  histogram_tester_.ExpectTotalCount(kFailureReasonsCWS, 0);
  histogram_tester_.ExpectTotalCount(kInstallationStages, 0);
  histogram_tester_.ExpectTotalCount(kFailureCrxInstallErrorStats, 0);
  histogram_tester_.ExpectTotalCount(kTotalCountStats, 0);
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
  user_manager::FakeUserManager* fake_user_manager =
      new user_manager::FakeUserManager();
  user_manager::ScopedUserManager scoped_user_manager(
      base::WrapUnique(fake_user_manager));
  const AccountId account_id =
      AccountId::FromUserEmail(profile_.GetProfileUserName());
  const user_manager::User* user =
      fake_user_manager->AddPublicAccountUser(account_id);
  fake_user_manager->UserLoggedIn(account_id, user->username_hash(),
                                  false /* browser_restart */,
                                  false /* is_child */);
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
      InstallationTracker::SessionType::SESSION_TYPE_PUBLIC_ACCOUNT, 2);
}

TEST_F(ForcedExtensionsInstallationTrackerTest,
       ReportGuestSessionOnExtensionFailure) {
  user_manager::FakeUserManager* fake_user_manager =
      new user_manager::FakeUserManager();
  user_manager::ScopedUserManager scoped_user_manager(
      base::WrapUnique(fake_user_manager));
  const AccountId guest_id =
      AccountId::FromUserEmail(user_manager::kGuestUserName);
  fake_user_manager->AddGuestUser(guest_id);
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
      InstallationTracker::SessionType::SESSION_TYPE_GUEST, 2);
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
// failures are admin-side misconfigurations. Misconfiguration failure includes
// error KIOSK_MODE_ONLY, when force installed extension fails to install with
// failure reason CRX_INSTALL_ERROR.
TEST_F(ForcedExtensionsInstallationTrackerTest,
       NonMisconfigurationFailureNotPresent) {
  SetupForceList();
  auto extension =
      ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  tracker_->OnExtensionLoaded(&profile_, extension.get());
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
