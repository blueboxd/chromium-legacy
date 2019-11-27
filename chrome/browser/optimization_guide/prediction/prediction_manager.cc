// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/prediction/prediction_manager.h"

#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_navigation_data.h"
#include "chrome/browser/optimization_guide/optimization_guide_permissions_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_session_statistic.h"
#include "chrome/browser/optimization_guide/prediction/prediction_model.h"
#include "chrome/browser/optimization_guide/prediction/prediction_model_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/optimization_guide/optimization_guide_constants.h"
#include "components/optimization_guide/optimization_guide_decider.h"
#include "components/optimization_guide/optimization_guide_enums.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_prefs.h"
#include "components/optimization_guide/optimization_guide_store.h"
#include "components/optimization_guide/optimization_guide_switches.h"
#include "components/optimization_guide/store_update_data.h"
#include "components/optimization_guide/top_host_provider.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace {

// Returns true if |optimization_target_decision| reflects that the model had
// already been evaluated.
bool ShouldUseCurrentOptimizationTargetDecision(
    optimization_guide::OptimizationTargetDecision
        optimization_target_decision) {
  switch (optimization_target_decision) {
    case optimization_guide::OptimizationTargetDecision::kPageLoadMatches:
    case optimization_guide::OptimizationTargetDecision::kPageLoadDoesNotMatch:
    case optimization_guide::OptimizationTargetDecision::
        kModelPredictionHoldback:
      return true;
    case optimization_guide::OptimizationTargetDecision::
        kModelNotAvailableOnClient:
    case optimization_guide::OptimizationTargetDecision::kUnknown:
    case optimization_guide::OptimizationTargetDecision::kDeciderNotInitialized:
      return false;
  }
}

// Delay between retries on failed fetch and store of prediction models and
// host model features from the remote Optimization Guide Service.
constexpr base::TimeDelta kFetchRetryDelay = base::TimeDelta::FromMinutes(16);

// The amount of time to wait after a successful fetch of models and host model
// features before requesting an update from the remote Optimization Guide
// Service.
constexpr base::TimeDelta kUpdateModelsAndFeaturesDelay =
    base::TimeDelta::FromHours(24);

// Provide a random time delta in seconds before fetching models and host model
// features.
base::TimeDelta RandomFetchDelay() {
  return base::TimeDelta::FromSeconds(base::RandInt(
      optimization_guide::features::PredictionModelFetchRandomMinDelaySecs(),
      optimization_guide::features::PredictionModelFetchRandomMaxDelaySecs()));
}

}  // namespace

