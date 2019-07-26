// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_optimization_guide.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/rand_util.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/time/default_clock.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/optimization_guide/hint_cache_store.h"
#include "components/optimization_guide/hints_component_info.h"
#include "components/optimization_guide/hints_component_util.h"
#include "components/optimization_guide/hints_fetcher.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_prefs.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/optimization_guide_switches.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/top_host_provider.h"
#include "components/prefs/pref_service.h"
#include "components/previews/content/previews_hints.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_constants.h"
#include "components/previews/core/previews_switches.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace previews {

namespace {

// The component version used with a manual config. This ensures that any hint
// component received from the OptimizationGuideService on a subsequent startup
// will have a newer version than it.
constexpr char kManualConfigComponentVersion[] = "0.0.0";

// Delay between retries on failed fetch and store of hints from the remote
// Optimization Guide Service.
constexpr base::TimeDelta kFetchRetryDelay = base::TimeDelta::FromMinutes(15);

// Delay until successfully fetched hints should be updated by requesting from
// the remote Optimization Guide Service.
constexpr base::TimeDelta kUpdateFetchedHintsDelay =
    base::TimeDelta::FromHours(24);

// Provides a random time delta in seconds between |kFetchRandomMinDelay| and
// |kFetchRandomMaxDelay|.
base::TimeDelta RandomFetchDelay() {
  constexpr int kFetchRandomMinDelaySecs = 30;
  constexpr int kFetchRandomMaxDelaySecs = 60;
  return base::TimeDelta::FromSeconds(
      base::RandInt(kFetchRandomMinDelaySecs, kFetchRandomMaxDelaySecs));
}

}  // namespace

PreviewsOptimizationGuide::PreviewsOptimizationGuide(
    optimization_guide::OptimizationGuideService* optimization_guide_service,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
    const base::FilePath& profile_path,
    PrefService* pref_service,
    leveldb_proto::ProtoDatabaseProvider* database_provider,
    optimization_guide::TopHostProvider* top_host_provider,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : optimization_guide_service_(optimization_guide_service),
      ui_task_runner_(ui_task_runner),
      background_task_runner_(background_task_runner),
      hint_cache_(std::make_unique<optimization_guide::HintCache>(
          std::make_unique<optimization_guide::HintCacheStore>(
              database_provider,
              profile_path,
              pref_service,
              background_task_runner_))),
      top_host_provider_(top_host_provider),
      time_clock_(base::DefaultClock::GetInstance()),
      pref_service_(pref_service),
      url_loader_factory_(url_loader_factory) {
  DCHECK(optimization_guide_service_);
  hint_cache_->Initialize(
      optimization_guide::switches::ShouldPurgeHintCacheStoreOnStartup(),
      base::BindOnce(&PreviewsOptimizationGuide::OnHintCacheInitialized,
                     ui_weak_ptr_factory_.GetWeakPtr()));
}

PreviewsOptimizationGuide::~PreviewsOptimizationGuide() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  optimization_guide_service_->RemoveObserver(this);
}

bool PreviewsOptimizationGuide::IsWhitelisted(
    PreviewsUserData* previews_data,
    const GURL& url,
    PreviewsType type,
    net::EffectiveConnectionType* out_ect_threshold) const {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  if (!hints_) {
    return false;
  }

  *out_ect_threshold = params::GetECTThresholdForPreview(type);
  int inflation_percent = 0;
  std::string serialized_hint_version_string;
  if (!hints_->IsWhitelisted(url, type, &inflation_percent, out_ect_threshold,
                             &serialized_hint_version_string)) {
    return false;
  }

  if (inflation_percent != 0 && previews_data) {
    previews_data->set_data_savings_inflation_percent(inflation_percent);
  }

  if (!serialized_hint_version_string.empty() && previews_data) {
    previews_data->set_serialized_hint_version_string(
        serialized_hint_version_string);
  }

  return true;
}

