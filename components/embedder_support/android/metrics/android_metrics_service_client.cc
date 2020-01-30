// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/metrics/android_metrics_service_client.h"

#include <cstdint>

#include "base/i18n/rtl.h"
#include "build/build_config.h"
#include "components/embedder_support/android/metrics/android_metrics_log_uploader.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "components/metrics/cpu_metrics_provider.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/net/cellular_logic_helper.h"
#include "components/metrics/net/network_metrics_provider.h"
#include "components/metrics/stability_metrics_helper.h"
#include "components/metrics/ui/screen_info_metrics_provider.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

namespace metrics {

namespace {

// Callbacks for MetricsStateManager::Create. Store/LoadClientInfo
// allow Windows Chrome to back up ClientInfo. They're no-ops for
// AndroidMetricsServiceClient.

void StoreClientInfo(const ClientInfo& client_info) {}

std::unique_ptr<ClientInfo> LoadClientInfo() {
  std::unique_ptr<ClientInfo> client_info;
  return client_info;
}

bool UintFallsInBottomPercentOfValues(uint32_t value, double fraction) {
  DCHECK_GT(fraction, 0);
  DCHECK_LT(fraction, 1.00);

  // Since hashing is ~uniform, the chance that the value falls in the bottom
  // X% of possible values is X%. UINT32_MAX fits within the range of integers
  // that can be expressed precisely by a 64-bit double. Casting back to a
  // uint32_t means we can determine if the value falls within the bottom X%,
  // within a 1/UINT32_MAX error margin.
  uint32_t value_threshold =
      static_cast<uint32_t>(static_cast<double>(UINT32_MAX) * fraction);

  return value < value_threshold;
}

}  // namespace

AndroidMetricsServiceClient::AndroidMetricsServiceClient() = default;
AndroidMetricsServiceClient::~AndroidMetricsServiceClient() = default;

// static
void AndroidMetricsServiceClient::RegisterPrefs(PrefRegistrySimple* registry) {
  MetricsService::RegisterPrefs(registry);
  StabilityMetricsHelper::RegisterPrefs(registry);
}

void AndroidMetricsServiceClient::Initialize(PrefService* pref_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!init_finished_);

  pref_service_ = pref_service;

  InitInternal();
  metrics_state_manager_ =
      MetricsStateManager::Create(pref_service_, this, base::string16(),
                                  base::BindRepeating(&StoreClientInfo),
                                  base::BindRepeating(&LoadClientInfo));

  init_finished_ = true;
  MaybeStartMetrics();
}

void AndroidMetricsServiceClient::MaybeStartMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Treat the debugging flag the same as user consent because the user set it,
  // but keep app_consent_ separate so we never persist data from an opted-out
  // app.
  bool user_consent_or_flag = user_consent_ || IsMetricsReportingForceEnabled();
  if (init_finished_ && set_consent_finished_) {
    if (app_consent_ && user_consent_or_flag) {
      metrics_service_ = CreateMetricsService(metrics_state_manager_.get(),
                                              this, pref_service_);
      // Register for notifications so we can detect when the user or app are
      // interacting with the embedder. We use these as signals to wake up the
      // MetricsService.
      RegisterForNotifications();
      metrics_state_manager_->ForceClientIdCreation();
      OnMetricsStart();
      is_in_sample_ = IsInSample();
      if (IsReportingEnabled()) {
        // We assume the embedder has no shutdown sequence, so there's no need
        // for a matching Stop() call.
        metrics_service_->Start();
      }
    } else {
      pref_service_->ClearPref(prefs::kMetricsClientID);
    }
  }
}

std::unique_ptr<MetricsService>
AndroidMetricsServiceClient::CreateMetricsService(
    MetricsStateManager* state_manager,
    AndroidMetricsServiceClient* client,
    PrefService* prefs) {
  auto service = std::make_unique<MetricsService>(state_manager, client, prefs);
  // Although targeted at mobile, the unit tests runs on all platforms and the
  // chrome os version CHECK fails if we include NetworkMetricsProvider.
#if !defined(OS_CHROMEOS)
  service->RegisterMetricsProvider(std::make_unique<NetworkMetricsProvider>(
      content::CreateNetworkConnectionTrackerAsyncGetter()));
#endif  // defined(OS_CHROMEOS)
  service->RegisterMetricsProvider(std::make_unique<CPUMetricsProvider>());
  service->RegisterMetricsProvider(
      std::make_unique<ScreenInfoMetricsProvider>());
  service->RegisterMetricsProvider(
      std::make_unique<CallStackProfileMetricsProvider>());
  RegisterAdditionalMetricsProviders(service.get());
  service->InitializeMetricsRecordingState();
  return service;
}

void AndroidMetricsServiceClient::RegisterForNotifications() {
  registrar_.Add(this, content::NOTIFICATION_LOAD_START,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDER_WIDGET_HOST_HANG,
                 content::NotificationService::AllSources());
}

void AndroidMetricsServiceClient::SetHaveMetricsConsent(bool user_consent,
                                                        bool app_consent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  set_consent_finished_ = true;
  user_consent_ = user_consent;
  app_consent_ = app_consent;
  MaybeStartMetrics();
}

void AndroidMetricsServiceClient::SetFastStartupForTesting(
    bool fast_startup_for_testing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fast_startup_for_testing_ = fast_startup_for_testing;
}