namespace optimization_guide {

PredictionManager::PredictionManager(
    const std::vector<optimization_guide::proto::OptimizationTarget>&
        optimization_targets_at_initialization,
    const base::FilePath& profile_path,
    leveldb_proto::ProtoDatabaseProvider* database_provider,
    TopHostProvider* top_host_provider,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* pref_service,
    Profile* profile)
    : PredictionManager(
          optimization_targets_at_initialization,
          std::make_unique<OptimizationGuideStore>(
              database_provider,
              profile_path.AddExtensionASCII(
                  optimization_guide::
                      kOptimizationGuidePredictionModelAndFeaturesStore),
              base::CreateSequencedTaskRunner(
                  {base::ThreadPool(), base::MayBlock(),
                   base::TaskPriority::BEST_EFFORT})),
          top_host_provider,
          url_loader_factory,
          pref_service,
          profile) {}

PredictionManager::PredictionManager(
    const std::vector<optimization_guide::proto::OptimizationTarget>&
        optimization_targets_at_initialization,
    std::unique_ptr<OptimizationGuideStore> model_and_features_store,
    TopHostProvider* top_host_provider,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* pref_service,
    Profile* profile)
    : session_fcp_(),
      top_host_provider_(top_host_provider),
      model_and_features_store_(std::move(model_and_features_store)),
      url_loader_factory_(url_loader_factory),
      pref_service_(pref_service),
      profile_(profile),
      clock_(base::DefaultClock::GetInstance()) {
  Initialize(optimization_targets_at_initialization);
}

PredictionManager::~PredictionManager() {
  g_browser_process->network_quality_tracker()
      ->RemoveEffectiveConnectionTypeObserver(this);
}

void PredictionManager::Initialize(const std::vector<proto::OptimizationTarget>&
                                       optimization_targets_at_initialization) {
  RegisterOptimizationTargets(optimization_targets_at_initialization);
  g_browser_process->network_quality_tracker()
      ->AddEffectiveConnectionTypeObserver(this);
  model_and_features_store_->Initialize(
      switches::ShouldPurgeModelAndFeaturesStoreOnStartup(),
      base::BindOnce(&PredictionManager::OnStoreInitialized,
                     ui_weak_ptr_factory_.GetWeakPtr()));
}

void PredictionManager::UpdateFCPSessionStatistics(base::TimeDelta fcp) {
  previous_load_fcp_ms_ = static_cast<float>(fcp.InMilliseconds());
  session_fcp_.AddSample(*previous_load_fcp_ms_);
}

void PredictionManager::RegisterOptimizationTargets(
    const std::vector<proto::OptimizationTarget>& optimization_targets) {
  SEQUENCE_CHECKER(sequence_checker_);

  if (optimization_targets.size() == 0)
    return;

  base::flat_set<proto::OptimizationTarget> new_optimization_targets;
  for (const auto& optimization_target : optimization_targets) {
    if (optimization_target == proto::OPTIMIZATION_TARGET_UNKNOWN)
      continue;
    if (registered_optimization_targets_.find(optimization_target) !=
        registered_optimization_targets_.end()) {
      continue;
    }
    registered_optimization_targets_.insert(optimization_target);
    new_optimization_targets.insert(optimization_target);
  }

  // Before loading/fetching models and features, the store must be ready.
  if (!store_is_ready_)
    return;

  // Only proceed if there are newly registered targets to load/fetch models and
  // features for. Otherwise, the registered targets will have models loaded
  // when the store was initialized.
  if (new_optimization_targets.size() == 0)
    return;

  // Start loading the host model features if they are not already.
  if (!host_model_features_loaded_) {
    LoadHostModelFeatures();
    return;
  }
  // Otherwise, the host model features are loaded, so load prediction models
  // for any newly registered targets.
  LoadPredictionModels(new_optimization_targets);
}

base::Optional<float> PredictionManager::GetValueForClientFeature(
    const std::string& model_feature,
    content::NavigationHandle* navigation_handle) const {
  SEQUENCE_CHECKER(sequence_checker_);

  proto::ClientModelFeature client_model_feature;
  if (!proto::ClientModelFeature_Parse(model_feature, &client_model_feature))
    return base::nullopt;

  switch (client_model_feature) {
    case proto::CLIENT_MODEL_FEATURE_UNKNOWN: {
      return base::nullopt;
    }
    case proto::CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE: {
      return static_cast<float>(current_effective_connection_type_);
    }
    case proto::CLIENT_MODEL_FEATURE_PAGE_TRANSITION: {
      return static_cast<float>(navigation_handle->GetPageTransition());
    }
    case proto::CLIENT_MODEL_FEATURE_SITE_ENGAGEMENT_SCORE: {
      Profile* profile = Profile::FromBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext());
      SiteEngagementService* engagement_service =
          SiteEngagementService::Get(profile);
      // Precision loss is acceptable/expected for prediction models.
      return static_cast<float>(
          engagement_service->GetScore(navigation_handle->GetURL()));
    }
    case proto::CLIENT_MODEL_FEATURE_SAME_ORIGIN_NAVIGATION: {
      return static_cast<float>(url::IsSameOriginWith(
          navigation_handle->GetURL(), navigation_handle->GetPreviousURL()));
    }
    case proto::CLIENT_MODEL_FEATURE_FIRST_CONTENTFUL_PAINT_SESSION_MEAN: {
      return session_fcp_.GetMean();
    }
    case proto::
        CLIENT_MODEL_FEATURE_FIRST_CONTENTFUL_PAINT_SESSION_STANDARD_DEVIATION: {
      return session_fcp_.GetStdDev();
    }
    case proto::
        CLIENT_MODEL_FEATURE_FIRST_CONTENTFUL_PAINT_PREVIOUS_PAGE_LOAD: {
      return previous_load_fcp_ms_.value_or(0.0);
    }
    default: {
      return base::nullopt;
    }
  }
}

