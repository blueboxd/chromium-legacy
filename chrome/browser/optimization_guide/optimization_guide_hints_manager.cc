// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_hints_manager.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/rand_util.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/time/default_clock.h"
#include "components/optimization_guide/bloom_filter.h"
#include "components/optimization_guide/hint_cache.h"
#include "components/optimization_guide/hint_cache_store.h"
#include "components/optimization_guide/hints_component_util.h"
#include "components/optimization_guide/hints_fetcher.h"
#include "components/optimization_guide/hints_processing_util.h"
#include "components/optimization_guide/optimization_filter.h"
#include "components/optimization_guide/optimization_guide_constants.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_prefs.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/optimization_guide_switches.h"
#include "components/optimization_guide/top_host_provider.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

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

void MaybeRunUpdateClosure(base::OnceClosure update_closure) {
  if (update_closure)
    std::move(update_closure).Run();
}

// Returns whether the particular component version can be processed, and if it
// can be, locks the semaphore (in the form of a pref) to signal that the
// processing of this particular version has started.
bool CanProcessComponentVersion(PrefService* pref_service,
                                const base::Version& version) {
  DCHECK(version.IsValid());

  const std::string previous_attempted_version_string = pref_service->GetString(
      optimization_guide::prefs::kPendingHintsProcessingVersion);
  if (!previous_attempted_version_string.empty()) {
    const base::Version previous_attempted_version =
        base::Version(previous_attempted_version_string);
    if (!previous_attempted_version.IsValid()) {
      DLOG(ERROR) << "Bad contents in hints processing pref";
      // Clear pref for fresh start next time.
      pref_service->ClearPref(
          optimization_guide::prefs::kPendingHintsProcessingVersion);
      return false;
    }
    if (previous_attempted_version.CompareTo(version) == 0) {
      // Previously attempted same version without completion.
      return false;
    }
  }

  // Write config version to pref.
  pref_service->SetString(
      optimization_guide::prefs::kPendingHintsProcessingVersion,
      version.GetString());
  return true;
}

}  // namespace

OptimizationGuideHintsManager::OptimizationGuideHintsManager(
    optimization_guide::OptimizationGuideService* optimization_guide_service,
    const base::FilePath& profile_path,
    PrefService* pref_service,
    leveldb_proto::ProtoDatabaseProvider* database_provider,
    optimization_guide::TopHostProvider* top_host_provider,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : optimization_guide_service_(optimization_guide_service),
      background_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::BEST_EFFORT})),
      pref_service_(pref_service),
      hint_cache_(std::make_unique<optimization_guide::HintCache>(
          std::make_unique<optimization_guide::HintCacheStore>(
              database_provider,
              profile_path,
              pref_service_,
              background_task_runner_))),
      top_host_provider_(top_host_provider),
      url_loader_factory_(url_loader_factory),
      clock_(base::DefaultClock::GetInstance()) {
  DCHECK(optimization_guide_service_);
  hint_cache_->Initialize(
      optimization_guide::switches::ShouldPurgeHintCacheStoreOnStartup(),
      base::BindOnce(&OptimizationGuideHintsManager::OnHintCacheInitialized,
                     ui_weak_ptr_factory_.GetWeakPtr()));
}

OptimizationGuideHintsManager::~OptimizationGuideHintsManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  optimization_guide_service_->RemoveObserver(this);
}

void OptimizationGuideHintsManager::Shutdown() {
  optimization_guide_service_->RemoveObserver(this);
}

void OptimizationGuideHintsManager::OnHintsComponentAvailable(
    const optimization_guide::HintsComponentInfo& info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Check for if hint component is disabled. This check is needed because the
  // optimization guide still registers with the service as an observer for
  // components as a signal during testing.
  if (optimization_guide::switches::IsHintComponentProcessingDisabled()) {
    MaybeRunUpdateClosure(std::move(next_update_closure_));
    return;
  }

  if (!CanProcessComponentVersion(pref_service_, info.version)) {
    optimization_guide::RecordProcessHintsComponentResult(
        optimization_guide::ProcessHintsComponentResult::
            kFailedFinishProcessing);
    MaybeRunUpdateClosure(std::move(next_update_closure_));
    return;
  }

  std::unique_ptr<optimization_guide::HintUpdateData> update_data =
      hint_cache_->MaybeCreateUpdateDataForComponentHints(info.version);

  // Processes the hints from the newly available component on a background
  // thread, providing a HintUpdateData for component update from the hint
  // cache, so that each hint within the component can be moved into it. In the
  // case where the component's version is not newer than the hint cache store's
  // component version, HintUpdateData will be a nullptr and hint
  // processing will be skipped. After PreviewsHints::Create() returns the newly
  // created PreviewsHints, it is initialized in UpdateHints() on the UI thread.
  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(&OptimizationGuideHintsManager::ProcessHintsComponent,
                     base::Unretained(this), info,
                     registered_optimization_types_, std::move(update_data)),
      base::BindOnce(&OptimizationGuideHintsManager::UpdateComponentHints,
                     ui_weak_ptr_factory_.GetWeakPtr(),
                     std::move(next_update_closure_)));

  // Only replace hints component info if it is not the same - otherwise we will
  // destruct the object and it will be invalid later.
  if (!hints_component_info_ ||
      hints_component_info_->version.CompareTo(info.version) != 0) {
    hints_component_info_.emplace(info.version, info.path);
  }
}

