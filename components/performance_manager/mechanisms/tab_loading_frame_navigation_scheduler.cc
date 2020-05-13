// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/mechanisms/tab_loading_frame_navigation_scheduler.h"

#include "base/no_destructor.h"
#include "components/performance_manager/public/graph/policies/tab_loading_frame_navigation_policy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager {
namespace mechanisms {

namespace {

using PolicyDelegate = TabLoadingFrameNavigationScheduler::PolicyDelegate;

// The default policy delegate that delegates directly to the
// TabLoadingFrameNavigationPolicy class.
class DefaultPolicyDelegate : public PolicyDelegate {
 public:
  using PolicyClass = policies::TabLoadingFrameNavigationPolicy;

  DefaultPolicyDelegate() = default;
  DefaultPolicyDelegate(const DefaultPolicyDelegate&) = delete;
  DefaultPolicyDelegate& operator=(const DefaultPolicyDelegate&) = delete;
  ~DefaultPolicyDelegate() override = default;

  // PolicyDelegate implementation:
  bool ShouldThrottleWebContents(content::WebContents* contents) override {
    return PolicyClass::ShouldThrottleWebContents(contents);
  }
  bool ShouldThrottleNavigation(content::NavigationHandle* handle) override {
    return PolicyClass::ShouldThrottleNavigation(handle);
  }

  static DefaultPolicyDelegate* Instance() {
    static base::NoDestructor<DefaultPolicyDelegate> default_policy_delegate;
    return default_policy_delegate.get();
  }
};

PolicyDelegate* g_policy_delegate = nullptr;
bool g_throttling_enabled = false;
TabLoadingFrameNavigationScheduler* g_root = nullptr;

PolicyDelegate* GetPolicyDelegate() {
  if (!g_policy_delegate)
    g_policy_delegate = DefaultPolicyDelegate::Instance();
  return g_policy_delegate;
}

}  // namespace

// A very simple throttle that always defers until Resume is called.
class TabLoadingFrameNavigationScheduler::Throttle
    : public content::NavigationThrottle {
 public:
  explicit Throttle(content::NavigationHandle* handle)
      : content::NavigationThrottle(handle) {}
  ~Throttle() override = default;

  // content::NavigationThrottle implementation
  const char* GetNameForLogging() override {
    static constexpr char kName[] =
        "TabLoadingFrameNavigationScheduler::Throttle";
    return kName;
  }
  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override {
    return content::NavigationThrottle::DEFER;
  }

  // Make this public so the scheduler can invoke it.
  using content::NavigationThrottle::Resume;
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabLoadingFrameNavigationScheduler)

TabLoadingFrameNavigationScheduler::~TabLoadingFrameNavigationScheduler() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(throttles_.empty());

  // Unlink ourselves from the linked list.
  if (g_root == this) {
    DCHECK_EQ(nullptr, prev_);
    g_root = next_;
  }
  if (prev_) {
    DCHECK_EQ(this, prev_->next_);
    prev_->next_ = next_;
    // Do not null |prev_| so we can access it below if needed.
  }
  if (next_) {
    DCHECK_EQ(this, next_->prev_);
    next_->prev_ = prev_;
    next_ = nullptr;
  }
  prev_ = nullptr;
}

