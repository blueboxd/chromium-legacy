// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"
#include "chrome/browser/metrics/testing/metrics_reporting_pref_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/secondary_account_helper.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_token_status.h"
#include "components/sync/test/fake_server/fake_server_network_resources.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/ukm_service.h"
#include "components/unified_consent/feature.h"
#include "components/unified_consent/scoped_unified_consent.h"
#include "components/unified_consent/unified_consent_service.h"
#include "components/variations/service/variations_field_trial_creator.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/network/test/test_network_quality_tracker.h"
#include "third_party/metrics_proto/ukm/report.pb.h"
#include "third_party/zlib/google/compression_utils.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/signin/scoped_account_consistency.h"
#endif

namespace metrics {

namespace {

// Clears the specified data using BrowsingDataRemover.
void ClearBrowsingData(Profile* profile) {
  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(profile);
  content::BrowsingDataRemoverCompletionObserver observer(remover);
  remover->RemoveAndReply(
      base::Time(), base::Time::Max(),
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB, &observer);
  observer.BlockUntilCompletion();
  // Make sure HistoryServiceObservers have a chance to be notified.
  content::RunAllTasksUntilIdle();
}

}  // namespace

// An observer that returns back to test code after a new profile is
// initialized.
void UnblockOnProfileCreation(base::RunLoop* run_loop,
                              Profile* profile,
                              Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    run_loop->Quit();
}

Profile* CreateGuestProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath new_path = profile_manager->GetGuestProfilePath();
  base::RunLoop run_loop;
  profile_manager->CreateProfileAsync(
      new_path, base::Bind(&UnblockOnProfileCreation, &run_loop),
      base::string16(), std::string());
  run_loop.Run();
  return profile_manager->GetProfileByPath(new_path);
}

// A helper object for overriding metrics enabled state.
class MetricsConsentOverride {
 public:
  explicit MetricsConsentOverride(bool initial_state) : state_(initial_state) {
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        &state_);
    Update(initial_state);
  }

  ~MetricsConsentOverride() {
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        nullptr);
  }

  void Update(bool state) {
    state_ = state;
    // Trigger rechecking of metrics state.
    g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(
        true);
  }

 private:
  bool state_;
};

class SyncConnectionOkChecker : public SingleClientStatusChangeChecker {
 public:
  explicit SyncConnectionOkChecker(syncer::ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  bool IsExitConditionSatisfied() override {
    return service()->GetSyncTokenStatusForDebugging().connection_status ==
           syncer::CONNECTION_OK;
  }

  std::string GetDebugMessage() const override {
    return "Waiting for CONNECTION_OK.";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncConnectionOkChecker);
};

// Test fixture that provides access to some UKM internals.
class UkmBrowserTestBase : public SyncTest {
 public:
  explicit UkmBrowserTestBase(bool is_unified_consent_enabled)
      : SyncTest(SINGLE_CLIENT),
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
        scoped_dice_(is_unified_consent_enabled
                         ? std::make_unique<ScopedAccountConsistencyDice>()
                         : nullptr),
#endif
        scoped_unified_consent_(
            is_unified_consent_enabled
                ? unified_consent::UnifiedConsentFeatureState::kEnabled
                : unified_consent::UnifiedConsentFeatureState::kDisabled) {
    // Explicitly enable UKM and disable the MetricsReporting (which should
    // not affect UKM).
    scoped_feature_list_.InitWithFeatures({ukm::kUkmFeature},
                                          {internal::kMetricsReportingFeature});
  }

  bool ukm_enabled() const {
    auto* service = ukm_service();
    return service ? service->recording_enabled_ : false;
  }
  bool ukm_extensions_enabled() const {
    auto* service = ukm_service();
    return service ? service->extensions_enabled_ : false;
  }
  uint64_t client_id() const {
    auto* service = ukm_service();
    DCHECK(service);
    // Can be non-zero only if UpdateUploadPermissions(true) has been called.
    return service->client_id_;
  }
  ukm::UkmSource* GetSource(ukm::SourceId source_id) {
    auto* service = ukm_service();
    if (!service)
      return nullptr;
    auto it = service->sources().find(source_id);
    return it == service->sources().end() ? nullptr : it->second.get();
  }
  ukm::UkmSource* NavigateAndGetSource(Browser* browser, const GURL& url) {
    content::NavigationHandleObserver observer(
        browser->tab_strip_model()->GetActiveWebContents(), url);
    ui_test_utils::NavigateToURL(browser, url);
    const ukm::SourceId source_id = ukm::ConvertToSourceId(
        observer.navigation_id(), ukm::SourceIdType::NAVIGATION_ID);
    return GetSource(source_id);
  }
  bool HasSource(ukm::SourceId source_id) const {
    auto* service = ukm_service();
    return service && base::Contains(service->sources(), source_id);
  }
  void RecordDummySource(ukm::SourceId source_id) {
    auto* service = ukm_service();
    if (service)
      service->UpdateSourceURL(source_id, GURL("http://example.com"));
  }
  void BuildAndStoreUkmLog() {
    auto* service = ukm_service();
    DCHECK(service);
    DCHECK(service->initialize_complete_);
    service->Flush();
    DCHECK(service->reporting_service_.ukm_log_store()->has_unsent_logs());
  }
  bool HasUnsentUkmLogs() {
    auto* service = ukm_service();
    DCHECK(service);
    return service->reporting_service_.ukm_log_store()->has_unsent_logs();
  }