std::unique_ptr<optimization_guide::HintUpdateData>
OptimizationGuideHintsManager::ProcessHintsComponent(
    const optimization_guide::HintsComponentInfo& info,
    const std::unordered_set<optimization_guide::proto::OptimizationType>&
        registered_optimization_types,
    std::unique_ptr<optimization_guide::HintUpdateData> update_data) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  optimization_guide::ProcessHintsComponentResult out_result;
  std::unique_ptr<optimization_guide::proto::Configuration> config =
      optimization_guide::ProcessHintsComponent(info, &out_result);
  if (!config) {
    optimization_guide::RecordProcessHintsComponentResult(out_result);
    return nullptr;
  }

  ProcessOptimizationFilters(config->optimization_blacklists(),
                             registered_optimization_types);

  if (update_data) {
    bool did_process_hints = optimization_guide::ProcessHints(
        config->mutable_hints(), update_data.get());
    optimization_guide::RecordProcessHintsComponentResult(
        did_process_hints
            ? optimization_guide::ProcessHintsComponentResult::kSuccess
            : optimization_guide::ProcessHintsComponentResult::
                  kProcessedNoHints);
  } else {
    optimization_guide::RecordProcessHintsComponentResult(
        optimization_guide::ProcessHintsComponentResult::
            kSkippedProcessingHints);
  }

  return update_data;
}

void OptimizationGuideHintsManager::ProcessOptimizationFilters(
    const google::protobuf::RepeatedPtrField<
        optimization_guide::proto::OptimizationFilter>&
        blacklist_optimization_filters,
    const std::unordered_set<optimization_guide::proto::OptimizationType>&
        registered_optimization_types) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  base::AutoLock lock(optimization_filters_lock_);

  optimization_types_with_filter_.clear();
  blacklist_optimization_filters_.clear();
  for (const auto& filter : blacklist_optimization_filters) {
    if (filter.optimization_type() !=
        optimization_guide::proto::TYPE_UNSPECIFIED) {
      optimization_types_with_filter_.insert(filter.optimization_type());
    }

    // Do not put anything in memory that we don't have registered.
    if (registered_optimization_types.find(filter.optimization_type()) ==
        registered_optimization_types.end()) {
      continue;
    }

    optimization_guide::RecordOptimizationFilterStatus(
        filter.optimization_type(),
        optimization_guide::OptimizationFilterStatus::
            kFoundServerBlacklistConfig);

    // Do not parse duplicate optimization filters.
    if (blacklist_optimization_filters_.find(filter.optimization_type()) !=
        blacklist_optimization_filters_.end()) {
      optimization_guide::RecordOptimizationFilterStatus(
          filter.optimization_type(),
          optimization_guide::OptimizationFilterStatus::
              kFailedServerBlacklistDuplicateConfig);
      continue;
    }

    // Parse optimization filter.
    optimization_guide::OptimizationFilterStatus status;
    std::unique_ptr<optimization_guide::OptimizationFilter>
        optimization_filter =
            optimization_guide::ProcessOptimizationFilter(filter, &status);
    if (optimization_filter) {
      blacklist_optimization_filters_.insert(
          {filter.optimization_type(), std::move(optimization_filter)});
    }
    optimization_guide::RecordOptimizationFilterStatus(
        filter.optimization_type(), status);
  }
}