bool PreviewsOptimizationGuide::IsBlacklisted(const GURL& url,
                                              PreviewsType type) const {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  if (type == PreviewsType::LITE_PAGE_REDIRECT) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kIgnoreLitePageRedirectOptimizationBlacklist)) {
      return false;
    }

    if (!hints_)
      return true;

    return hints_->IsBlacklisted(url, PreviewsType::LITE_PAGE_REDIRECT);
  }

  // This function is only used by lite page redirect.
  NOTREACHED();
  return false;
}

void PreviewsOptimizationGuide::OnLoadedHint(
    base::OnceClosure callback,
    const GURL& document_url,
    const optimization_guide::proto::Hint* loaded_hint) const {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // Record that the hint finished loading. This is used as a signal during
  // tests.
  LOCAL_HISTOGRAM_BOOLEAN(
      kPreviewsOptimizationGuideOnLoadedHintResultHistogramString, loaded_hint);

  // Run the callback now that the hint is loaded. This is used as a signal by
  // tests.
  std::move(callback).Run();
}

bool PreviewsOptimizationGuide::MaybeLoadOptimizationHints(
    const GURL& url,
    base::OnceClosure callback) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  if (!hints_) {
    return false;
  }

  return hints_->MaybeLoadOptimizationHints(
      url, base::BindOnce(&PreviewsOptimizationGuide::OnLoadedHint,
                          ui_weak_ptr_factory_.GetWeakPtr(),
                          std::move(callback), url));
}

bool PreviewsOptimizationGuide::GetResourceLoadingHints(
    const GURL& url,
    std::vector<std::string>* out_resource_patterns_to_block) const {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  if (!hints_)
    return false;

  return hints_->GetResourceLoadingHints(url, out_resource_patterns_to_block);
}

void PreviewsOptimizationGuide::LogHintCacheMatch(
    const GURL& url,
    bool is_committed,
    net::EffectiveConnectionType ect) const {
  if (!hints_) {
    return;
  }

  hints_->LogHintCacheMatch(url, is_committed, ect);
}

void PreviewsOptimizationGuide::OnHintCacheInitialized() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  // Check if there is a valid hint proto given on the command line first. We
  // don't normally expect one, but if one is provided then use that and do not
  // register as an observer as the opt_guide service.
  std::unique_ptr<optimization_guide::proto::Configuration> manual_config =
      optimization_guide::switches::ParseComponentConfigFromCommandLine();
  if (manual_config) {
    // Allow |UpdateHints| to block startup so that the first navigation gets
    // the hints when a command line hint proto is provided.
    UpdateHints(base::OnceClosure(),
                PreviewsHints::CreateFromHintsConfiguration(
                    std::move(manual_config),
                    hint_cache_->MaybeCreateUpdateDataForComponentHints(
                        base::Version(kManualConfigComponentVersion))));
  }



  // Register as an observer regardless of hint proto override usage. This is
  // needed as a signal during testing.
  optimization_guide_service_->AddObserver(this);
}

void PreviewsOptimizationGuide::OnHintsComponentAvailable(
    const optimization_guide::HintsComponentInfo& info) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // Check for if hint component is disabled. This check is needed because the
  // optimization guide still registers with the service as an observer for
  // components as a signal during testing.
  if (optimization_guide::switches::IsHintComponentProcessingDisabled()) {
    return;
  }

  // Create PreviewsHints from the newly available component on a background
  // thread, providing a HintUpdateData for component update from the hint
  // cache, so that each hint within the component can be moved into it. In the
  // case where the component's version is not newer than the hint cache store's
  // component version, HintUpdateData will be a nullptr and hint
  // processing will be skipped. After PreviewsHints::Create() returns the newly
  // created PreviewsHints, it is initialized in UpdateHints() on the UI thread.
  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(
          &PreviewsHints::CreateFromHintsComponent, info,
          hint_cache_->MaybeCreateUpdateDataForComponentHints(info.version)),
      base::BindOnce(&PreviewsOptimizationGuide::UpdateHints,
                     ui_weak_ptr_factory_.GetWeakPtr(),
                     std::move(next_update_closure_)));
}