// static
std::unique_ptr<content::NavigationThrottle>
TabLoadingFrameNavigationScheduler::MaybeCreateThrottleForNavigation(
    content::NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<content::NavigationThrottle> empty_throttle;

  if (!g_throttling_enabled)
    return empty_throttle;

  // Get the contents, and the associated scheduler if it exists.
  auto* contents = handle->GetWebContents();
  auto* scheduler = FromWebContents(contents);

  // If this is a non-main frame and no scheduler exists, then the decision was
  // already made to *not* throttle this contents, so we can return early.
  if (!handle->IsInMainFrame() && !scheduler) {
    return empty_throttle;
  }

  // If a scheduler exists and this is a main frame navigation then its a
  // renavigation and the contents is being reused. In this case we need to
  // tear down the existing scheduler, and potentially create a new one.
  if (handle->IsInMainFrame() && scheduler) {
    DCHECK_NE(handle->GetNavigationId(), scheduler->navigation_id_);
    scheduler->StopThrottlingImpl();  // Causes |scheduler| to delete itself.
    scheduler = FromWebContents(contents);
    DCHECK_EQ(nullptr, scheduler);
  }

  // If there's no scheduler for this contents, check the policy object to see
  // if one should be created.
  if (!scheduler) {
    DCHECK(handle->IsInMainFrame());
    if (!GetPolicyDelegate()->ShouldThrottleWebContents(contents)) {
      return empty_throttle;
    }
    CreateForWebContents(contents);
    scheduler = FromWebContents(contents);
    DCHECK(scheduler);
    scheduler->navigation_id_ = handle->GetNavigationId();
    // The main frame should never be throttled, so we can return early.
    return empty_throttle;
  }

  // At this point we have a scheduler, and the navigation is for a child
  // frame. Determine whether the child frame should be throttled.
  if (!GetPolicyDelegate()->ShouldThrottleNavigation(handle)) {
    return empty_throttle;
  }

  // Getting here indicates that the navigation is to be throttled. Create a
  // throttle and remember it.
  std::unique_ptr<Throttle> throttle(new Throttle(handle));
  auto result =
      scheduler->throttles_.insert(std::make_pair(handle, throttle.get()));
  DCHECK(result.second);
  return throttle;
}

// static
void TabLoadingFrameNavigationScheduler::SetThrottlingEnabled(bool enabled) {
  if (enabled == g_throttling_enabled)
    return;
  g_throttling_enabled = enabled;
  if (enabled)
    return;

  // At this point the throttling is being disabled. Stop throttling all
  // currently-throttled contents.
  while (g_root)
    g_root->StopThrottlingImpl();  // Causes |g_root| to delete itself.
}

// static
void TabLoadingFrameNavigationScheduler::StopThrottling(
    content::WebContents* contents,
    int64_t last_navigation_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // This should never be called for a |contents| without an associated
  // scheduler, as besides contents re-use, all schedulers eventually receive
  // a StopThrottling notification.
  auto* scheduler = FromWebContents(contents);
  // There is a race between renavigations and policy messages. Only dispatch
  // this if its intended for the appropriate navigation ID.
  if (scheduler->navigation_id_ != last_navigation_id)
    return;
  scheduler->StopThrottlingImpl();
}

// static
void TabLoadingFrameNavigationScheduler::SetPolicyDelegateForTesting(
    PolicyDelegate* policy_delegate) {
  g_policy_delegate = policy_delegate;
}

// static
bool TabLoadingFrameNavigationScheduler::IsThrottlingEnabledForTesting() {
  return g_throttling_enabled;
}

TabLoadingFrameNavigationScheduler::TabLoadingFrameNavigationScheduler(
    content::WebContents* contents)
    : content::WebContentsObserver(contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Link ourselves into the linked list.
  prev_ = nullptr;
  next_ = g_root;
  if (next_)
    next_->prev_ = this;
  g_root = this;
}

void TabLoadingFrameNavigationScheduler::DidFinishNavigation(
    content::NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // If we're throttling a canceled navigation then stop tracking it. The
  // |handle| becomes invalid shortly after this function returns.
  throttles_.erase(handle);
}

void TabLoadingFrameNavigationScheduler::StopThrottlingImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Release all of the throttles.
  for (auto& entry : throttles_) {
    auto* throttle = entry.second;
    throttle->Resume();
  }
  throttles_.clear();

  // Tear down this object. This must be called last so as not to UAF ourselves.
  // Note that this is always called from static functions in this translation
  // unit, thus there are no other frames on the stack belonging to this object.
  web_contents()->RemoveUserData(UserDataKey());
}

}  // namespace mechanisms
}  // namespace performance_manager
