// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_facade.h"

#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_factory.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_cache_impl.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

namespace performance_manager {

class GraphImpl;

SiteDataCacheFacade::SiteDataCacheFacade(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserContext* parent_context = nullptr;
  if (browser_context->IsOffTheRecord()) {
    parent_context =
        chrome::GetBrowserContextRedirectedInIncognito(browser_context);
  }
  SiteDataCacheFactory::OnBrowserContextCreatedOnUIThread(
      SiteDataCacheFactory::GetInstance(), browser_context_, parent_context);

  history::HistoryService* history =
      HistoryServiceFactory::GetForProfileWithoutCreating(
          Profile::FromBrowserContext(browser_context_));
  if (history)
    history_observer_.Add(history);
}

SiteDataCacheFacade::~SiteDataCacheFacade() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SiteDataCacheFactory::OnBrowserContextDestroyedOnUIThread(
      SiteDataCacheFactory::GetInstance(), browser_context_);
}

void SiteDataCacheFacade::IsDataCacheRecordingForTesting(
    base::OnceCallback<void(bool)> cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SiteDataCacheFactory::GetInstance()->IsDataCacheRecordingForTesting(
      browser_context_->UniqueId(), std::move(cb));
}

void SiteDataCacheFacade::WaitUntilCacheInitializedForTesting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::RunLoop run_loop;
  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindOnce(
                     [](base::OnceClosure quit_closure,
                        const std::string& browser_context_id,
                        Graph* graph_unused) {
                       auto* cache = SiteDataCacheFactory::GetInstance()
                                         ->GetDataCacheForBrowserContext(
                                             browser_context_id);
                       if (cache->IsRecordingForTesting()) {
                         static_cast<SiteDataCacheImpl*>(cache)
                             ->SetInitializationCallbackForTesting(
                                 std::move(quit_closure));
                       }
                     },
                     run_loop.QuitClosure(), browser_context_->UniqueId()));
  run_loop.Run();
}

void SiteDataCacheFacade::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  if (deletion_info.IsAllHistory()) {
    auto clear_all_site_data_cb = base::BindOnce(
        [](const std::string& browser_context_id, Graph* graph_unused) {
          auto* cache = SiteDataCacheFactory::GetInstance()
                            ->GetDataCacheForBrowserContext(browser_context_id);
          static_cast<SiteDataCacheImpl*>(cache)->ClearAllSiteData();
        },
        browser_context_->UniqueId());
    PerformanceManager::CallOnGraph(FROM_HERE,
                                    std::move(clear_all_site_data_cb));
  } else {
    std::vector<url::Origin> origins_to_remove;

    for (const auto& it : deletion_info.deleted_urls_origin_map()) {
      const url::Origin origin = url::Origin::Create(it.first);
      const int remaining_visits_in_history = it.second.first;

      // If the origin is no longer exists in history, clear the site data.
      DCHECK_GE(remaining_visits_in_history, 0);
      if (remaining_visits_in_history == 0)
        origins_to_remove.emplace_back(origin);
    }

    if (origins_to_remove.empty())
      return;

    auto clear_site_data_cb = base::BindOnce(
        [](const std::string& browser_context_id,
           const std::vector<url::Origin>& origins_to_remove,
           Graph* graph_unused) {
          auto* cache = SiteDataCacheFactory::GetInstance()
                            ->GetDataCacheForBrowserContext(browser_context_id);
          static_cast<SiteDataCacheImpl*>(cache)->ClearSiteDataForOrigins(
              origins_to_remove);
        },
        browser_context_->UniqueId(), std::move(origins_to_remove));
    PerformanceManager::CallOnGraph(FROM_HERE, std::move(clear_site_data_cb));
  }
}

void SiteDataCacheFacade::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  history_observer_.Remove(history_service);
}

}  // namespace performance_manager