  ukm::Report GetUkmReport() {
    EXPECT_TRUE(HasUnsentUkmLogs());

    metrics::UnsentLogStore* log_store =
        ukm_service()->reporting_service_.ukm_log_store();
    if (log_store->has_staged_log()) {
      // For testing purposes, we are examining the content of a staged log
      // without ever sending the log, so discard any previously staged log.
      log_store->DiscardStagedLog();
    }
    log_store->StageNextLog();
    EXPECT_TRUE(log_store->has_staged_log());

    std::string uncompressed_log_data;
    EXPECT_TRUE(compression::GzipUncompress(log_store->staged_log(),
                                            &uncompressed_log_data));

    ukm::Report report;
    EXPECT_TRUE(report.ParseFromString(uncompressed_log_data));
    return report;
  }

 protected:
  std::unique_ptr<ProfileSyncServiceHarness> InitializeProfileForSync(
      Profile* profile) {
    ProfileSyncServiceFactory::GetAsProfileSyncServiceForProfile(profile)
        ->OverrideNetworkResourcesForTest(
            std::make_unique<fake_server::FakeServerNetworkResources>(
                GetFakeServer()->AsWeakPtr()));

    std::string username;
#if defined(OS_CHROMEOS)
    // In browser tests, the profile may already by authenticated with stub
    // account |user_manager::kStubUserEmail|.
    CoreAccountInfo info =
        IdentityManagerFactory::GetForProfile(profile)->GetPrimaryAccountInfo();
    username = info.email;
#endif
    if (username.empty()) {
      username = "user@gmail.com";
    }

    std::unique_ptr<ProfileSyncServiceHarness> harness =
        ProfileSyncServiceHarness::Create(
            profile, username, "unused" /* password */,
            ProfileSyncServiceHarness::SigninType::FAKE_SIGNIN);
    return harness;
  }

  std::unique_ptr<ProfileSyncServiceHarness> EnableSyncForProfile(
      Profile* profile) {
    std::unique_ptr<ProfileSyncServiceHarness> harness =
        InitializeProfileForSync(profile);
    EXPECT_TRUE(harness->SetupSync());

    // If unified consent is enabled, then enable url-keyed-anonymized data
    // collection through the consent service.
    // Note: If unfied consent is not enabled, then UKM will be enabled based on
    // the history sync state.
    unified_consent::UnifiedConsentService* consent_service =
        UnifiedConsentServiceFactory::GetForProfile(profile);
    if (consent_service)
      consent_service->SetUrlKeyedAnonymizedDataCollectionEnabled(true);

    return harness;
  }

  Profile* CreateNonSyncProfile() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath new_path =
        profile_manager->GenerateNextProfileDirectoryPath();
    base::RunLoop run_loop;
    profile_manager->CreateProfileAsync(
        new_path, base::Bind(&UnblockOnProfileCreation, &run_loop),
        base::string16(), std::string());
    run_loop.Run();
    Profile* profile = profile_manager->GetProfileByPath(new_path);
    SetupMockGaiaResponsesForProfile(profile);
    return profile;
  }

 private:
  ukm::UkmService* ukm_service() const {
    return g_browser_process->GetMetricsServicesManager()->GetUkmService();
  }

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // ScopedAccountConsistencyDice is required for unified consent to be enabled.
  // Note that it uses forced field trials to enable DICE which disable metrics
  // which are required by UkmConsentParamBrowserTest (see
  // |IsMetricsReportingEnabledForOfficialBuild|).
  const std::unique_ptr<ScopedAccountConsistencyDice> scoped_dice_;
#endif
  const unified_consent::ScopedUnifiedConsent scoped_unified_consent_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(UkmBrowserTestBase);
};

class UkmBrowserTestUnifiedConsentDisabled : public UkmBrowserTestBase {
 public:
  UkmBrowserTestUnifiedConsentDisabled() : UkmBrowserTestBase(false) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(UkmBrowserTestUnifiedConsentDisabled);
};

class UkmBrowserTest : public UkmBrowserTestBase,
                       public testing::WithParamInterface<bool> {
 public:
  UkmBrowserTest() : UkmBrowserTestBase(GetParam()) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(UkmBrowserTest);
};

class UkmBrowserTestWithSyncTransport : public UkmBrowserTest {
 public:
  UkmBrowserTestWithSyncTransport() {
    features_.InitAndEnableFeature(switches::kSyncSupportSecondaryAccount);
  }

  void SetUpInProcessBrowserTestFixture() override {
    // This is required to support (fake) secondary-account-signin (based on
    // cookies) in tests. Without this, the real GaiaCookieManagerService would
    // try talking to Google servers which of course wouldn't work in tests.
    test_signin_client_factory_ =
        secondary_account_helper::SetUpSigninClient(&test_url_loader_factory_);
    UkmBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
#if defined(OS_CHROMEOS)
    secondary_account_helper::InitNetwork();
#endif  // defined(OS_CHROMEOS)
    UkmBrowserTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList features_;

  secondary_account_helper::ScopedSigninClientFactory
      test_signin_client_factory_;

  DISALLOW_COPY_AND_ASSIGN(UkmBrowserTestWithSyncTransport);
};

// This tests if UKM service is enabled/disabled appropriately based on an
// input bool param. The bool reflects if metrics reporting state is
// enabled/disabled via prefs.
class UkmConsentParamBrowserTest : public UkmBrowserTestBase,
                                   public testing::WithParamInterface<bool> {
 public:
  UkmConsentParamBrowserTest()
      : UkmBrowserTestBase(/*is_unified_consent=*/false) {}

  static bool IsMetricsAndCrashReportingEnabled() {
    return ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
  }

  // InProcessBrowserTest overrides.
  bool SetUpUserDataDirectory() override {
    local_state_path_ = SetUpUserDataDirectoryForTesting(
        is_metrics_reporting_enabled_initial_value());
    return !local_state_path_.empty();
  }

  void CreatedBrowserMainParts(content::BrowserMainParts* parts) override {
    // IsMetricsReportingEnabled() in non-official builds always returns false.
    // Enable the official build checks so that this test can work in both
    // official and non-official builds.
    ChromeMetricsServiceAccessor::SetForceIsMetricsReportingEnabledPrefLookup(
        true);
  }

  bool is_metrics_reporting_enabled_initial_value() const { return GetParam(); }

 private:
  base::FilePath local_state_path_;
  DISALLOW_COPY_AND_ASSIGN(UkmConsentParamBrowserTest);
};

class UkmEnabledChecker : public SingleClientStatusChangeChecker {
 public:
  UkmEnabledChecker(UkmBrowserTestBase* test,
                    syncer::ProfileSyncService* service,
                    bool want_enabled)
      : SingleClientStatusChangeChecker(service),
        test_(test),
        want_enabled_(want_enabled) {}