base::flat_map<std::string, float> PredictionManager::BuildFeatureMap(
    content::NavigationHandle* navigation_handle,
    const base::flat_set<std::string>& model_features) const {
  SEQUENCE_CHECKER(sequence_checker_);
  base::flat_map<std::string, float> feature_map;
  if (model_features.size() == 0)
    return feature_map;

  const base::flat_map<std::string, float>* host_model_features = nullptr;

  std::string host = navigation_handle->GetURL().host();
  auto it = host_model_features_map_.find(host);
  if (it != host_model_features_map_.end())
    host_model_features = &(it->second);

  UMA_HISTOGRAM_BOOLEAN(
      "OptimizationGuide.PredictionManager.HasHostModelFeaturesForHost",
      host_model_features != nullptr);

  // If the feature is not implemented by the client, it is assumed that it is a
  // host model feature we have in the map. If it is not in either, a default is
  // created for it. This ensures that the prediction model will have values for
  // every feature that it requires to be evaluated.
  for (const auto& model_feature : model_features) {
    base::Optional<float> value =
        GetValueForClientFeature(model_feature, navigation_handle);
    if (value) {
      feature_map[model_feature] = *value;
      continue;
    }
    if (!host_model_features || !host_model_features->contains(model_feature)) {
      feature_map[model_feature] = 0.0;
      continue;
    }
    feature_map[model_feature] =
        host_model_features->find(model_feature)->second;
  }
  return feature_map;
}

OptimizationTargetDecision PredictionManager::ShouldTargetNavigation(
    content::NavigationHandle* navigation_handle,
    proto::OptimizationTarget optimization_target) const {
  SEQUENCE_CHECKER(sequence_checker_);
  DCHECK(navigation_handle->GetURL().SchemeIsHTTPOrHTTPS());

  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle);
  if (navigation_data) {
    base::Optional<optimization_guide::OptimizationTargetDecision>
        optimization_target_decision =
            navigation_data->GetDecisionForOptimizationTarget(
                optimization_target);
    if (optimization_target_decision.has_value() &&
        ShouldUseCurrentOptimizationTargetDecision(
            *optimization_target_decision)) {
      return *optimization_target_decision;
    }
  }

  // TODO(crbug/1001194): Add histogram to record that the optimization target
  // was not registered but was requested.
  if (!registered_optimization_targets_.contains(optimization_target))
    return OptimizationTargetDecision::kUnknown;

  auto it = optimization_target_prediction_model_map_.find(optimization_target);
  if (it == optimization_target_prediction_model_map_.end()) {
    // TODO(crbug/1001194): Check the store to see if there is a model
    // available. There will also be a check with metrics on if the model was
    // available in the but not loaded.
    return OptimizationTargetDecision::kModelNotAvailableOnClient;
  }
  PredictionModel* prediction_model = it->second.get();

  base::flat_map<std::string, float> feature_map =
      BuildFeatureMap(navigation_handle, prediction_model->GetModelFeatures());

  double prediction_score = 0.0;
  optimization_guide::OptimizationTargetDecision target_decision =
      prediction_model->Predict(feature_map, &prediction_score);

  if (navigation_data) {
    navigation_data->SetModelVersionForOptimizationTarget(
        optimization_target, prediction_model->GetVersion());
    navigation_data->SetModelPredictionScoreForOptimizationTarget(
        optimization_target, prediction_score);
  }

  if (optimization_guide::features::
          ShouldOverrideOptimizationTargetDecisionForMetricsPurposes(
              optimization_target)) {
    return optimization_guide::OptimizationTargetDecision::
        kModelPredictionHoldback;
  }

  return target_decision;
}

void PredictionManager::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType effective_connection_type) {
  SEQUENCE_CHECKER(sequence_checker_);
  current_effective_connection_type_ = effective_connection_type;
}

PredictionModel* PredictionManager::GetPredictionModelForTesting(
    proto::OptimizationTarget optimization_target) const {
  auto it = optimization_target_prediction_model_map_.find(optimization_target);
  if (it != optimization_target_prediction_model_map_.end())
    return it->second.get();
  return nullptr;
}

base::flat_map<std::string, base::flat_map<std::string, float>>
PredictionManager::GetHostModelFeaturesForTesting() const {
  return host_model_features_map_;
}

void PredictionManager::SetPredictionModelFetcherForTesting(
    std::unique_ptr<PredictionModelFetcher> prediction_model_fetcher) {
  prediction_model_fetcher_ = std::move(prediction_model_fetcher);
}

