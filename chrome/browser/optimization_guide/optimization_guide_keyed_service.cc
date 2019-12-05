// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "chrome/browser/optimization_guide/optimization_guide_hints_manager.h"
#include "chrome/browser/optimization_guide/optimization_guide_navigation_data.h"
#include "chrome/browser/optimization_guide/optimization_guide_session_statistic.h"
#include "chrome/browser/optimization_guide/optimization_guide_top_host_provider.h"
#include "chrome/browser/optimization_guide/prediction/prediction_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/optimization_guide/command_line_top_host_provider.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/top_host_provider.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"

namespace {

// Returns the top host provider to be used with this keyed service. Can return
// nullptr if the user or browser is not permitted to call the remote
// Optimization Guide Service.
std::unique_ptr<optimization_guide::TopHostProvider>
GetTopHostProviderIfUserPermitted(content::BrowserContext* browser_context) {
  // First check whether the command-line flag should be used.
  std::unique_ptr<optimization_guide::TopHostProvider> top_host_provider =
      optimization_guide::CommandLineTopHostProvider::CreateIfEnabled();
  if (top_host_provider)
    return top_host_provider;

  // If not enabled by flag, see if the user is a Data Saver user and has seen
  // all the right prompts for it.
  return OptimizationGuideTopHostProvider::CreateIfAllowed(browser_context);
}

// Logs |optimization_target_decision| for |optimization_target| in the current
// navigation's OptimizationGuideNavigationData.
void LogOptimizationTargetDecision(
    content::NavigationHandle* navigation_handle,
    optimization_guide::proto::OptimizationTarget optimization_target,
    optimization_guide::OptimizationTargetDecision
        optimization_target_decision) {
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle);
  if (navigation_data) {
    navigation_data->SetDecisionForOptimizationTarget(
        optimization_target, optimization_target_decision);
  }
}

// Logs the |optimization_type_decision| for |optimization_type| in the current
// navigation's OptimizationGuideNavigationData.
void LogOptimizationTypeDecision(
    content::NavigationHandle* navigation_handle,
    optimization_guide::proto::OptimizationType optimization_type,
    optimization_guide::OptimizationTypeDecision optimization_type_decision) {
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle);
  if (navigation_data) {
    navigation_data->SetDecisionForOptimizationType(optimization_type,
                                                    optimization_type_decision);
  }
}

// Returns the OptimizationGuideDecision from |optimization_target_decision|.
optimization_guide::OptimizationGuideDecision
GetOptimizationGuideDecisionFromOptimizationTargetDecision(
    optimization_guide::OptimizationTargetDecision
        optimization_target_decision) {
  switch (optimization_target_decision) {
    case optimization_guide::OptimizationTargetDecision::kPageLoadDoesNotMatch:
    case optimization_guide::OptimizationTargetDecision::
        kModelPredictionHoldback:
      return optimization_guide::OptimizationGuideDecision::kFalse;
    case optimization_guide::OptimizationTargetDecision::kPageLoadMatches:
      return optimization_guide::OptimizationGuideDecision::kTrue;
    case optimization_guide::OptimizationTargetDecision::
        kModelNotAvailableOnClient:
    case optimization_guide::OptimizationTargetDecision::kUnknown:
    case optimization_guide::OptimizationTargetDecision::kDeciderNotInitialized:
      return optimization_guide::OptimizationGuideDecision::kUnknown;
  }
}

// Returns the OptimizationGuideDecision from |optimization_type_decision|.
optimization_guide::OptimizationGuideDecision
GetOptimizationGuideDecisionFromOptimizationTypeDecision(
    optimization_guide::OptimizationTypeDecision optimization_type_decision) {
  switch (optimization_type_decision) {
    case optimization_guide::OptimizationTypeDecision::
        kAllowedByOptimizationFilter:
    case optimization_guide::OptimizationTypeDecision::kAllowedByHint:
      return optimization_guide::OptimizationGuideDecision::kTrue;
    case optimization_guide::OptimizationTypeDecision::kUnknown:
    case optimization_guide::OptimizationTypeDecision::
        kHadOptimizationFilterButNotLoadedInTime:
    case optimization_guide::OptimizationTypeDecision::
        kHadHintButNotLoadedInTime:
    case optimization_guide::OptimizationTypeDecision::
        kHintFetchStartedButNotAvailableInTime:
    case optimization_guide::OptimizationTypeDecision::kDeciderNotInitialized:
      return optimization_guide::OptimizationGuideDecision::kUnknown;
    case optimization_guide::OptimizationTypeDecision::kNotAllowedByHint:
    case optimization_guide::OptimizationTypeDecision::kNoMatchingPageHint:
    case optimization_guide::OptimizationTypeDecision::kNoHintAvailable:
    case optimization_guide::OptimizationTypeDecision::
        kNotAllowedByOptimizationFilter:
      return optimization_guide::OptimizationGuideDecision::kFalse;
  }
}

}  // namespace

OptimizationGuideKeyedService::OptimizationGuideKeyedService(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!browser_context_->IsOffTheRecord());
}