void OptimizationGuideHintsManager::OnHintCacheInitialized() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Check if there is a valid hint proto given on the command line first. We
  // don't normally expect one, but if one is provided then use that and do not
  // register as an observer as the opt_guide service.
  std::unique_ptr<optimization_guide::proto::Configuration> manual_config =
      optimization_guide::switches::ParseComponentConfigFromCommandLine();
  if (manual_config) {
    std::unique_ptr<optimization_guide::HintUpdateData> hint_update_data =
        hint_cache_->MaybeCreateUpdateDataForComponentHints(
            base::Version(kManualConfigComponentVersion));
    optimization_guide::ProcessHints(manual_config->mutable_hints(),
                                     hint_update_data.get());
    // Allow |UpdateComponentHints| to block startup so that the first
    // navigation gets the hints when a command line hint proto is provided.
    UpdateComponentHints(base::DoNothing(), std::move(hint_update_data));
  }

  MaybeScheduleHintsFetch();

  // Register as an observer regardless of hint proto override usage. This is
  // needed as a signal during testing.
  optimization_guide_service_->AddObserver(this);
}

void OptimizationGuideHintsManager::UpdateComponentHints(
    base::OnceClosure update_closure,
    std::unique_ptr<optimization_guide::HintUpdateData> hint_update_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If we get here, the hints have been processed correctly.
  pref_service_->ClearPref(
      optimization_guide::prefs::kPendingHintsProcessingVersion);

  if (hint_update_data) {
    hint_cache_->UpdateComponentHints(
        std::move(hint_update_data),
        base::BindOnce(&OptimizationGuideHintsManager::OnComponentHintsUpdated,
                       ui_weak_ptr_factory_.GetWeakPtr(),
                       std::move(update_closure),
                       /* hints_updated=*/true));
  } else {
    OnComponentHintsUpdated(std::move(update_closure), /*hints_updated=*/false);
  }
}

void OptimizationGuideHintsManager::OnComponentHintsUpdated(
    base::OnceClosure update_closure,
    bool hints_updated) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Record the result of updating the hints. This is used as a signal for the
  // hints being fully processed in testing.
  LOCAL_HISTOGRAM_BOOLEAN(
      optimization_guide::kComponentHintsUpdatedResultHistogramString,
      hints_updated);

  MaybeRunUpdateClosure(std::move(update_closure));
}

void OptimizationGuideHintsManager::ListenForNextUpdateForTesting(
    base::OnceClosure next_update_closure) {
  DCHECK(!next_update_closure_)
      << "Only one update closure is supported at a time";
  next_update_closure_ = std::move(next_update_closure);
}

void OptimizationGuideHintsManager::SetClockForTesting(
    const base::Clock* clock) {
  clock_ = clock;
}

void OptimizationGuideHintsManager::SetHintsFetcherForTesting(
    std::unique_ptr<optimization_guide::HintsFetcher> hints_fetcher) {
  hints_fetcher_ = std::move(hints_fetcher);
}

void OptimizationGuideHintsManager::MaybeScheduleHintsFetch() {
  if (!optimization_guide::features::IsHintsFetchingEnabled() ||
      !top_host_provider_) {
    return;
  }

  if (optimization_guide::switches::ShouldOverrideFetchHintsTimer()) {
    SetLastHintsFetchAttemptTime(clock_->Now());
    FetchHints();
  } else {
    ScheduleHintsFetch();
  }
}

void OptimizationGuideHintsManager::ScheduleHintsFetch() {
  DCHECK(!hints_fetch_timer_.IsRunning());

  const base::TimeDelta time_until_update_time =
      hint_cache_->FetchedHintsUpdateTime() - clock_->Now();
  const base::TimeDelta time_until_retry =
      GetLastHintsFetchAttemptTime() + kFetchRetryDelay - clock_->Now();
  base::TimeDelta fetcher_delay;
  if (time_until_update_time <= base::TimeDelta() &&
      time_until_retry <= base::TimeDelta()) {
    // Fetched hints in the store should be updated and an attempt has not
    // been made in last |kFetchRetryDelay|.
    SetLastHintsFetchAttemptTime(clock_->Now());
    hints_fetch_timer_.Start(FROM_HERE, RandomFetchDelay(), this,
                             &OptimizationGuideHintsManager::FetchHints);
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
    hints_fetch_timer_.Start(
        FROM_HERE, fetcher_delay, this,
        &OptimizationGuideHintsManager::ScheduleHintsFetch);
  }
}