void PredictionManager::FetchModelsAndHostModelFeatures() {
  SEQUENCE_CHECKER(sequence_checker_);
  if (!top_host_provider_ ||
      !IsUserPermittedToFetchFromRemoteOptimizationGuide(profile_)) {
    return;
  }

  // TODO(crbug/1027596): Update the prediction model fetcher to run the
  // callback even in failure so that the fetch can be rescheduled on failure
  // rather than preemptively.
  ScheduleModelsAndHostModelFeaturesFetch();

  // Models and host model features should not be fetched if there are no
  // optimization targets registered.
  if (registered_optimization_targets_.size() == 0)
    return;

  std::vector<std::string> top_hosts = top_host_provider_->GetTopHosts();

  if (!prediction_model_fetcher_) {
    prediction_model_fetcher_ = std::make_unique<PredictionModelFetcher>(
        url_loader_factory_,
        features::GetOptimizationGuideServiceGetModelsURL());
  }

  std::vector<proto::ModelInfo> models_info = std::vector<proto::ModelInfo>();

  proto::ModelInfo base_model_info;
  for (auto client_model_feature = proto::ClientModelFeature_MIN + 1;
       client_model_feature <= proto::ClientModelFeature_MAX;
       client_model_feature++) {
    if (proto::ClientModelFeature_IsValid(client_model_feature)) {
      base_model_info.add_supported_model_features(
          static_cast<proto::ClientModelFeature>(client_model_feature));
    }
  }
  // Only Decision Trees are currently supported.
  base_model_info.add_supported_model_types(proto::MODEL_TYPE_DECISION_TREE);

  // For now, we will fetch for all registered optimization targets.
  for (const auto& optimization_target : registered_optimization_targets_) {
    proto::ModelInfo model_info(base_model_info);
    model_info.set_optimization_target(optimization_target);

    auto it =
        optimization_target_prediction_model_map_.find(optimization_target);
    if (it != optimization_target_prediction_model_map_.end())
      model_info.set_version(it->second.get()->GetVersion());

    models_info.push_back(model_info);
  }

  prediction_model_fetcher_->FetchOptimizationGuideServiceModels(
      models_info, top_hosts, optimization_guide::proto::CONTEXT_BATCH_UPDATE,
      base::BindOnce(&PredictionManager::OnModelsAndHostFeaturesFetched,
                     ui_weak_ptr_factory_.GetWeakPtr()));
}

void PredictionManager::OnModelsAndHostFeaturesFetched(
    base::Optional<std::unique_ptr<proto::GetModelsResponse>>
        get_models_response_data) {
  SEQUENCE_CHECKER(sequence_checker_);
  if (!get_models_response_data)
    return;

  // Update host model features, even if empty so the store metadata
  // that contains the update time for new models and features to be fetched
  // from the remote Optimization Guide Service is updated.
  UpdateHostModelFeatures((*get_models_response_data)->host_model_features());

  // If no models were returned, no updates to the store are required.
  if ((*get_models_response_data)->models_size() > 0)
    UpdatePredictionModels((*get_models_response_data)->models());

  // TODO(crbug/1001194): After the models and host model features are stored
  // asynchronously, the timer will be set based on the update time provided
  // by the store.
  fetch_timer_.Stop();
  fetch_timer_.Start(
      FROM_HERE, kUpdateModelsAndFeaturesDelay, this,
      &PredictionManager::ScheduleModelsAndHostModelFeaturesFetch);
}

void PredictionManager::UpdateHostModelFeatures(
    const google::protobuf::RepeatedPtrField<proto::HostModelFeatures>&
        host_model_features) {
  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<StoreUpdateData> host_model_features_update_data =
      StoreUpdateData::CreateHostModelFeaturesStoreUpdateData(
          /*update_time=*/clock_->Now() + kUpdateModelsAndFeaturesDelay,
          /*expiry_time=*/clock_->Now() +
              features::StoredHostModelFeaturesFreshnessDuration());
  for (const auto& host_model_features : host_model_features) {
    if (ProcessAndStoreHostModelFeatures(host_model_features)) {
      host_model_features_update_data->CopyHostModelFeaturesIntoUpdateData(
          host_model_features);
    }
  }
  model_and_features_store_->UpdateHostModelFeatures(
      std::move(host_model_features_update_data),
      base::BindOnce(&PredictionManager::OnHostModelFeaturesStored,
                     ui_weak_ptr_factory_.GetWeakPtr()));
}

std::unique_ptr<PredictionModel> PredictionManager::CreatePredictionModel(
    const proto::PredictionModel& model) const {
  SEQUENCE_CHECKER(sequence_checker_);
  return PredictionModel::Create(
      std::make_unique<proto::PredictionModel>(model));
}

