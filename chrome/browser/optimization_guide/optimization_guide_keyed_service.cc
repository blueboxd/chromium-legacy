// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "chrome/browser/data_saver/data_saver_top_host_provider.h"
#include "chrome/browser/optimization_guide/optimization_guide_hints_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/optimization_guide/command_line_top_host_provider.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/top_host_provider.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"

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
  hints_manager_ = std::make_unique<OptimizationGuideHintsManager>(
      optimization_guide_service, profile_path,
      Profile::FromBrowserContext(browser_context_)->GetPrefs(),
      database_provider, top_host_provider_.get());
}

void OptimizationGuideKeyedService::RegisterOptimizationTypes(
    std::vector<optimization_guide::proto::OptimizationType>
        optimization_types) {
  DCHECK(hints_manager_);

  hints_manager_->RegisterOptimizationTypes(optimization_types);
}

void OptimizationGuideKeyedService::MaybeLoadHintForNavigation(
    content::NavigationHandle* navigation_handle) {
  if (hints_manager_ && hints_manager_->HasRegisteredOptimizationTypes())
    hints_manager_->LoadHintForNavigation(navigation_handle, base::DoNothing());
}

void OptimizationGuideKeyedService::Shutdown() {
  if (hints_manager_) {
    hints_manager_->Shutdown();
    hints_manager_ = nullptr;
  }
}