void OptimizationGuideHintsManager::FetchHints() {
  DCHECK(top_host_provider_);

  size_t max_hints_to_fetch = optimization_guide::features::
      MaxHostsForOptimizationGuideServiceHintsFetch();
  std::vector<std::string> top_hosts =
      top_host_provider_->GetTopHosts(max_hints_to_fetch);
  if (top_hosts.empty())
    return;
  DCHECK_GE(max_hints_to_fetch, top_hosts.size());

  if (!hints_fetcher_) {
    hints_fetcher_ = std::make_unique<optimization_guide::HintsFetcher>(
        url_loader_factory_,
        optimization_guide::features::GetOptimizationGuideServiceURL());
  }
  hints_fetcher_->FetchOptimizationGuideServiceHints(
      top_hosts, base::BindOnce(&OptimizationGuideHintsManager::OnHintsFetched,
                                ui_weak_ptr_factory_.GetWeakPtr()));
}

void OptimizationGuideHintsManager::OnHintsFetched(
    base::Optional<std::unique_ptr<optimization_guide::proto::GetHintsResponse>>
        get_hints_response) {
  if (get_hints_response) {
    hint_cache_->UpdateFetchedHints(
        std::move(*get_hints_response),
        clock_->Now() + kUpdateFetchedHintsDelay,
        base::BindOnce(&OptimizationGuideHintsManager::OnFetchedHintsStored,
                       ui_weak_ptr_factory_.GetWeakPtr()));
  } else {
    // The fetch did not succeed so we will schedule to retry the fetch in
    // after delaying for |kFetchRetryDelay|
    // TODO(mcrouse): When the store is refactored from closures, the timer will
    // be scheduled on failure of the store instead.
    hints_fetch_timer_.Start(
        FROM_HERE, kFetchRetryDelay, this,
        &OptimizationGuideHintsManager::ScheduleHintsFetch);
  }
}

void OptimizationGuideHintsManager::OnFetchedHintsStored() {
  hints_fetch_timer_.Stop();
  hints_fetch_timer_.Start(
      FROM_HERE, hint_cache_->FetchedHintsUpdateTime() - clock_->Now(), this,
      &OptimizationGuideHintsManager::ScheduleHintsFetch);
}

base::Time OptimizationGuideHintsManager::GetLastHintsFetchAttemptTime() const {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(pref_service_->GetInt64(
          optimization_guide::prefs::kHintsFetcherLastFetchAttempt)));
}

