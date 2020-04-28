// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_

#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/optimization_guide_decider.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace base {
class FilePath;
}  // namespace base

namespace content {
class BrowserContext;
class NavigationHandle;
}  // namespace content

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace optimization_guide {
namespace android {
class OptimizationGuideBridge;
}  // namespace android
class OptimizationGuideService;
class TopHostProvider;
class PredictionManager;
class PredictionManagerBrowserTest;
}  // namespace optimization_guide

class GURL;
class OptimizationGuideHintsManager;
class OptimizationGuideNavigationData;

class OptimizationGuideKeyedService
    : public KeyedService,
      public optimization_guide::OptimizationGuideDecider {
 public:
  explicit OptimizationGuideKeyedService(
      content::BrowserContext* browser_context);
  ~OptimizationGuideKeyedService() override;

  // optimization_guide::OptimizationGuideDecider implementation:
  void RegisterOptimizationTypesAndTargets(
      const std::vector<optimization_guide::proto::OptimizationType>&
          optimization_types,
      const std::vector<optimization_guide::proto::OptimizationTarget>&
          optimization_targets) override;
  optimization_guide::OptimizationGuideDecision ShouldTargetNavigation(
      content::NavigationHandle* navigation_handle,
      optimization_guide::proto::OptimizationTarget optimization_target)
      override;
  optimization_guide::OptimizationGuideDecision CanApplyOptimization(
      content::NavigationHandle* navigation_handle,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationMetadata* optimization_metadata) override;
  void CanApplyOptimizationAsync(
      content::NavigationHandle* navigation_handle,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationGuideDecisionCallback callback) override;

  // Adds hints for a URL with provided metadata to the optimization guide.
  // For testing purposes only. This will flush any callbacks for |url| that
  // were registered via |CanApplyOptimizationAsync|. If no applicable callbacks
  // were registered, this will just add the hint for later use.
  void AddHintForTesting(
      const GURL& url,
      optimization_guide::proto::OptimizationType optimization_type,
      const base::Optional<optimization_guide::OptimizationMetadata>& metadata);

 private:
  friend class ChromeBrowsingDataRemoverDelegate;
  friend class HintsFetcherBrowserTest;
  friend class OptimizationGuideKeyedServiceBrowserTest;
  friend class OptimizationGuideWebContentsObserver;
  friend class ProfileManager;
  friend class optimization_guide::PredictionManagerBrowserTest;
  friend class optimization_guide::android::OptimizationGuideBridge;

  // Initializes the service. |optimization_guide_service| is the
  // Optimization Guide Service that is being listened to and is guaranteed to
  // outlive |this|. |profile_path| is the path to user data on disk.
  void Initialize(
      optimization_guide::OptimizationGuideService* optimization_guide_service,
      leveldb_proto::ProtoDatabaseProvider* database_provider,
      const base::FilePath& profile_path);

  // Virtualized for testing.
  virtual OptimizationGuideHintsManager* GetHintsManager();

  optimization_guide::TopHostProvider* GetTopHostProvider() {
    return top_host_provider_.get();
  }

  optimization_guide::PredictionManager* GetPredictionManager() {
    return prediction_manager_.get();
  }

  // Notifies |hints_manager_| that the navigation associated with
  // |navigation_handle| has started or redirected.
  void OnNavigationStartOrRedirect(
      content::NavigationHandle* navigation_handle);

  // Notifies |hints_manager_| that the navigation associated with
  // |navigation_redirect_chain| has finished.
  void OnNavigationFinish(const std::vector<GURL>& navigation_redirect_chain,
                          OptimizationGuideNavigationData* navigation_data);

  // Clears data specific to the user.
  void ClearData();

  // Updates |prediction_manager_| with the provided fcp value.
  void UpdateSessionFCP(base::TimeDelta fcp);

  // KeyedService implementation:
  void Shutdown() override;

  content::BrowserContext* browser_context_;

  // The optimization types registered prior to initialization.
  std::vector<optimization_guide::proto::OptimizationType>
      pre_initialized_optimization_types_;

  // The optimization targets registered prior to initialization.
  std::vector<optimization_guide::proto::OptimizationTarget>
      pre_initialized_optimization_targets_;

  // Manages the storing, loading, and fetching of hints.
  std::unique_ptr<OptimizationGuideHintsManager> hints_manager_;

  // Manages the storing, loading, and evaluating of optimization target
  // prediction models.
  std::unique_ptr<optimization_guide::PredictionManager> prediction_manager_;

  // The top host provider to use for fetching information for the user's top
  // hosts. Will be null if the user has not consented to this type of browser
  // behavior.
  std::unique_ptr<optimization_guide::TopHostProvider> top_host_provider_;

  DISALLOW_COPY_AND_ASSIGN(OptimizationGuideKeyedService);
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_