void PreviewsOptimizationGuide::FetchHints() {
  base::Optional<std::vector<std::string>> top_hosts =
      optimization_guide::switches::ParseHintsFetchOverrideFromCommandLine();
  if (!top_hosts) {
    top_hosts = top_host_provider_->GetTopHosts(
        optimization_guide::features::
            MaxHostsForOptimizationGuideServiceHintsFetch());
  }
  DCHECK_GE(optimization_guide::features::
                MaxHostsForOptimizationGuideServiceHintsFetch(),
            top_hosts->size());

  if (!hints_fetcher_) {
    hints_fetcher_ = std::make_unique<optimization_guide::HintsFetcher>(
        url_loader_factory_,
        optimization_guide::features::GetOptimizationGuideServiceURL());
  }

  if (top_hosts->size() > 0) {
    hints_fetcher_->FetchOptimizationGuideServiceHints(
        *top_hosts, base::BindOnce(&PreviewsOptimizationGuide::OnHintsFetched,
                                   ui_weak_ptr_factory_.GetWeakPtr()));
  }
}

void PreviewsOptimizationGuide::OnHintsFetched(
    base::Optional<std::unique_ptr<optimization_guide::proto::GetHintsResponse>>
        get_hints_response) {
  // TODO(mcrouse): this will be dropped into a backgroundtask as it will likely
  // be intensive/slow storing hints.
  if (get_hints_response) {
    hint_cache_->UpdateFetchedHints(
        std::move(*get_hints_response),
        time_clock_->Now() + kUpdateFetchedHintsDelay,
        base::BindOnce(&PreviewsOptimizationGuide::OnFetchedHintsStored,
                       ui_weak_ptr_factory_.GetWeakPtr()));
  } else {
    // The fetch did not succeed so we will schedule to retry the fetch in
    // after delaying for |kFetchRetryDelay|
    // TODO(mcrouse): When the store is refactored from closures, the timer will
    // be scheduled on failure of the store instead.
    hints_fetch_timer_.Start(FROM_HERE, kFetchRetryDelay, this,
                             &PreviewsOptimizationGuide::ScheduleHintsFetch);
  }
}

void PreviewsOptimizationGuide::OnFetchedHintsStored() {
  hints_fetch_timer_.Stop();
  hints_fetch_timer_.Start(
      FROM_HERE, hint_cache_->FetchedHintsUpdateTime() - time_clock_->Now(),
      this, &PreviewsOptimizationGuide::ScheduleHintsFetch);
  // TODO(mcrouse): Purge hints now that new fetched hints have been stored.
}

void PreviewsOptimizationGuide::UpdateHints(
    base::OnceClosure update_closure,
    std::unique_ptr<PreviewsHints> hints) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  hints_ = std::move(hints);
  if (hints_) {
    hints_->Initialize(
        hint_cache_.get(),
        base::BindOnce(&PreviewsOptimizationGuide::OnHintsUpdated,
                       ui_weak_ptr_factory_.GetWeakPtr(),
                       std::move(update_closure)));
  } else {
    OnHintsUpdated(std::move(update_closure));
  }
}

void PreviewsOptimizationGuide::ClearFetchedHints() {
  DCHECK(hint_cache_);
  hint_cache_->ClearFetchedHints();
}