  // StatusChangeChecker:
  bool IsExitConditionSatisfied() override {
    return test_->ukm_enabled() == want_enabled_;
  }
  std::string GetDebugMessage() const override {
    return std::string("waiting for ukm_enabled=") +
           (want_enabled_ ? "true" : "false");
  }

 private:
  UkmBrowserTestBase* const test_;
  const bool want_enabled_;
  DISALLOW_COPY_AND_ASSIGN(UkmEnabledChecker);
};

// Make sure that UKM is disabled while an incognito window is open.
// Keep in sync with UkmTest.testRegularPlusIncognitoCheck in
// chrome/android/javatests/src/org/chromium/chrome/browser/metrics/
// UkmTest.java.
IN_PROC_BROWSER_TEST_P(UkmBrowserTest, RegularPlusIncognitoCheck) {
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_enabled());
  uint64_t original_client_id = client_id();
  EXPECT_NE(0U, original_client_id);

  Browser* incognito_browser = CreateIncognitoBrowser();
  EXPECT_FALSE(ukm_enabled());

  // Opening another regular browser mustn't enable UKM.
  Browser* regular_browser = CreateBrowser(profile);
  EXPECT_FALSE(ukm_enabled());

  // Opening and closing another Incognito browser mustn't enable UKM.
  CloseBrowserSynchronously(CreateIncognitoBrowser());
  EXPECT_FALSE(ukm_enabled());

  CloseBrowserSynchronously(regular_browser);
  EXPECT_FALSE(ukm_enabled());

  CloseBrowserSynchronously(incognito_browser);
  EXPECT_TRUE(ukm_enabled());
  // Client ID should not have been reset.
  EXPECT_EQ(original_client_id, client_id());

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(sync_browser);
}

// Make sure opening a real window after Incognito doesn't enable UKM.
// Keep in sync with UkmTest.testIncognitoPlusRegularCheck in
// chrome/android/javatests/src/org/chromium/chrome/browser/metrics/
// UkmTest.java.
IN_PROC_BROWSER_TEST_P(UkmBrowserTest, IncognitoPlusRegularCheck) {
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* incognito_browser = CreateIncognitoBrowser();
  EXPECT_FALSE(ukm_enabled());

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_FALSE(ukm_enabled());

  CloseBrowserSynchronously(incognito_browser);
  EXPECT_TRUE(ukm_enabled());

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(sync_browser);
}

// Make sure that UKM is disabled while a guest profile's window is open.
IN_PROC_BROWSER_TEST_P(UkmBrowserTest, RegularPlusGuestCheck) {
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* regular_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_enabled());
  uint64_t original_client_id = client_id();

  // Create browser for guest profile. Only "off the record" browsers may be
  // opened in this mode.
  Profile* guest_profile = CreateGuestProfile();
  Browser* guest_browser = CreateIncognitoBrowser(guest_profile);
  EXPECT_FALSE(ukm_enabled());

  CloseBrowserSynchronously(guest_browser);
  // TODO(crbug/746076): UKM doesn't actually get re-enabled yet.
  // EXPECT_TRUE(ukm_enabled());
  // Client ID should not have been reset.
  EXPECT_EQ(original_client_id, client_id());

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(regular_browser);
}

// Make sure that UKM is disabled while an non-sync profile's window is open.
IN_PROC_BROWSER_TEST_P(UkmBrowserTest, OpenNonSyncCheck) {
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_enabled());
  uint64_t original_client_id = client_id();
  EXPECT_NE(0U, original_client_id);

  Profile* nonsync_profile = CreateNonSyncProfile();
  Browser* nonsync_browser = CreateBrowser(nonsync_profile);
  EXPECT_FALSE(ukm_enabled());

  CloseBrowserSynchronously(nonsync_browser);
  // TODO(crbug/746076): UKM doesn't actually get re-enabled yet.
  // EXPECT_TRUE(ukm_enabled());
  // Client ID should not have been reset.
  EXPECT_EQ(original_client_id, client_id());

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(sync_browser);
}

// Make sure that UKM is disabled when metrics consent is revoked.
// Keep in sync with UkmTest.testMetricConsent in
// chrome/android/javatests/src/org/chromium/chrome/browser/sync/
// UkmTest.java.

IN_PROC_BROWSER_TEST_P(UkmBrowserTest, MetricsConsentCheck) {
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_enabled());
  uint64_t original_client_id = client_id();
  EXPECT_NE(0U, original_client_id);

  // Make sure there is a persistent log.
  BuildAndStoreUkmLog();
  EXPECT_TRUE(HasUnsentUkmLogs());

  metrics_consent.Update(false);
  EXPECT_FALSE(ukm_enabled());
  EXPECT_FALSE(HasUnsentUkmLogs());

  metrics_consent.Update(true);

  EXPECT_TRUE(ukm_enabled());
  // Client ID should have been reset.
  EXPECT_NE(original_client_id, client_id());

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(sync_browser);
}

