// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager_resource_coordinator_signal_observer.h"

#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/resource_coordinator/resource_coordinator_parts.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/resource_coordinator/tab_manager_stats_collector.h"
#include "chrome/browser/resource_coordinator/tab_manager_web_contents_data.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace resource_coordinator {

// A helper class for accessing TabLoadTracker. TabLoadTracker can't directly
// friend TabManager::ResourceCoordinatorSignalObserver as it's a nested class
// and can't be forward declared.
class TabManagerResourceCoordinatorSignalObserverHelper {
 public:
  static void OnPageAlmostIdle(content::WebContents* web_contents) {
    // This object is create on demand, so always exists.
    TabLoadTracker::Get()->OnPageAlmostIdle(web_contents);
  }
};

TabManager::ResourceCoordinatorSignalObserver::
    ResourceCoordinatorSignalObserver(
        const base::WeakPtr<TabManager>& tab_manager)
    : tab_manager_(tab_manager) {}

TabManager::ResourceCoordinatorSignalObserver::
    ~ResourceCoordinatorSignalObserver() = default;

bool TabManager::ResourceCoordinatorSignalObserver::ShouldObserve(
    const NodeBase* node) {
  return node->type() == performance_manager::PageNodeImpl::Type() ||
         node->type() == performance_manager::ProcessNodeImpl::Type();
}

void TabManager::ResourceCoordinatorSignalObserver::OnPageAlmostIdleChanged(
    PageNodeImpl* page_node) {
  // Only notify of changes to almost idle.
  if (!page_node->page_almost_idle())
    return;
  // Forward the notification over to the UI thread.
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&OnPageAlmostIdleOnUi, tab_manager_,
                     page_node->contents_proxy(), page_node->navigation_id()));
}

void TabManager::ResourceCoordinatorSignalObserver::
    OnExpectedTaskQueueingDurationSample(ProcessNodeImpl* process_node) {
  // Report this measurement to all pages that are hosting a main frame in
  // the process that was sampled.
  const base::TimeDelta& duration =
      process_node->expected_task_queueing_duration();
  for (auto* frame_node : process_node->frame_nodes()) {
    if (!frame_node->IsMainFrame())
      continue;
    auto* page_node = frame_node->page_node();

    // Forward the notification over to the UI thread.
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&OnExpectedTaskQueueingDurationSampleOnUi, tab_manager_,
                       page_node->contents_proxy(), page_node->navigation_id(),
                       duration));
  }
}

// static
content::WebContents*
TabManager::ResourceCoordinatorSignalObserver::GetContentsForDispatch(
    const base::WeakPtr<TabManager>& tab_manager,
    const WebContentsProxy& contents_proxy,
    int64_t navigation_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!tab_manager.get() || !contents_proxy.Get() ||
      contents_proxy.LastNavigationId() != navigation_id) {
    return nullptr;
  }
  return contents_proxy.Get();
}

// static
void TabManager::ResourceCoordinatorSignalObserver::OnPageAlmostIdleOnUi(
    const base::WeakPtr<TabManager>& tab_manager,
    const WebContentsProxy& contents_proxy,
    int64_t navigation_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (auto* contents =
          GetContentsForDispatch(tab_manager, contents_proxy, navigation_id)) {
    TabManagerResourceCoordinatorSignalObserverHelper::OnPageAlmostIdle(
        contents);
  }
}

// static
void TabManager::ResourceCoordinatorSignalObserver::
    OnExpectedTaskQueueingDurationSampleOnUi(
        const base::WeakPtr<TabManager>& tab_manager,
        const WebContentsProxy& contents_proxy,
        int64_t navigation_id,
        base::TimeDelta duration) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (auto* contents =
          GetContentsForDispatch(tab_manager, contents_proxy, navigation_id)) {
    // This object is create on demand, so always exists.
    g_browser_process->GetTabManager()
        ->stats_collector()
        ->RecordExpectedTaskQueueingDuration(contents, duration);
  }
}

}  // namespace resource_coordinator