void AndroidMetricsServiceClient::SetUploadIntervalForTesting(
    const base::TimeDelta& upload_interval) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  overridden_upload_interval_ = upload_interval;
}

std::unique_ptr<const base::FieldTrial::EntropyProvider>
AndroidMetricsServiceClient::CreateLowEntropyProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return metrics_state_manager_->CreateLowEntropyProvider();
}

bool AndroidMetricsServiceClient::IsConsentGiven() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return user_consent_ && app_consent_;
}

bool AndroidMetricsServiceClient::IsReportingEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!app_consent_)
    return false;
  return IsMetricsReportingForceEnabled() ||
         (EnabledStateProvider::IsReportingEnabled() && is_in_sample_);
}

MetricsService* AndroidMetricsServiceClient::GetMetricsService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This will be null if initialization hasn't finished, or if metrics
  // collection is disabled.
  return metrics_service_.get();
}

// In Chrome, UMA and Crashpad are enabled/disabled together by the same
// checkbox and they share the same client ID (a.k.a. GUID). SetMetricsClientId
// is intended to provide the ID to Breakpad. In AndroidMetricsServiceClients
// UMA and Crashpad are independent, so this is a no-op.
void AndroidMetricsServiceClient::SetMetricsClientId(
    const std::string& client_id) {}

std::string AndroidMetricsServiceClient::GetApplicationLocale() {
  return base::i18n::GetConfiguredLocale();
}

bool AndroidMetricsServiceClient::GetBrand(std::string* brand_code) {
  // AndroidMetricsServiceClients don't use brand codes.
  return false;
}

void AndroidMetricsServiceClient::CollectFinalMetricsForLog(
    base::OnceClosure done_callback) {
  std::move(done_callback).Run();
}

std::unique_ptr<MetricsLogUploader> AndroidMetricsServiceClient::CreateUploader(
    const GURL& server_url,
    const GURL& insecure_server_url,
    base::StringPiece mime_type,
    MetricsLogUploader::MetricServiceType service_type,
    const MetricsLogUploader::UploadCallback& on_upload_complete) {
  // |server_url|, |insecure_server_url|, and |mime_type| are unused because
  // AndroidMetricsServiceClients send metrics to the platform logging mechanism
  // rather than to Chrome's metrics server.
  return std::make_unique<AndroidMetricsLogUploader>(on_upload_complete);
}

base::TimeDelta AndroidMetricsServiceClient::GetStandardUploadInterval() {
  // In AndroidMetricsServiceClients, metrics collection (when we batch up all
  // logged histograms into a ChromeUserMetricsExtension proto) and metrics
  // uploading (when the proto goes to the server) happen separately.
  //
  // This interval controls the metrics collection rate, so we choose the
  // standard upload interval to make sure we're collecting metrics consistently
  // with Chrome for Android. The metrics uploading rate for
  // AndroidMetricsServiceClients is controlled by the platform logging
  // mechanism. Since this mechanism has its own logic for rate-limiting on
  // cellular connections, we disable the component-layer logic.
  if (!overridden_upload_interval_.is_zero()) {
    return overridden_upload_interval_;
  }
  return metrics::GetUploadInterval(false /* use_cellular_upload_interval */);
}

bool AndroidMetricsServiceClient::ShouldStartUpFastForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return fast_startup_for_testing_;
}

void AndroidMetricsServiceClient::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (type) {
    case content::NOTIFICATION_LOAD_STOP:
    case content::NOTIFICATION_LOAD_START:
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED:
    case content::NOTIFICATION_RENDER_WIDGET_HOST_HANG:
      if (ShouldWakeMetricsService())
        metrics_service_->OnApplicationNotIdle();
      break;
    default:
      NOTREACHED();
  }
}

bool AndroidMetricsServiceClient::IsInSample() {
  // Called in MaybeStartMetrics(), after |metrics_service_| is created.
  // NOTE IsInSample and IsInPackageNameSample deliberately use the same hash to
  // guarantee we never exceed 10% of total, opted-in clients for PackageNames.
  return IsInSample(base::PersistentHash(metrics_service_->GetClientId()));
}

bool AndroidMetricsServiceClient::IsInSample(uint32_t value) {
  return UintFallsInBottomPercentOfValues(value, GetSampleRate());
}

bool AndroidMetricsServiceClient::IsInPackageNameSample() {
  // Check if this client falls within the group for which it's acceptable to
  // log package name. This guarantees we enforce the privacy requirement
  // because we never log package names for more than kPackageNameLimitRate
  // percent of clients. We'll actually log package name for less than this,
  // because we also filter out packages for certain types of apps (see
  // CanRecordPackageNameForAppType()).
  return IsInPackageNameSample(
      base::PersistentHash(metrics_service_->GetClientId()));
}

bool AndroidMetricsServiceClient::IsInPackageNameSample(uint32_t value) {
  return UintFallsInBottomPercentOfValues(value, GetPackageNameLimitRate());
}

std::string AndroidMetricsServiceClient::GetAppPackageName() {
  if (IsInPackageNameSample() && CanRecordPackageNameForAppType())
    return GetAppPackageNameInternal();
  return std::string();
}

}  // namespace metrics