IN_PROC_BROWSER_TEST_P(UkmBrowserTest, LogProtoData) {
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_enabled());
  uint64_t original_client_id = client_id();
  EXPECT_NE(0U, original_client_id);

  // Make sure there is a persistent log.
  BuildAndStoreUkmLog();
  EXPECT_TRUE(HasUnsentUkmLogs());

  // Check log contents.
  ukm::Report report = GetUkmReport();
  EXPECT_EQ(original_client_id, report.client_id());
  // Note: The version number reported in the proto may have a suffix, such as
  // "-64-devel", so use use StartsWith() rather than checking for equality.
  EXPECT_TRUE(base::StartsWith(report.system_profile().app_version(),
                               version_info::GetVersionNumber(),
                               base::CompareCase::SENSITIVE));

// Chrome OS hardware class comes from a different API than on other platforms.
#if defined(OS_CHROMEOS)
  EXPECT_EQ(variations::VariationsFieldTrialCreator::GetShortHardwareClass(),
            report.system_profile().hardware().hardware_class());
#else   // !defined(OS_CHROMEOS)
  EXPECT_EQ(base::SysInfo::HardwareModelName(),
            report.system_profile().hardware().hardware_class());
#endif  // defined(OS_CHROMEOS)

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(sync_browser);
}

// Verifies that network provider attaches effective connection type correctly
// to the UKM report.
IN_PROC_BROWSER_TEST_P(UkmBrowserTest, NetworkProviderPopulatesSystemProfile) {
  // Override network quality to 2G. This should cause the
  // |max_effective_connection_type| in the system profile to be set to 2G.
  g_browser_process->network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_2G);

  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_enabled());
  uint64_t original_client_id = client_id();
  EXPECT_NE(0U, original_client_id);

  // Override network quality to 4G. This should cause the
  // |max_effective_connection_type| in the system profile to be set to 4G.
  g_browser_process->network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_4G);

  // Make sure there is a persistent log.
  BuildAndStoreUkmLog();
  EXPECT_TRUE(HasUnsentUkmLogs());
  // Check log contents.
  ukm::Report report = GetUkmReport();

  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_2G,
            report.system_profile().network().min_effective_connection_type());
  EXPECT_EQ(SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_4G,
            report.system_profile().network().max_effective_connection_type());

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(sync_browser);
}

// Make sure that providing consent doesn't enable UKM when sync is disabled.
// Keep in sync with UkmTest.consentAddedButNoSyncCheck in
// chrome/android/javatests/src/org/chromium/chrome/browser/sync/
// UkmTest.java.
IN_PROC_BROWSER_TEST_P(UkmBrowserTest, ConsentAddedButNoSyncCheck) {
  MetricsConsentOverride metrics_consent(false);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  Browser* browser = CreateBrowser(profile);
  EXPECT_FALSE(ukm_enabled());

  metrics_consent.Update(true);
  EXPECT_FALSE(ukm_enabled());

  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);
  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(true);
  EXPECT_TRUE(ukm_enabled());

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(browser);
}

// Make sure that UKM is disabled when an open sync window disables history.
// Keep in sync with UkmTest.singleDisableHistorySyncCheck in
// chrome/android/javatests/src/org/chromium/chrome/browser/sync/
// UkmTest.java.
IN_PROC_BROWSER_TEST_P(UkmBrowserTest, SingleDisableHistorySyncCheck) {
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_enabled());
  uint64_t original_client_id = client_id();
  EXPECT_NE(0U, original_client_id);

  harness->DisableSyncForType(syncer::UserSelectableType::kHistory);
  if (unified_consent::IsUnifiedConsentFeatureEnabled()) {
    // Disable history sync does not disable UKM when unified consent is
    // enabled.
    EXPECT_TRUE(ukm_enabled());
  } else {
    EXPECT_FALSE(ukm_enabled());

    harness->EnableSyncForType(syncer::UserSelectableType::kHistory);
    EXPECT_TRUE(ukm_enabled());
    // Client ID should be reset.
    EXPECT_NE(original_client_id, client_id());
  }

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(sync_browser);
}

// Make sure that UKM is disabled when any open sync window disables history.
IN_PROC_BROWSER_TEST_P(UkmBrowserTest, MultiDisableHistorySyncCheck) {
  MetricsConsentOverride metrics_consent(true);

  Profile* profile1 = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness1 =
      EnableSyncForProfile(profile1);

  Browser* browser1 = CreateBrowser(profile1);
  EXPECT_TRUE(ukm_enabled());
  uint64_t original_client_id = client_id();
  EXPECT_NE(0U, original_client_id);

  Profile* profile2 = CreateNonSyncProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness2 =
      EnableSyncForProfile(profile2);
  Browser* browser2 = CreateBrowser(profile2);
  EXPECT_TRUE(ukm_enabled());
  EXPECT_EQ(original_client_id, client_id());

  harness2->DisableSyncForType(syncer::UserSelectableType::kHistory);
  if (unified_consent::IsUnifiedConsentFeatureEnabled()) {
    // Disable history sync does not disable UKM when unified consent is
    // enabled.
    EXPECT_TRUE(ukm_enabled());
  } else {
    EXPECT_FALSE(ukm_enabled());
    EXPECT_NE(original_client_id, client_id());
    original_client_id = client_id();
    EXPECT_NE(0U, original_client_id);
  }

  harness2->EnableSyncForType(syncer::UserSelectableType::kHistory);
  EXPECT_TRUE(ukm_enabled());
  EXPECT_EQ(original_client_id, client_id());

  harness2->service()->GetUserSettings()->SetSyncRequested(false);
  harness1->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(browser2);
  CloseBrowserSynchronously(browser1);
}