void OptimizationGuideHintsManager::SetLastHintsFetchAttemptTime(
    base::Time last_attempt_time) {
  pref_service_->SetInt64(
      optimization_guide::prefs::kHintsFetcherLastFetchAttempt,
      last_attempt_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

void OptimizationGuideHintsManager::LoadHintForNavigation(
    content::NavigationHandle* navigation_handle,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const auto& url = navigation_handle->GetURL();
  if (!url.has_host()) {
    std::move(callback).Run();
    return;
  }

  hint_cache_->LoadHint(
      url.host(),
      base::BindOnce(&OptimizationGuideHintsManager::OnHintLoaded,
                     ui_weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void OptimizationGuideHintsManager::OnHintLoaded(
    base::OnceClosure callback,
    const optimization_guide::proto::Hint* loaded_hint) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Record the result of loading a hint. This is used as a signal for testing.
  LOCAL_HISTOGRAM_BOOLEAN(optimization_guide::kLoadedHintLocalHistogramString,
                          loaded_hint);

  // Run the callback now that the hint is loaded. This is used as a signal by
  // tests.
  std::move(callback).Run();
}

void OptimizationGuideHintsManager::RegisterOptimizationTypes(
    std::vector<optimization_guide::proto::OptimizationType>
        optimization_types) {
  bool should_load_new_optimization_filter = false;
  for (const auto optimization_type : optimization_types) {
    if (optimization_type == optimization_guide::proto::TYPE_UNSPECIFIED)
      continue;

    if (registered_optimization_types_.find(optimization_type) !=
        registered_optimization_types_.end()) {
      continue;
    }
    registered_optimization_types_.insert(optimization_type);

    if (!should_load_new_optimization_filter) {
      base::AutoLock lock(optimization_filters_lock_);
      if (optimization_types_with_filter_.find(optimization_type) !=
          optimization_types_with_filter_.end()) {
        should_load_new_optimization_filter = true;
      }
    }
  }

  if (should_load_new_optimization_filter) {
    DCHECK(hints_component_info_);

    OnHintsComponentAvailable(*hints_component_info_);
  } else {
    MaybeRunUpdateClosure(std::move(next_update_closure_));
  }
}

bool OptimizationGuideHintsManager::HasLoadedOptimizationFilter(
    optimization_guide::proto::OptimizationType optimization_type) {
  base::AutoLock lock(optimization_filters_lock_);

  return blacklist_optimization_filters_.find(optimization_type) !=
         blacklist_optimization_filters_.end();
}

optimization_guide::OptimizationGuideDecision
OptimizationGuideHintsManager::CanApplyOptimization(
    content::NavigationHandle* navigation_handle,
    optimization_guide::OptimizationTarget optimization_target,
    optimization_guide::proto::OptimizationType optimization_type,
    optimization_guide::OptimizationMetadata* optimization_metadata) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Clear out optimization metadata if provided.
  if (optimization_metadata)
    (*optimization_metadata).previews_metadata.Clear();

  // We only support the optimization target |kPainfulPageLoad|, so just return
  // that we don't know if the target doesn't match that.
  if (optimization_target !=
      optimization_guide::OptimizationTarget::kPainfulPageLoad) {
    return optimization_guide::OptimizationGuideDecision::kUnknown;
  }

  // We do not have an estimate for the effective connection type, so just say
  // it's not painful.
  if (current_effective_connection_type_ ==
      net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
    return optimization_guide::OptimizationGuideDecision::kFalse;
  }

  const auto& url = navigation_handle->GetURL();
  // If the URL doesn't have a host, we cannot query the hint for it, so just
  // return early.
  if (!url.has_host()) {
    return optimization_guide::OptimizationGuideDecision::kFalse;
  }
  const auto& host = url.host();

  net::EffectiveConnectionType max_ect_trigger =
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_2G;

  // TODO(sophiechang): Maybe cache the page hint for a navigation ID so we
  // don't have to iterate through all page hints every time this is called.

  // Check if we have a hint already loaded for this navigation.
  const optimization_guide::proto::Hint* loaded_hint =
      hint_cache_->GetHintIfLoaded(host);
  const optimization_guide::proto::PageHint* matched_page_hint =
      loaded_hint ? optimization_guide::FindPageHintForURL(url, loaded_hint)
                  : nullptr;
  if (matched_page_hint && matched_page_hint->has_max_ect_trigger()) {
    max_ect_trigger = optimization_guide::ConvertProtoEffectiveConnectionType(
        matched_page_hint->max_ect_trigger());
  }

  // The current network is not slow enough, so this navigation is likely not
  // going to be painful.
  if (current_effective_connection_type_ > max_ect_trigger)
    return optimization_guide::OptimizationGuideDecision::kFalse;

  // Check if the URL should be filtered out if we have an optimization filter
  // for the type.
  {
    base::AutoLock lock(optimization_filters_lock_);

    // Check if we have a filter loaded into memory for it, and if we do, see
    // if the URL matches anything in the filter.
    if (blacklist_optimization_filters_.find(optimization_type) !=
        blacklist_optimization_filters_.end()) {
      return blacklist_optimization_filters_[optimization_type]->Matches(url)
                 ? optimization_guide::OptimizationGuideDecision::kFalse
                 : optimization_guide::OptimizationGuideDecision::kTrue;
    }

    // Check if we had an optimization filter for it, but it was not loaded into
    // memory.
    if (optimization_types_with_filter_.find(optimization_type) !=
        optimization_types_with_filter_.end()) {
      return optimization_guide::OptimizationGuideDecision::kUnknown;
    }
  }

  if (!loaded_hint) {
    // If we do not have a hint already loaded and we do not have one in the
    // cache, we don't know what to do with the URL so just return false.
    // Otherwise, we do have information, but we just don't know it yet.
    return hint_cache_->HasHint(host)
               ? optimization_guide::OptimizationGuideDecision::kUnknown
               : optimization_guide::OptimizationGuideDecision::kFalse;
  }
  if (!matched_page_hint)
    return optimization_guide::OptimizationGuideDecision::kFalse;

  // Now check if we have any optimizations for it.
  for (const auto& optimization :
       matched_page_hint->whitelisted_optimizations()) {
    if (optimization_type != optimization.optimization_type())
      continue;

    if (optimization_guide::IsDisabledPerOptimizationHintExperiment(
            optimization)) {
      continue;
    }

    // We found an optimization that can be applied. Populate optimization
    // metadata if applicable and return true.
    if (optimization_metadata && optimization.has_previews_metadata()) {
      (*optimization_metadata).previews_metadata =
          optimization.previews_metadata();
    }
    return optimization_guide::OptimizationGuideDecision::kTrue;
  }

  // We didn't find anything, return false.
  return optimization_guide::OptimizationGuideDecision::kFalse;
}

void OptimizationGuideHintsManager::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType effective_connection_type) {
  current_effective_connection_type_ = effective_connection_type;
}