OptimizationGuideKeyedService::~OptimizationGuideKeyedService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void OptimizationGuideKeyedService::Initialize(
    optimization_guide::OptimizationGuideService* optimization_guide_service,
    leveldb_proto::ProtoDatabaseProvider* database_provider,
    const base::FilePath& profile_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(optimization_guide_service);

  Profile* profile = Profile::FromBrowserContext(browser_context_);
  top_host_provider_ = GetTopHostProviderIfUserPermitted(browser_context_);
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess();
  hints_manager_ = std::make_unique<OptimizationGuideHintsManager>(
      pre_initialized_optimization_types_, optimization_guide_service, profile,
      profile_path, profile->GetPrefs(), database_provider,
      top_host_provider_.get(), url_loader_factory);
  if (optimization_guide::features::IsOptimizationTargetPredictionEnabled() &&
      optimization_guide::features::IsRemoteFetchingEnabled()) {
    prediction_manager_ =
        std::make_unique<optimization_guide::PredictionManager>(
            pre_initialized_optimization_targets_, profile_path,
            database_provider, top_host_provider_.get(), url_loader_factory,
            profile->GetPrefs(), profile);
  }
}

void OptimizationGuideKeyedService::MaybeLoadHintForNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (hints_manager_ && hints_manager_->HasRegisteredOptimizationTypes()) {
    hints_manager_->OnNavigationStartOrRedirect(navigation_handle,
                                                base::DoNothing());
  }
}

void OptimizationGuideKeyedService::RegisterOptimizationTypesAndTargets(
    const std::vector<optimization_guide::proto::OptimizationType>&
        optimization_types,
    const std::vector<optimization_guide::proto::OptimizationTarget>&
        optimization_targets) {
  // If the service has not been initialized yet, keep track of the optimization
  // types and targets that are registered, so that we can pass them to the
  // appropriate managers at initialization.
  if (!hints_manager_) {
    pre_initialized_optimization_types_.insert(
        pre_initialized_optimization_types_.begin(), optimization_types.begin(),
        optimization_types.end());

    pre_initialized_optimization_targets_.insert(
        pre_initialized_optimization_targets_.begin(),
        optimization_targets.begin(), optimization_targets.end());
    return;
  }

  hints_manager_->RegisterOptimizationTypes(optimization_types);

  if (prediction_manager_)
    prediction_manager_->RegisterOptimizationTargets(optimization_targets);
}

optimization_guide::OptimizationGuideDecision
OptimizationGuideKeyedService::ShouldTargetNavigation(
    content::NavigationHandle* navigation_handle,
    optimization_guide::proto::OptimizationTarget optimization_target) {
  if (!hints_manager_) {
    // We are not initialized yet, just return unknown.
    LogOptimizationTargetDecision(
        navigation_handle, optimization_target,
        optimization_guide::OptimizationTargetDecision::kDeciderNotInitialized);
    return optimization_guide::OptimizationGuideDecision::kUnknown;
  }

  optimization_guide::OptimizationTargetDecision optimization_target_decision;
  if (prediction_manager_) {
    optimization_target_decision = prediction_manager_->ShouldTargetNavigation(
        navigation_handle, optimization_target);
  } else {
    DCHECK(hints_manager_);
    optimization_guide::OptimizationTypeDecision
        unused_optimization_type_decision;
    hints_manager_->CanApplyOptimization(
        navigation_handle, optimization_target,
        optimization_guide::proto::OPTIMIZATION_NONE,
        &optimization_target_decision, &unused_optimization_type_decision,
        /*optimization_metadata=*/nullptr);
  }

  LogOptimizationTargetDecision(navigation_handle, optimization_target,
                                optimization_target_decision);
  return GetOptimizationGuideDecisionFromOptimizationTargetDecision(
      optimization_target_decision);
}

optimization_guide::OptimizationGuideDecision
OptimizationGuideKeyedService::CanApplyOptimization(
    content::NavigationHandle* navigation_handle,
    optimization_guide::proto::OptimizationType optimization_type,
    optimization_guide::OptimizationMetadata* optimization_metadata) {
  DCHECK(hints_manager_);

  optimization_guide::OptimizationTargetDecision
      unused_optimization_target_decision;
  optimization_guide::OptimizationTypeDecision optimization_type_decision;
  hints_manager_->CanApplyOptimization(
      navigation_handle, optimization_guide::proto::OPTIMIZATION_TARGET_UNKNOWN,
      optimization_type, &unused_optimization_target_decision,
      &optimization_type_decision, optimization_metadata);

  LogOptimizationTypeDecision(navigation_handle, optimization_type,
                              optimization_type_decision);
  return GetOptimizationGuideDecisionFromOptimizationTypeDecision(
      optimization_type_decision);
}

void OptimizationGuideKeyedService::ClearData() {
  if (hints_manager_)
    hints_manager_->ClearFetchedHints();
  if (prediction_manager_)
    prediction_manager_->ClearHostModelFeatures();
}

void OptimizationGuideKeyedService::Shutdown() {
  if (hints_manager_) {
    hints_manager_->Shutdown();
    hints_manager_ = nullptr;
  }
}

void OptimizationGuideKeyedService::UpdateSessionFCP(base::TimeDelta fcp) {
  if (prediction_manager_)
    prediction_manager_->UpdateFCPSessionStatistics(fcp);
}