// Make sure that extension URLs are disabled when an open sync window
// disables it.
IN_PROC_BROWSER_TEST_P(UkmBrowserTest, SingleDisableExtensionsSyncCheck) {
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_enabled());
  EXPECT_TRUE(ukm_extensions_enabled());
  uint64_t original_client_id = client_id();
  EXPECT_NE(0U, original_client_id);

  ASSERT_TRUE(
      harness->DisableSyncForType(syncer::UserSelectableType::kExtensions));
  EXPECT_TRUE(ukm_enabled());
  EXPECT_FALSE(ukm_extensions_enabled());

  ASSERT_TRUE(
      harness->EnableSyncForType(syncer::UserSelectableType::kExtensions));
  EXPECT_TRUE(ukm_enabled());
  EXPECT_TRUE(ukm_extensions_enabled());
  // Client ID should not be reset.
  EXPECT_EQ(original_client_id, client_id());

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(sync_browser);
}

// Make sure that extension URLs are disabled when any open sync window
// disables it.
IN_PROC_BROWSER_TEST_P(UkmBrowserTest, MultiDisableExtensionsSyncCheck) {
  MetricsConsentOverride metrics_consent(true);

  Profile* profile1 = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness1 =
      EnableSyncForProfile(profile1);

  Browser* browser1 = CreateBrowser(profile1);
  EXPECT_TRUE(ukm_enabled());
  uint64_t original_client_id = client_id();
  EXPECT_NE(0U, original_client_id);

  Profile* profile2 = CreateNonSyncProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness2 =
      EnableSyncForProfile(profile2);
  Browser* browser2 = CreateBrowser(profile2);
  EXPECT_TRUE(ukm_enabled());
  EXPECT_TRUE(ukm_extensions_enabled());
  EXPECT_EQ(original_client_id, client_id());

  harness2->DisableSyncForType(syncer::UserSelectableType::kExtensions);
  EXPECT_TRUE(ukm_enabled());
  EXPECT_FALSE(ukm_extensions_enabled());

  harness2->EnableSyncForType(syncer::UserSelectableType::kExtensions);
  EXPECT_TRUE(ukm_enabled());
  EXPECT_TRUE(ukm_extensions_enabled());
  EXPECT_EQ(original_client_id, client_id());

  harness2->service()->GetUserSettings()->SetSyncRequested(false);
  harness1->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(browser2);
  CloseBrowserSynchronously(browser1);
}

IN_PROC_BROWSER_TEST_P(UkmBrowserTest, LogsTabId) {
  ASSERT_TRUE(embedded_test_server()->Start());
  MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);
  Browser* sync_browser = CreateBrowser(profile);

  const ukm::UkmSource* first_source = NavigateAndGetSource(
      sync_browser, embedded_test_server()->GetURL("/title1.html"));

  // Tab ids are incremented starting from 1. Since we started a new sync
  // browser, this is the second tab.
  EXPECT_EQ(2, first_source->navigation_data().tab_id);

  // Ensure the tab id is constant in a single tab.
  const ukm::UkmSource* second_source = NavigateAndGetSource(
      sync_browser, embedded_test_server()->GetURL("/title2.html"));
  EXPECT_EQ(first_source->navigation_data().tab_id,
            second_source->navigation_data().tab_id);

  // Add a new tab, it should get a new tab id.
  chrome::NewTab(sync_browser);
  const ukm::UkmSource* third_source = NavigateAndGetSource(
      sync_browser, embedded_test_server()->GetURL("/title3.html"));
  EXPECT_EQ(3, third_source->navigation_data().tab_id);
}

IN_PROC_BROWSER_TEST_P(UkmBrowserTest, LogsPreviousSourceId) {
  ASSERT_TRUE(embedded_test_server()->Start());
  MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);
  Browser* sync_browser = CreateBrowser(profile);

  const ukm::UkmSource* first_source = NavigateAndGetSource(
      sync_browser, embedded_test_server()->GetURL("/title1.html"));

  const ukm::UkmSource* second_source = NavigateAndGetSource(
      sync_browser, embedded_test_server()->GetURL("/title2.html"));
  EXPECT_EQ(first_source->id(),
            second_source->navigation_data().previous_source_id);

  // Open a new tab with window.open.
  content::WebContents* opener =
      sync_browser->tab_strip_model()->GetActiveWebContents();
  GURL new_tab_url = embedded_test_server()->GetURL("/title3.html");
  content::TestNavigationObserver waiter(new_tab_url);
  waiter.StartWatchingNewWebContents();
  EXPECT_TRUE(content::ExecuteScript(
      opener, content::JsReplace("window.open($1)", new_tab_url)));
  waiter.Wait();
  EXPECT_NE(opener, sync_browser->tab_strip_model()->GetActiveWebContents());
  ukm::SourceId new_id = ukm::GetSourceIdForWebContentsDocument(
      sync_browser->tab_strip_model()->GetActiveWebContents());
  ukm::UkmSource* new_tab_source = GetSource(new_id);
  EXPECT_NE(nullptr, new_tab_source);
  EXPECT_EQ(ukm::kInvalidSourceId,
            new_tab_source->navigation_data().previous_source_id);

  // Subsequent navigations within the tab should get a previous_source_id field
  // set.
  const ukm::UkmSource* subsequent_source = NavigateAndGetSource(
      sync_browser, embedded_test_server()->GetURL("/title3.html"));
  EXPECT_EQ(new_tab_source->id(),
            subsequent_source->navigation_data().previous_source_id);
}

