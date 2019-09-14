// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "chrome/browser/data_saver/data_saver_top_host_provider.h"
#include "chrome/browser/optimization_guide/optimization_guide_hints_manager.h"
#include "chrome/browser/optimization_guide/optimization_guide_navigation_data.h"
#include "chrome/browser/optimization_guide/optimization_guide_session_statistic.h"
#include "chrome/browser/optimization_guide/optimization_guide_web_contents_observer.h"
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
  return DataSaverTopHostProvider::CreateIfAllowed(browser_context);
}

// Logs |optimization_target_decision| for |optimization_target| and the
// |optimization_type_decision| for |optimization_type| in the current
// navigation's OptimizationGuideNavigationData;
void LogDecisions(
    content::NavigationHandle* navigation_handle,
    optimization_guide::OptimizationTarget optimization_target,
    optimization_guide::OptimizationTargetDecision optimization_target_decision,
    optimization_guide::proto::OptimizationType optimization_type,
    optimization_guide::OptimizationTypeDecision optimization_type_decision) {
  OptimizationGuideWebContentsObserver*
      optimization_guide_web_contents_observer =
          OptimizationGuideWebContentsObserver::FromWebContents(
              navigation_handle->GetWebContents());
  if (!optimization_guide_web_contents_observer)
    return;

  OptimizationGuideNavigationData* navigation_data =
      optimization_guide_web_contents_observer
          ->GetOrCreateOptimizationGuideNavigationData(navigation_handle);
  navigation_data->SetDecisionForOptimizationTarget(
      optimization_target, optimization_target_decision);
  navigation_data->SetDecisionForOptimizationType(optimization_type,
                                                  optimization_type_decision);
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
      return optimization_guide::OptimizationGuideDecision::kUnknown;
    default:
      return optimization_guide::OptimizationGuideDecision::kFalse;
  }
  static_assert(
      optimization_guide::OptimizationTypeDecision::kMaxValue ==
          optimization_guide::OptimizationTypeDecision::kNoHintAvailable,
      "This function should be updated when a new OptimizationTypeDecision is "
      "added");
}

// Returns the OptimizationGuideDecision based on |optimization_target_decision|
// and |optimization_guide_decision|. If either resolves to false,
// then the decision will be false. Otherwise, resolves to true or unknown.
optimization_guide::OptimizationGuideDecision ResolveOptimizationGuideDecision(
    optimization_guide::OptimizationTargetDecision optimization_target_decision,
    optimization_guide::OptimizationTypeDecision optimization_type_decision) {
  switch (optimization_target_decision) {
    case optimization_guide::OptimizationTargetDecision::kPageLoadDoesNotMatch:
      return optimization_guide::OptimizationGuideDecision::kFalse;
    case optimization_guide::OptimizationTargetDecision::kPageLoadMatches:
      return GetOptimizationGuideDecisionFromOptimizationTypeDecision(
          optimization_type_decision);
    default:
      return optimization_guide::OptimizationGuideDecision::kUnknown;
  }
  static_assert(
      optimization_guide::OptimizationTargetDecision::kMaxValue ==
          optimization_guide::OptimizationTargetDecision::kPageLoadMatches,
      "This function should be updated when a new OptimizationTargetDecision "
      "is added");
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

  top_host_provider_ = GetTopHostProviderIfUserPermitted(browser_context_);
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  hints_manager_ = std::make_unique<OptimizationGuideHintsManager>(
      optimization_guide_service, profile_path, profile->GetPrefs(),
      database_provider, top_host_provider_.get(),
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess());
}

void OptimizationGuideKeyedService::MaybeLoadHintForNavigation(
    content::NavigationHandle* navigation_handle) {
  if (hints_manager_ && hints_manager_->HasRegisteredOptimizationTypes())
    hints_manager_->LoadHintForNavigation(navigation_handle, base::DoNothing());
}

void OptimizationGuideKeyedService::RegisterOptimizationTypes(
    std::vector<optimization_guide::proto::OptimizationType>
        optimization_types) {
  DCHECK(hints_manager_);

  hints_manager_->RegisterOptimizationTypes(optimization_types);
}

optimization_guide::OptimizationGuideDecision
OptimizationGuideKeyedService::CanApplyOptimization(
    content::NavigationHandle* navigation_handle,
    optimization_guide::OptimizationTarget optimization_target,
    optimization_guide::proto::OptimizationType optimization_type,
    optimization_guide::OptimizationMetadata* optimization_metadata) {
  DCHECK(hints_manager_);

  optimization_guide::OptimizationTargetDecision optimization_target_decision;
  optimization_guide::OptimizationTypeDecision optimization_type_decision;
  hints_manager_->CanApplyOptimization(
      navigation_handle, optimization_target, optimization_type,
      &optimization_target_decision, &optimization_type_decision,
      optimization_metadata);

  LogDecisions(navigation_handle, optimization_target,
               optimization_target_decision, optimization_type,
               optimization_type_decision);
  return ResolveOptimizationGuideDecision(optimization_target_decision,
                                          optimization_type_decision);
}

void OptimizationGuideKeyedService::ClearData() {
  if (hints_manager_)
    hints_manager_->ClearFetchedHints();
}

void OptimizationGuideKeyedService::Shutdown() {
  if (hints_manager_) {
    hints_manager_->Shutdown();
    hints_manager_ = nullptr;
  }
}

void OptimizationGuideKeyedService::UpdateSessionFCP(base::TimeDelta fcp) {
  // TODO(crbug/1001194): This will be passed to the
  // OptimizationGuidePredictionManager that will own the FCP session
  // statistics.
  if (!session_fcp_)
    session_fcp_ = std::make_unique<OptimizationGuideSessionStatistic>();
  session_fcp_->AddSample(static_cast<float>(fcp.InMilliseconds()));
}