void PreviewsOptimizationGuide::OnHintsUpdated(
    base::OnceClosure update_closure) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(pref_service_);

  // Record the result of updating the hints. This is used as a signal for the
  // hints being fully processed in release tools and testing.
  LOCAL_HISTOGRAM_BOOLEAN(
      optimization_guide::kComponentHintsUpdatedResultHistogramString,
      hints_ != nullptr);

  if (!update_closure.is_null())
    std::move(update_closure).Run();

  // If the client is eligible to fetch hints, currently controlled by a feature
  // flag |kOptimizationHintsFetching|, fetch hints from the remote Optimization
  // Guide Service.
  //
  // TODO(mcrouse): Add a check if Infobar notification needs to be shown to the
  // user.
  if (!data_reduction_proxy::DataReductionProxySettings::
          IsDataSaverEnabledByUser(pref_service_)) {
    return;
  }

  if (!optimization_guide::features::IsHintsFetchingEnabled())
    return;

  if (optimization_guide::switches::ParseHintsFetchOverrideFromCommandLine() ||
      optimization_guide::switches::ShouldOverrideFetchHintsTimer()) {
    // Skip the fetch scheduling logic and perform a hints fetch immediately
    // after initialization.
    SetLastHintsFetchAttemptTime(time_clock_->Now());
    FetchHints();
  } else {
    ScheduleHintsFetch();
  }
}

void PreviewsOptimizationGuide::SetLastHintsFetchAttemptTime(
    base::Time last_attempt_time) {
  DCHECK(pref_service_);
  pref_service_->SetInt64(
      optimization_guide::prefs::kHintsFetcherLastFetchAttempt,
      last_attempt_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

base::Time PreviewsOptimizationGuide::GetLastHintsFetchAttemptTime() const {
  DCHECK(pref_service_);
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(pref_service_->GetInt64(
          optimization_guide::prefs::kHintsFetcherLastFetchAttempt)));
}

void PreviewsOptimizationGuide::ScheduleHintsFetch() {
  DCHECK(!hints_fetch_timer_.IsRunning());
  DCHECK(pref_service_);

  if (!data_reduction_proxy::DataReductionProxySettings::
          IsDataSaverEnabledByUser(pref_service_)) {
    return;
  }
  const base::TimeDelta time_until_update_time =
      hint_cache_->FetchedHintsUpdateTime() - time_clock_->Now();
  const base::TimeDelta time_until_retry =
      GetLastHintsFetchAttemptTime() + kFetchRetryDelay - time_clock_->Now();
  base::TimeDelta fetcher_delay;
  if (time_until_update_time <= base::TimeDelta() &&
      time_until_retry <= base::TimeDelta()) {
    // Fetched hints in the store should be updated and an attempt has not
    // been made in last |kFetchRetryDelay|.
    SetLastHintsFetchAttemptTime(time_clock_->Now());
    hints_fetch_timer_.Start(FROM_HERE, RandomFetchDelay(), this,
                             &PreviewsOptimizationGuide::FetchHints);
  } else {
    if (time_until_update_time >= base::TimeDelta()) {
      // If the fetched hints in the store are still up-to-date, set a timer
      // for when the hints need to be updated.
      fetcher_delay = time_until_update_time;
    } else {
      // Otherwise, hints need to be updated but an attempt was made in last
      // |kFetchRetryDelay|. Schedule the timer for after the retry
      // delay.
      fetcher_delay = time_until_retry;
    }
    hints_fetch_timer_.Start(FROM_HERE, fetcher_delay, this,
                             &PreviewsOptimizationGuide::ScheduleHintsFetch);
  }
}

void PreviewsOptimizationGuide::SetTimeClockForTesting(
    const base::Clock* time_clock) {
  time_clock_ = time_clock;
}

void PreviewsOptimizationGuide::SetHintsFetcherForTesting(
    std::unique_ptr<optimization_guide::HintsFetcher> hints_fetcher) {
  hints_fetcher_ = std::move(hints_fetcher);
}

optimization_guide::HintsFetcher*
PreviewsOptimizationGuide::GetHintsFetcherForTesting() {
  return hints_fetcher_.get();
}

void PreviewsOptimizationGuide::ListenForNextUpdateForTesting(
    base::OnceClosure next_update_closure) {
  DCHECK(next_update_closure_.is_null())
      << "Only one update closure is supported at a time";
  next_update_closure_ = std::move(next_update_closure);
}

}  // namespace previews