IN_PROC_BROWSER_TEST_P(UkmBrowserTest, LogsOpenerSource) {
  ASSERT_TRUE(embedded_test_server()->Start());
  MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);
  Browser* sync_browser = CreateBrowser(profile);

  const ukm::UkmSource* first_source = NavigateAndGetSource(
      sync_browser, embedded_test_server()->GetURL("/title1.html"));
  // This tab was not opened by another tab, so it should not have an opener
  // id.
  EXPECT_EQ(ukm::kInvalidSourceId,
            first_source->navigation_data().opener_source_id);

  // Open a new tab with window.open.
  content::WebContents* opener =
      sync_browser->tab_strip_model()->GetActiveWebContents();
  GURL new_tab_url = embedded_test_server()->GetURL("/title2.html");
  content::TestNavigationObserver waiter(new_tab_url);
  waiter.StartWatchingNewWebContents();
  EXPECT_TRUE(content::ExecuteScript(
      opener, content::JsReplace("window.open($1)", new_tab_url)));
  waiter.Wait();
  EXPECT_NE(opener, sync_browser->tab_strip_model()->GetActiveWebContents());
  ukm::SourceId new_id = ukm::GetSourceIdForWebContentsDocument(
      sync_browser->tab_strip_model()->GetActiveWebContents());
  ukm::UkmSource* new_tab_source = GetSource(new_id);
  EXPECT_NE(nullptr, new_tab_source);
  EXPECT_EQ(first_source->id(),
            new_tab_source->navigation_data().opener_source_id);

  // Subsequent navigations within the tab should not get an opener set.
  const ukm::UkmSource* subsequent_source = NavigateAndGetSource(
      sync_browser, embedded_test_server()->GetURL("/title3.html"));
  EXPECT_EQ(ukm::kInvalidSourceId,
            subsequent_source->navigation_data().opener_source_id);
}

// Make sure that UKM is disabled when an secondary passphrase is set.
// Keep in sync with UkmTest.secondaryPassphraseCheck in
// chrome/android/javatests/src/org/chromium/chrome/browser/sync/
// UkmTest.java.
IN_PROC_BROWSER_TEST_F(UkmBrowserTestUnifiedConsentDisabled,
                       SecondaryPassphraseCheck) {
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_enabled());
  uint64_t original_client_id = client_id();
  EXPECT_NE(0U, original_client_id);

  // Setting an encryption passphrase is done on the "sync" thread meaning the
  // method only posts a task and returns. That task, when executed, will
  // set the passphrase and notify observers (which disables UKM).
  harness->service()->GetUserSettings()->SetEncryptionPassphrase("foo");
  UkmEnabledChecker checker(this, harness->service(), false);
  EXPECT_TRUE(checker.Wait());

  // Client ID should be reset.
  EXPECT_NE(original_client_id, client_id());

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(sync_browser);
}

// ChromeOS doesn't have the concept of sign-out so this test doesn't make sense
// there.
#if !defined(OS_CHROMEOS)
// Make sure that UKM is disabled when the profile signs out of Sync.
// Keep in sync with UkmTest.singleSyncSignoutCheck in
// chrome/android/javatests/src/org/chromium/chrome/browser/sync/
// UkmTest.java.
IN_PROC_BROWSER_TEST_P(UkmBrowserTest, SingleSyncSignoutCheck) {
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_enabled());
  uint64_t original_client_id = client_id();
  EXPECT_NE(0U, original_client_id);

  harness->SignOutPrimaryAccount();
  EXPECT_FALSE(ukm_enabled());
  EXPECT_NE(original_client_id, client_id());

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(sync_browser);
}
#endif  // !OS_CHROMEOS

// ChromeOS doesn't have the concept of sign-out so this test doesn't make sense
// there.
#if !defined(OS_CHROMEOS)
// Make sure that UKM is disabled when any profile signs out of Sync.
IN_PROC_BROWSER_TEST_P(UkmBrowserTest, MultiSyncSignoutCheck) {
  MetricsConsentOverride metrics_consent(true);

  Profile* profile1 = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness1 =
      EnableSyncForProfile(profile1);

  Browser* browser1 = CreateBrowser(profile1);
  EXPECT_TRUE(ukm_enabled());
  uint64_t original_client_id = client_id();
  EXPECT_NE(0U, original_client_id);

  Profile* profile2 = CreateNonSyncProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness2 =
      EnableSyncForProfile(profile2);
  Browser* browser2 = CreateBrowser(profile2);
  EXPECT_TRUE(ukm_enabled());
  EXPECT_EQ(original_client_id, client_id());

  harness2->SignOutPrimaryAccount();
  EXPECT_FALSE(ukm_enabled());
  EXPECT_NE(original_client_id, client_id());

  harness2->service()->GetUserSettings()->SetSyncRequested(false);
  harness1->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(browser2);
  CloseBrowserSynchronously(browser1);
}
#endif  // !OS_CHROMEOS

