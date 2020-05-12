// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/policies/tab_loading_frame_navigation_policy.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace performance_manager {
namespace policies {

namespace {

bool CanThrottleUrlScheme(const GURL& url) {
  return url.SchemeIs("http") || url.SchemeIs("https");
}

}  // namespace

TabLoadingFrameNavigationPolicy::TabLoadingFrameNavigationPolicy(
    StopThrottlingCallback stop_throttling_callback)
    : stop_throttling_callback_(stop_throttling_callback) {
  DCHECK(!stop_throttling_callback.is_null());
}

TabLoadingFrameNavigationPolicy::~TabLoadingFrameNavigationPolicy() {
  // All timers and timeouts should have been canceled, as no page nodes
  // should be actively tracked.
  DCHECK(timeouts_.empty());
  DCHECK(!timeout_timer_.IsRunning());
  DCHECK_EQ(base::TimeTicks::Min(), scheduled_timer_);
}

// static
bool TabLoadingFrameNavigationPolicy::ShouldThrottleWebContents(
    content::WebContents* contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Don't throttle unless http or https.
  bool throttled = true;
  const GURL& url = contents->GetLastCommittedURL();
  if (!CanThrottleUrlScheme(url))
    throttled = false;

  // Post a notification to the graph. Even if we're not throttling the
  // notification is sent, just in case the WebContents was previously throttled
  // and is being reused for a new navigation. This is racy, as the policy
  // object can be destroyed while this message is in flight. This is resolved
  // on the PM sequence, with the message only being dispatched if the policy
  // object exists in the graph.
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(&SetPageNodeThrottled,
                     PerformanceManager::GetPageNodeForWebContents(contents),
                     throttled));

  return throttled;
}

