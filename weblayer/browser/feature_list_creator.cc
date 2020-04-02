// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/feature_list_creator.h"

#include "base/base_switches.h"
#include "base/debug/leak_annotations.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/ui_string_overrider.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_crash_keys.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_switch_dependent_feature_overrides.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "weblayer/browser/android/metrics/weblayer_metrics_service_client.h"
#include "weblayer/browser/system_network_context_manager.h"
#include "weblayer/browser/weblayer_variations_service_client.h"

#if defined(OS_ANDROID)
namespace switches {
const char kDisableBackgroundNetworking[] = "disable-background-networking";
}  // namespace switches
#endif

namespace weblayer {
namespace {

FeatureListCreator* feature_list_creator_instance = nullptr;

void HandleReadError(PersistentPrefStore::PrefReadError error) {}

#if defined(OS_ANDROID)
base::FilePath GetPrefStorePath() {
  base::FilePath path;
  base::PathService::Get(base::DIR_ANDROID_APP_DATA, &path);
  path = path.Append(FILE_PATH_LITERAL("pref_store"));
  return path;
}
#endif

std::unique_ptr<PrefService> CreatePrefService() {
  auto pref_registry = base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();

#if defined(OS_ANDROID)
  metrics::AndroidMetricsServiceClient::RegisterPrefs(pref_registry.get());
#endif
  variations::VariationsService::RegisterPrefs(pref_registry.get());
  PrefServiceFactory pref_service_factory;

#if defined(OS_ANDROID)
  pref_service_factory.set_user_prefs(
      base::MakeRefCounted<JsonPrefStore>(GetPrefStorePath()));
#else
  // For now just use in memory PrefStore for desktop.
  // TODO(weblayer-dev): Find a long term solution.
  pref_service_factory.set_user_prefs(
      base::MakeRefCounted<InMemoryPrefStore>());
#endif

  pref_service_factory.set_read_error_callback(
      base::BindRepeating(&HandleReadError));

  return pref_service_factory.Create(pref_registry);
}

}  // namespace

FeatureListCreator::FeatureListCreator() {
  DCHECK(!feature_list_creator_instance);
  feature_list_creator_instance = this;
}

FeatureListCreator::~FeatureListCreator() {
  feature_list_creator_instance = nullptr;
}

// static
FeatureListCreator* FeatureListCreator::GetInstance() {
  DCHECK(feature_list_creator_instance);
  return feature_list_creator_instance;
}

void FeatureListCreator::SetSystemNetworkContextManager(
    SystemNetworkContextManager* system_network_context_manager) {
  system_network_context_manager_ = system_network_context_manager;
}

void FeatureListCreator::SetUpFieldTrials() {
#if defined(OS_ANDROID)
  auto* metrics_client = WebLayerMetricsServiceClient::GetInstance();

  // Initialize FieldTrialList to support FieldTrials. If an instance already
  // exists, this is likely a test scenario with a ScopedFeatureList active,
  // so use that one to apply any overrides.
  if (!base::FieldTrialList::GetInstance()) {
    // Note: This is intentionally leaked since it needs to live for the
    // duration of the browser process and there's no benefit in cleaning it up
    // at exit.
    // Note: We deliberately use CreateLowEntropyProvider because
    // CreateDefaultEntropyProvider needs to know if user conset has been given
    // but getting consent from GMS is slow.
    base::FieldTrialList* leaked_field_trial_list = new base::FieldTrialList(
        metrics_client->metrics_state_manager()->CreateLowEntropyProvider());
    ANNOTATE_LEAKING_OBJECT_PTR(leaked_field_trial_list);
    ignore_result(leaked_field_trial_list);
  }

  DCHECK(system_network_context_manager_);
  variations_service_ = variations::VariationsService::Create(
      std::make_unique<WebLayerVariationsServiceClient>(
          system_network_context_manager_),
      local_state_.get(), metrics_client->metrics_state_manager(),
      switches::kDisableBackgroundNetworking, variations::UIStringOverrider(),
      base::BindOnce(&content::GetNetworkConnectionTracker));
  variations_service_->OverridePlatform(
      variations::Study::PLATFORM_ANDROID_WEBLAYER, "android_weblayer");

  std::set<std::string> unforceable_field_trials;
  std::vector<std::string> variation_ids;
  auto feature_list = std::make_unique<base::FeatureList>();

  variations_service_->SetupFieldTrials(
      cc::switches::kEnableGpuBenchmarking, switches::kEnableFeatures,
      switches::kDisableFeatures, unforceable_field_trials, variation_ids,
      content::GetSwitchDependentFeatureOverrides(
          *base::CommandLine::ForCurrentProcess()),
      std::move(feature_list), &weblayer_field_trials_);
  variations::InitCrashKeys();
#else
  // TODO(weblayer-dev): Support variations on desktop.
#endif
}

void FeatureListCreator::CreateFeatureListAndFieldTrials() {
  local_state_ = CreatePrefService();
  CHECK(local_state_);
#if defined(OS_ANDROID)
  WebLayerMetricsServiceClient::GetInstance()->Initialize(local_state_.get());
#endif
  SetUpFieldTrials();
}

void FeatureListCreator::PerformPreMainMessageLoopStartup() {
#if defined(OS_ANDROID)
  // It is expected this is called after SetUpFieldTrials().
  DCHECK(variations_service_);
  variations_service_->PerformPreMainMessageLoopStartup();
#endif
}

void FeatureListCreator::OnBrowserFragmentStarted() {
  if (has_browser_fragment_started_)
    return;

  has_browser_fragment_started_ = true;
  // It is expected this is called after SetUpFieldTrials().
  DCHECK(variations_service_);

  // This function is called any time a BrowserFragment is started.
  // OnAppEnterForeground() really need only be called once, and because our
  // notion of a fragment doesn't really map to the Application as a whole,
  // call this function once.
  variations_service_->OnAppEnterForeground();
}

}  // namespace weblayer