// Make sure that if history/sync services weren't available when we tried to
// attach listeners, UKM is not enabled.
IN_PROC_BROWSER_TEST_P(UkmBrowserTest, ServiceListenerInitFailedCheck) {
  MetricsConsentOverride metrics_consent(true);
  ChromeMetricsServiceClient::SetNotificationListenerSetupFailedForTesting(
      true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_FALSE(ukm_enabled());
  harness->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(sync_browser);
}

// Make sure that UKM is not affected by MetricsReporting Feature (sampling).
IN_PROC_BROWSER_TEST_P(UkmBrowserTest, MetricsReportingCheck) {
  // Need to set the Metrics Default to OPT_OUT to trigger MetricsReporting.
  DCHECK(g_browser_process);
  PrefService* local_state = g_browser_process->local_state();
  metrics::ForceRecordMetricsReportingDefaultState(
      local_state, metrics::EnableMetricsDefault::OPT_OUT);
  // Verify that kMetricsReportingFeature is disabled (i.e. other metrics
  // services will be sampled out).
  EXPECT_FALSE(
      base::FeatureList::IsEnabled(internal::kMetricsReportingFeature));

  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_enabled());

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(sync_browser);
}

// Make sure that pending data is deleted when user deletes history.
// Keep in sync with UkmTest.testHistoryDeleteCheck in
// chrome/android/javatests/src/org/chromium/chrome/browser/metrics/
// UkmTest.java.
IN_PROC_BROWSER_TEST_P(UkmBrowserTest, HistoryDeleteCheck) {
  MetricsConsentOverride metrics_consent(true);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);
  EXPECT_TRUE(ukm_enabled());
  uint64_t original_client_id = client_id();
  EXPECT_NE(0U, original_client_id);

  const ukm::SourceId kDummySourceId = 0x54321;
  RecordDummySource(kDummySourceId);
  EXPECT_TRUE(HasSource(kDummySourceId));

  ClearBrowsingData(profile);
  // Other sources may already have been recorded since the data was cleared,
  // but the dummy source should be gone.
  EXPECT_FALSE(HasSource(kDummySourceId));
  // Client ID should NOT be reset.
  EXPECT_EQ(original_client_id, client_id());
  EXPECT_TRUE(ukm_enabled());

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(sync_browser);
}

// Run UKM browser test suite with Unified Consent enabled and disabled.
INSTANTIATE_TEST_SUITE_P(, UkmBrowserTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(UkmBrowserTestWithSyncTransport, SyncFeatureCheck) {
  MetricsConsentOverride metrics_consent(true);

  // Set up Sync-the-feature.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            sync_service->GetTransportState());
  ASSERT_TRUE(sync_service->IsSyncFeatureActive());

  // Sanity check: UKM should now be active.
  ASSERT_TRUE(ukm_enabled());

  // Turn off Sync-the-feature by user choice. The machinery should start up
  // again in transport-only mode.
  sync_service->GetUserSettings()->SetSyncRequested(false);
  ASSERT_TRUE(harness->AwaitSyncTransportActive());

  // The Sync machinery is now active in transport mode (Sync-the-feature is
  // disabled).
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            sync_service->GetTransportState());
  ASSERT_FALSE(sync_service->IsSyncFeatureEnabled());

  // Trigger a Sync cycle so the connection status will resolve to
  // CONNECTION_OK.
  sync_service->TriggerRefresh(syncer::Intersection(
      sync_service->GetActiveDataTypes(), syncer::ProtocolTypes()));
  SyncConnectionOkChecker connection_ok(
      ProfileSyncServiceFactory::GetAsProfileSyncServiceForProfile(profile));
  ASSERT_TRUE(connection_ok.Wait());

  // History Sync is now not active anymore, but (maybe surprisingly) TYPED_URLS
  // is still considered part of the "chosen" data types, since the user hasn't
  // disabled it.
  ASSERT_FALSE(sync_service->GetActiveDataTypes().Has(syncer::TYPED_URLS));
  ASSERT_FALSE(sync_service->GetActiveDataTypes().Has(
      syncer::HISTORY_DELETE_DIRECTIVES));
  ASSERT_TRUE(sync_service->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));

  // If unified consent is disabled, then UKM should now be off, since Sync (the
  // feature) isn't enabled anymore, even though the machinery is still active.
  // With unified consent enabled, however, it's still on: The consent is *not*
  // revoked by (temporarily) turning off Sync.
  EXPECT_EQ(unified_consent::IsUnifiedConsentFeatureEnabled(), ukm_enabled());

  // Finally, turn Sync-the-feature on again.
  sync_service->GetUserSettings()->SetSyncRequested(true);
  ASSERT_TRUE(harness->AwaitSyncSetupCompletion());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            sync_service->GetTransportState());
  ASSERT_TRUE(sync_service->IsSyncFeatureActive());

  // Now UKM should be on again.
  EXPECT_TRUE(ukm_enabled());
}

// On ChromeOS, the test profile starts with a primary account already set, so
// this test doesn't apply.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(UkmBrowserTestWithSyncTransport,
                       NotEnabledForSecondaryAccountSync) {
  MetricsConsentOverride metrics_consent(true);

  // Signing in (without making the account Chrome's primary one or explicitly
  // setting up Sync) causes the Sync machinery to start up in standalone
  // transport mode.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      InitializeProfileForSync(profile);
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);

  secondary_account_helper::SignInSecondaryAccount(
      profile, &test_url_loader_factory_, "secondary_user@email.com");
  ASSERT_NE(syncer::SyncService::TransportState::DISABLED,
            sync_service->GetTransportState());
  ASSERT_TRUE(harness->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            sync_service->GetTransportState());
  ASSERT_FALSE(sync_service->IsSyncFeatureEnabled());

  // History Sync is not active, but (maybe surprisingly) TYPED_URLS is still
  // considered part of the "chosen" data types, since the user hasn't disabled
  // it.
  ASSERT_FALSE(sync_service->GetActiveDataTypes().Has(syncer::TYPED_URLS));
  ASSERT_FALSE(sync_service->GetActiveDataTypes().Has(
      syncer::HISTORY_DELETE_DIRECTIVES));
  ASSERT_TRUE(sync_service->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));

  EXPECT_FALSE(ukm_enabled());
}
#endif  // !OS_CHROMEOS