void PredictionManager::UpdatePredictionModels(
    const google::protobuf::RepeatedPtrField<proto::PredictionModel>&
        prediction_models) {
  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<StoreUpdateData> prediction_model_update_data =
      StoreUpdateData::CreatePredictionModelStoreUpdateData();
  bool models_to_store = false;
  for (const auto& model : prediction_models) {
    if (ProcessAndStorePredictionModel(model)) {
      prediction_model_update_data->CopyPredictionModelIntoUpdateData(model);
      models_to_store = true;
    }
  }
  if (models_to_store) {
    model_and_features_store_->UpdatePredictionModels(
        std::move(prediction_model_update_data),
        base::BindOnce(&PredictionManager::OnPredictionModelsStored,
                       ui_weak_ptr_factory_.GetWeakPtr()));
  }
}

void PredictionManager::OnPredictionModelsStored() {
  SEQUENCE_CHECKER(sequence_checker_);
  LOCAL_HISTOGRAM_BOOLEAN(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", true);
}

void PredictionManager::OnHostModelFeaturesStored() {
  SEQUENCE_CHECKER(sequence_checker_);
  LOCAL_HISTOGRAM_BOOLEAN(
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", true);
  // TODO(crbug/1027596): Stopping the timer can be removed once the fetch
  // callback refactor is done. Otherwise, at the start of a fetch, a timer is
  // running to handle the cases that a fetch fails but the callback is not run.
  fetch_timer_.Stop();
  ScheduleModelsAndHostModelFeaturesFetch();
}

void PredictionManager::OnStoreInitialized() {
  SEQUENCE_CHECKER(sequence_checker_);
  store_is_ready_ = true;

  // Only load host model features if there are optimization targets registered.
  if (registered_optimization_targets_.size() == 0)
    return;

  // The store is ready so start loading host model features and the models for
  // the registered optimization targets.  Once the host model features are
  // loaded, prediction models for the registered optimization targets will be
  // loaded.
  LoadHostModelFeatures();

  MaybeScheduleModelAndHostModelFeaturesFetch();
}

void PredictionManager::LoadHostModelFeatures() {
  SEQUENCE_CHECKER(sequence_checker_);
  // Load the host model features first, each prediction model requires the set
  // of host model features to be known before creation.
  model_and_features_store_->LoadAllHostModelFeatures(
      base::BindOnce(&PredictionManager::OnLoadHostModelFeatures,
                     ui_weak_ptr_factory_.GetWeakPtr()));
}

void PredictionManager::OnLoadHostModelFeatures(
    std::unique_ptr<std::vector<proto::HostModelFeatures>>
        all_host_model_features) {
  SEQUENCE_CHECKER(sequence_checker_);
  // If the store returns an empty vector of host model features, the store
  // contains no host model features. However, the load is otherwise complete
  // and prediction models can be loaded but they will require no host model
  // feature information.
  host_model_features_loaded_ = true;
  if (all_host_model_features) {
    for (const auto& host_model_features : *all_host_model_features)
      ProcessAndStoreHostModelFeatures(host_model_features);
  }
  UMA_HISTOGRAM_COUNTS_1000(
      "OptimizationGuide.PredictionManager.HostModelFeaturesMapSize",
      host_model_features_map_.size());

  // Load the prediction models for all the registered optimization targets now
  // that it is not blocked by loading the host model features.
  LoadPredictionModels(registered_optimization_targets_);
}

void PredictionManager::LoadPredictionModels(
    const base::flat_set<proto::OptimizationTarget>& optimization_targets) {
  SEQUENCE_CHECKER(sequence_checker_);
  DCHECK(host_model_features_loaded_);

  OptimizationGuideStore::EntryKey model_entry_key;
  for (const auto& optimization_target : optimization_targets) {
    // The prediction model for this optimization target has already been
    // loaded.
    if (optimization_target_prediction_model_map_.contains(optimization_target))
      continue;
    if (!model_and_features_store_->FindPredictionModelEntryKey(
            optimization_target, &model_entry_key)) {
      continue;
    }
    model_and_features_store_->LoadPredictionModel(
        model_entry_key,
        base::BindOnce(&PredictionManager::OnLoadPredictionModel,
                       ui_weak_ptr_factory_.GetWeakPtr()));
  }
}

void PredictionManager::OnLoadPredictionModel(
    std::unique_ptr<proto::PredictionModel> model) {
  SEQUENCE_CHECKER(sequence_checker_);
  if (model)
    ProcessAndStorePredictionModel(*model);
}

bool PredictionManager::ProcessAndStorePredictionModel(
    const proto::PredictionModel& model) {
  SEQUENCE_CHECKER(sequence_checker_);
  if (!model.model_info().has_optimization_target())
    return false;
  if (!registered_optimization_targets_.contains(
          model.model_info().optimization_target())) {
    return false;
  }

  std::unique_ptr<PredictionModel> prediction_model =
      CreatePredictionModel(model);
  if (!prediction_model)
    return false;

  auto it = optimization_target_prediction_model_map_.find(
      model.model_info().optimization_target());
  if (it == optimization_target_prediction_model_map_.end()) {
    optimization_target_prediction_model_map_.emplace(
        model.model_info().optimization_target(), std::move(prediction_model));
    return true;
  }
  if (it->second->GetVersion() != prediction_model->GetVersion()) {
    it->second = std::move(prediction_model);
    return true;
  }
  return false;
}

bool PredictionManager::ProcessAndStoreHostModelFeatures(
    const proto::HostModelFeatures& host_model_features) {
  SEQUENCE_CHECKER(sequence_checker_);
  if (!host_model_features.has_host())
    return false;
  if (host_model_features.model_features_size() == 0)
    return false;

  base::flat_map<std::string, float> model_features_for_host;
  model_features_for_host.reserve(host_model_features.model_features_size());
  for (const auto& model_feature : host_model_features.model_features()) {
    if (!model_feature.has_feature_name())
      continue;
    switch (model_feature.feature_value_case()) {
      case proto::ModelFeature::kDoubleValue:
        // Loss of precision from double is acceptable for features supported
        // by the prediction models.
        model_features_for_host.emplace(
            model_feature.feature_name(),
            static_cast<float>(model_feature.double_value()));
        break;
      case proto::ModelFeature::kInt64Value:
        model_features_for_host.emplace(
            model_feature.feature_name(),
            static_cast<float>(model_feature.int64_value()));
        break;
      case proto::ModelFeature::FEATURE_VALUE_NOT_SET:
        NOTREACHED();
        break;
    }
  }
  if (model_features_for_host.size() == 0)
    return false;

  host_model_features_map_[host_model_features.host()] =
      model_features_for_host;
  return true;
}

void PredictionManager::MaybeScheduleModelAndHostModelFeaturesFetch() {
  if (optimization_guide::switches::
          ShouldOverrideFetchModelsAndFeaturesTimer()) {
    SetLastModelAndFeaturesFetchAttemptTime(clock_->Now());
    FetchModelsAndHostModelFeatures();
  } else {
    ScheduleModelsAndHostModelFeaturesFetch();
  }
}

base::Time PredictionManager::GetLastFetchAttemptTime() const {
  SEQUENCE_CHECKER(squence_checker_);
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(
          pref_service_->GetInt64(prefs::kModelAndFeaturesLastFetchAttempt)));
}

void PredictionManager::ScheduleModelsAndHostModelFeaturesFetch() {
  DCHECK(!fetch_timer_.IsRunning());
  DCHECK(store_is_ready_);
  const base::TimeDelta time_until_update_time =
      model_and_features_store_->GetHostModelFeaturesUpdateTime() -
      clock_->Now();
  const base::TimeDelta time_until_retry =
      GetLastFetchAttemptTime() + kFetchRetryDelay - clock_->Now();
  base::TimeDelta fetcher_delay =
      std::max(time_until_update_time, time_until_retry);
  if (fetcher_delay <= base::TimeDelta()) {
    SetLastModelAndFeaturesFetchAttemptTime(clock_->Now());
    fetch_timer_.Start(FROM_HERE, RandomFetchDelay(), this,
                       &PredictionManager::FetchModelsAndHostModelFeatures);
    return;
  }
  fetch_timer_.Start(
      FROM_HERE, fetcher_delay, this,
      &PredictionManager::ScheduleModelsAndHostModelFeaturesFetch);
}

void PredictionManager::SetLastModelAndFeaturesFetchAttemptTime(
    base::Time last_attempt_time) {
  SEQUENCE_CHECKER(sequence_checker_);
  pref_service_->SetInt64(
      prefs::kModelAndFeaturesLastFetchAttempt,
      last_attempt_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

void PredictionManager::SetClockForTesting(const base::Clock* clock) {
  clock_ = clock;
}

}  // namespace optimization_guide
