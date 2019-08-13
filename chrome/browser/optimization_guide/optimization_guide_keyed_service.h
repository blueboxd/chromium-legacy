// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_

#include <memory>
#include <unordered_set>
#include <vector>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/proto/hints.pb.h"

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
class OptimizationGuideService;
class TopHostProvider;
}  // namespace optimization_guide

class OptimizationGuideHintsManager;

class OptimizationGuideKeyedService : public KeyedService {
 public:
  explicit OptimizationGuideKeyedService(
      content::BrowserContext* browser_context);
  ~OptimizationGuideKeyedService() override;

  // Initializes the service. |optimization_guide_service| is the
  // Optimization Guide Service that is being listened to and is guaranteed to
  // outlive |this|. |profile_path| is the path to user data on disk.
  void Initialize(
      optimization_guide::OptimizationGuideService* optimization_guide_service,
      leveldb_proto::ProtoDatabaseProvider* database_provider,
      const base::FilePath& profile_path);

  OptimizationGuideHintsManager* GetHintsManager() {
    return hints_manager_.get();
  }

  optimization_guide::TopHostProvider* GetTopHostProvider() {
    return top_host_provider_.get();
  }

  // Registers the optimization types that intend to be queried during the
  // session.
  void RegisterOptimizationTypes(
      std::vector<optimization_guide::proto::OptimizationType>
          optimization_types);

  // Prompts the load of the hint for the navigation, if there is at least one
  // optimization type registered and there is a hint available.
  void MaybeLoadHintForNavigation(content::NavigationHandle* navigation_handle);

  // KeyedService implementation.
  void Shutdown() override;

 private:
  content::BrowserContext* browser_context_;

  // Manages the storing, loading, and fetching of hints.
  std::unique_ptr<OptimizationGuideHintsManager> hints_manager_;

  // The top host provider to use for fetching information for the user's top
  // hosts. Will be null if the user has not consented to this type of browser
  // behavior.
  std::unique_ptr<optimization_guide::TopHostProvider> top_host_provider_;

  DISALLOW_COPY_AND_ASSIGN(OptimizationGuideKeyedService);
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_KEYED_SERVICE_H_