// Run UKM browser test suite with Unified Consent enabled and disabled.
INSTANTIATE_TEST_SUITE_P(, UkmBrowserTestWithSyncTransport, testing::Bool());

IN_PROC_BROWSER_TEST_P(UkmConsentParamBrowserTest, GroupPolicyConsentCheck) {
  // Note we are not using the synthetic MetricsConsentOverride since we are
  // testing directly from prefs.

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);

  Browser* sync_browser = CreateBrowser(profile);

  // The input param controls whether we set the prefs related to group policy
  // enabled or not. Based on its value, we should report the same value for
  // both if reporting is enabled and if UKM service is enabled.
  bool is_enabled = is_metrics_reporting_enabled_initial_value();
  EXPECT_EQ(is_enabled,
            UkmConsentParamBrowserTest::IsMetricsAndCrashReportingEnabled());
  EXPECT_EQ(is_enabled, ukm_enabled());

  harness->service()->GetUserSettings()->SetSyncRequested(false);
  CloseBrowserSynchronously(sync_browser);
}

// Verify UKM is enabled/disabled for both potential settings of group policy.
INSTANTIATE_TEST_SUITE_P(UkmConsentParamBrowserTests,
                         UkmConsentParamBrowserTest,
                         testing::Bool());

// Verify that sources kept alive in-memory will be discarded by UKM service in
// one reporting cycle after the web contents are destroyed when the tab is
// closed or when the user navigated away in the same tab.
IN_PROC_BROWSER_TEST_P(UkmBrowserTest, EvictObsoleteSources) {
  MetricsConsentOverride metrics_consent(true);
  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSyncForProfile(profile);
  Browser* sync_browser = CreateBrowser(profile);

  // Navigate to a URL in a new tab.
  AddTabAtIndexToBrowser(sync_browser, 1, GURL("https://www.chromium.org"),
                         ui::PAGE_TRANSITION_TYPED, true);
  ukm::SourceId source_id1 = ukm::GetSourceIdForWebContentsDocument(
      sync_browser->tab_strip_model()->GetWebContentsAt(1));
  ukm::SourceId source_id2 = ukm::kInvalidSourceId;

  // The UKM report contains this newly-created source.
  BuildAndStoreUkmLog();
  ukm::Report report = GetUkmReport();
  bool has_source_id1 = false;
  bool has_source_id2 = false;
  for (const auto& s : report.sources()) {
    has_source_id1 |= s.id() == source_id1;
    has_source_id2 |= s.id() == source_id2;
  }
  EXPECT_TRUE(has_source_id1);
  EXPECT_FALSE(has_source_id2);

  // Navigate to a URL in another new tab.
  AddTabAtIndexToBrowser(sync_browser, 2, GURL("https://www.google.com"),
                         ui::PAGE_TRANSITION_TYPED, true);
  source_id2 = ukm::GetSourceIdForWebContentsDocument(
      sync_browser->tab_strip_model()->GetWebContentsAt(2));

  // The next report should again contain source 1 because the tab is still
  // alive, and also source 2 associated to the new tab that has just been
  // opened.
  BuildAndStoreUkmLog();
  report = GetUkmReport();
  has_source_id1 = false;
  has_source_id2 = false;
  for (const auto& s : report.sources()) {
    has_source_id1 |= s.id() == source_id1;
    has_source_id2 |= s.id() == source_id2;
  }
  EXPECT_TRUE(has_source_id1);
  EXPECT_TRUE(has_source_id2);

  // Close the tab corresponding to source 1, this should mark source 1 as
  // obsolete. Next report will still contain source 1 because we might have
  // associated entries before it was closed.
  sync_browser->tab_strip_model()->CloseWebContentsAt(
      1, TabStripModel::CloseTypes::CLOSE_NONE);

  BuildAndStoreUkmLog();
  report = GetUkmReport();
  has_source_id1 = false;
  has_source_id2 = false;
  for (const auto& s : report.sources()) {
    has_source_id1 |= s.id() == source_id1;
    has_source_id2 |= s.id() == source_id2;
  }
  EXPECT_TRUE(has_source_id1);
  EXPECT_TRUE(has_source_id2);

  // Navigate to a new URL in the current tab, this will mark source 2 that was
  // in the current tab as obsolete.
  ui_test_utils::NavigateToURL(sync_browser, GURL("https://www.wikipedia.org"));

  // The previous report was the last one that could potentially contain entries
  // for source 1. Source 1 is thus no longer included in future reports. This
  // report will still contain source 2 because we might have associated entries
  // since the last report.
  BuildAndStoreUkmLog();
  report = GetUkmReport();
  has_source_id1 = false;
  has_source_id2 = false;
  for (const auto& s : report.sources()) {
    has_source_id1 |= s.id() == source_id1;
    has_source_id2 |= s.id() == source_id2;
  }
  EXPECT_FALSE(has_source_id1);
  EXPECT_TRUE(has_source_id2);

  // Neither source 1 or source 2 is alive anymore.
  BuildAndStoreUkmLog();
  report = GetUkmReport();
  has_source_id1 = false;
  has_source_id2 = false;
  for (const auto& s : report.sources()) {
    has_source_id1 |= s.id() == source_id1;
    has_source_id2 |= s.id() == source_id2;
  }
  EXPECT_FALSE(has_source_id1);
  EXPECT_FALSE(has_source_id2);

  CloseBrowserSynchronously(sync_browser);
}

}  // namespace metrics