// static
bool TabLoadingFrameNavigationPolicy::ShouldThrottleNavigation(
    content::NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Don't throttle unless http or https.
  const GURL& url = handle->GetURL();
  if (!CanThrottleUrlScheme(url))
    return false;

  // Never throttle the main frame.
  if (handle->IsInMainFrame())
    return false;

  // Never throttle frames that are navigating to the same eTLD+1 as the main
  // frame.
  auto* contents = handle->GetWebContents();
  if (net::registry_controlled_domains::SameDomainOrHost(
          url, contents->GetLastCommittedURL(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    return false;
  }

  // Throttle any child-frame navigations to a different eTLD+1.
  return true;
}

void TabLoadingFrameNavigationPolicy::OnBeforePageNodeRemoved(
    const PageNode* page_node) {
  // There's no public graph accessor. We could cache this in OnPassedToGraph,
  // but it's reachable this way.
  DCHECK(IsRegistered(page_node->GetGraph()));
  MaybeErasePageTimeout(page_node);
}

void TabLoadingFrameNavigationPolicy::OnFirstContentfulPaint(
    const FrameNode* frame_node,
    base::TimeDelta time_since_navigation_start) {
  // There's no public graph accessor. We could cache this in OnPassedToGraph,
  // but it's reachable this way.
  DCHECK(IsRegistered(frame_node->GetGraph()));

  // We're only interested in current main-frame FCP notifications.
  if (!frame_node->IsMainFrame() || !frame_node->IsCurrent())
    return;

  // Wait for another FCP time period, waiting a minimum of 1 second.
  double fcp_ms = time_since_navigation_start.InMillisecondsF();
  double delta_ms = std::max(1000.0, fcp_ms);
  MaybeUpdatePageTimeout(frame_node->GetPageNode(),
                         base::TimeDelta::FromMillisecondsD(delta_ms));
}

void TabLoadingFrameNavigationPolicy::OnPassedToGraph(Graph* graph) {
  DCHECK(NothingRegistered(graph));
  graph->AddFrameNodeObserver(this);
  graph->AddPageNodeObserver(this);
  graph->RegisterObject(this);
}

void TabLoadingFrameNavigationPolicy::OnTakenFromGraph(Graph* graph) {
  DCHECK(IsRegistered(graph));
  graph->UnregisterObject(this);
  graph->RemovePageNodeObserver(this);
  graph->RemoveFrameNodeObserver(this);
}

// static
void TabLoadingFrameNavigationPolicy::SetPageNodeThrottled(
    base::WeakPtr<const PageNode> page_node,
    bool throttled,
    Graph* graph) {
  auto* self = GetFromGraph(graph);
  if (!self || !page_node)
    return;
  self->SetPageNodeThrottledImpl(page_node.get(), throttled);
}

void TabLoadingFrameNavigationPolicy::SetPageNodeThrottledImpl(
    const PageNode* page_node,
    bool throttled) {
  // There's no public graph accessor. We could cache this in OnPassedToGraph,
  // but it's reachable this way.
  DCHECK(IsRegistered(page_node->GetGraph()));

  // It's possible for WebContents to be reused if a main-frame renavigates.
  // On the UI thread a new scheduler object is created in that case, which will
  // cause a timeout entry to be (temporarily) orphaned here. So first cleanup
  // existing timeouts.
  for (size_t i = 0; i < timeouts_.size(); ++i) {
    if (timeouts_[i].page_node == page_node) {
      timeouts_.erase(i);
      break;
    }
  }

  if (!throttled) {
    MaybeUpdateTimeoutTimer();
    return;
  }

  // Create a brand new timeout for the page.
  // TODO(chrisha): Make this configurable via Finch experiment.
  CreatePageTimeout(page_node, timeout_);
}

void TabLoadingFrameNavigationPolicy::CreatePageTimeout(
    const PageNode* page_node,
    base::TimeDelta timeout) {
#if DCHECK_IS_ON()
  // Sanity check that no timeout entry already exists for this page.
  for (const auto& timeout : timeouts_) {
    DCHECK_NE(timeout.page_node, page_node);
  }
#endif
  base::TimeTicks when = base::TimeTicks::Now() + timeout;
  timeouts_.insert(Timeout{page_node, when});
  MaybeUpdateTimeoutTimer();
}

void TabLoadingFrameNavigationPolicy::MaybeUpdatePageTimeout(
    const PageNode* page_node,
    base::TimeDelta timeout) {
  // Find or create an entry for the given |page_node|.
  size_t i = 0;
  for (; i < timeouts_.size(); ++i) {
    if (timeouts_[i].page_node == page_node)
      break;
  }
  if (i == timeouts_.size())
    return;

  // Update the entry if need be.
  base::TimeTicks when = base::TimeTicks::Now() + timeout;
  if (when < timeouts_[i].timeout) {
    timeouts_.Replace(i, Timeout{page_node, when});
    MaybeUpdateTimeoutTimer();
  }
}

void TabLoadingFrameNavigationPolicy::MaybeErasePageTimeout(
    const PageNode* page_node) {
  for (size_t i = 0; i < timeouts_.size(); ++i) {
    if (timeouts_[i].page_node == page_node) {
      timeouts_.erase(i);
      MaybeUpdateTimeoutTimer();
      return;
    }
  }
}

void TabLoadingFrameNavigationPolicy::MaybeUpdateTimeoutTimer() {
  if (timeouts_.empty()) {
    timeout_timer_.Stop();
    scheduled_timer_ = base::TimeTicks::Min();
    return;
  }

  if (timeout_timer_.IsRunning()) {
    // If the timer is already set to the right time then it doesn't need
    // updating.
    if (scheduled_timer_ == timeouts_.top().timeout)
      return;

    // Otherwise the timer needs rescheduling.
    scheduled_timer_ = base::TimeTicks::Min();
    timeout_timer_.Stop();
  }

  // If the next timeout should already have fired, do so synchronously.
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks when = timeouts_.top().timeout;
  if (when <= now) {
    StopThrottlingExpiredPages();

    // Early return if there are no remaining timeouts.
    if (timeouts_.empty())
      return;
  }

  // Restart the timer for the next event.
  DCHECK(!timeouts_.empty());
  when = timeouts_.top().timeout;
  DCHECK_LT(now, when);
  timeout_timer_.Start(
      FROM_HERE, when - now,
      base::BindOnce(&TabLoadingFrameNavigationPolicy::OnTimeout,
                     base::Unretained(this)));
  scheduled_timer_ = when;
}

void TabLoadingFrameNavigationPolicy::OnTimeout() {
  DCHECK(!timeouts_.empty());
  timeout_timer_.Stop();
  scheduled_timer_ = base::TimeTicks::Min();
  StopThrottlingExpiredPages();
  MaybeUpdateTimeoutTimer();
}

void TabLoadingFrameNavigationPolicy::StopThrottlingExpiredPages() {
  DCHECK(!timeouts_.empty());

  // Send notifications for all expired throttles.
  auto now = base::TimeTicks::Now();
  while (timeouts_.size() && now >= timeouts_.top().timeout) {
    // There's no public graph accessor. We could cache this in OnPassedToGraph,
    // but it's reachable this way.
    const PageNode* page_node = timeouts_.top().page_node;
    DCHECK(IsRegistered(page_node->GetGraph()));
    timeouts_.pop();

    // Post a task to the UI thread to notify the mechanism to stop throttling
    // the contents.
    base::PostTask(
        FROM_HERE,
        {content::BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
        base::BindOnce(
            [](StopThrottlingCallback stop_throttling_callback,
               const WebContentsProxy& proxy) {
              auto* contents = proxy.Get();
              if (contents)
                stop_throttling_callback.Run(contents);
            },
            stop_throttling_callback_, page_node->GetContentsProxy()));
  }
}

}  // namespace policies
}  // namespace performance_manager
