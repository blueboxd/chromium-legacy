// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_throttle_runner.h"

#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/frame_host/ancestor_throttle.h"
#include "content/browser/frame_host/blocked_scheme_navigation_throttle.h"
#include "content/browser/frame_host/form_submission_throttle.h"
#include "content/browser/frame_host/history_navigation_ablation_study_navigation_throttle.h"
#include "content/browser/frame_host/mixed_content_navigation_throttle.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigator_delegate.h"
#include "content/browser/frame_host/origin_policy_throttle.h"
#include "content/browser/frame_host/webui_navigation_throttle.h"

namespace content {

namespace {

NavigationThrottle::ThrottleCheckResult ExecuteNavigationEvent(
    NavigationThrottle* throttle,
    NavigationThrottleRunner::Event event) {
  switch (event) {
    case NavigationThrottleRunner::Event::WillStartRequest:
      return throttle->WillStartRequest();
    case NavigationThrottleRunner::Event::WillRedirectRequest:
      return throttle->WillRedirectRequest();
    case NavigationThrottleRunner::Event::WillFailRequest:
      return throttle->WillFailRequest();
    case NavigationThrottleRunner::Event::WillProcessResponse:
      return throttle->WillProcessResponse();
    default:
      NOTREACHED();
  }
  NOTREACHED();
  return NavigationThrottle::CANCEL_AND_IGNORE;
}

const char* GetEventName(NavigationThrottleRunner::Event event) {
  switch (event) {
    case NavigationThrottleRunner::Event::WillStartRequest:
      return "NavigationThrottle::WillStartRequest";
    case NavigationThrottleRunner::Event::WillRedirectRequest:
      return "NavigationThrottle::WillRedirectRequest";
    case NavigationThrottleRunner::Event::WillFailRequest:
      return "NavigationThrottle::WillFailRequest";
    case NavigationThrottleRunner::Event::WillProcessResponse:
      return "NavigationThrottle::WillProcessResponse";
    default:
      NOTREACHED();
  }
  return "";
}

}  // namespace

NavigationThrottleRunner::NavigationThrottleRunner(Delegate* delegate,
                                                   NavigationHandle* handle)
    : delegate_(delegate), handle_(handle) {}

NavigationThrottleRunner::~NavigationThrottleRunner() = default;

void NavigationThrottleRunner::ProcessNavigationEvent(Event event) {
  DCHECK_NE(Event::NoEvent, event);
  current_event_ = event;
  next_index_ = 0;
  ProcessInternal();
}

void NavigationThrottleRunner::ResumeProcessingNavigationEvent(
    NavigationThrottle* deferring_throttle) {
  DCHECK_EQ(GetDeferringThrottle(), deferring_throttle);
  ProcessInternal();
}

void NavigationThrottleRunner::CallResumeForTesting() {
  ProcessInternal();
}

void NavigationThrottleRunner::RegisterNavigationThrottles() {
  // Note: |throttle_| might not be empty. Some NavigationThrottles might have
  // been registered with RegisterThrottleForTesting. These must reside at the
  // end of |throttles_|. TestNavigationManagerThrottle expects that the
  // NavigationThrottles added for test are the last NavigationThrottles to
  // execute. Take them out while appending the rest of the
  // NavigationThrottles.
  std::vector<std::unique_ptr<NavigationThrottle>> testing_throttles =
      std::move(throttles_);

  NavigationHandleImpl* handle = static_cast<NavigationHandleImpl*>(handle_);
  throttles_ = handle->GetDelegate()->CreateThrottlesForNavigation(handle);

  // Enforce rules for WebUI navigations.
  AddThrottle(WebUINavigationThrottle::CreateThrottleForNavigation(handle));

  // Check for renderer-inititated main frame navigations to blocked URL schemes
  // (data, filesystem). This is done early as it may block the main frame
  // navigation altogether.
  AddThrottle(
      BlockedSchemeNavigationThrottle::CreateThrottleForNavigation(handle));

  AddThrottle(AncestorThrottle::MaybeCreateThrottleFor(handle));
  AddThrottle(FormSubmissionThrottle::MaybeCreateThrottleFor(handle));

  // Check for mixed content. This is done after the AncestorThrottle and the
  // FormSubmissionThrottle so that when folks block mixed content with a CSP
  // policy, they don't get a warning. They'll still get a warning in the
  // console about CSP blocking the load.
  AddThrottle(
      MixedContentNavigationThrottle::CreateThrottleForNavigation(handle));

  // Handle Origin Policy (if enabled)
  AddThrottle(OriginPolicyThrottle::MaybeCreateThrottleFor(handle));

  for (auto& throttle :
       devtools_instrumentation::CreateNavigationThrottles(handle)) {
    AddThrottle(std::move(throttle));
  }

  // Delay navigation for an ablation study (if needed).
  AddThrottle(HistoryNavigationAblationStudyNavigationThrottle::
                  MaybeCreateForNavigation(handle));

  // Insert all testing NavigationThrottles last.
  throttles_.insert(throttles_.end(),
                    std::make_move_iterator(testing_throttles.begin()),
                    std::make_move_iterator(testing_throttles.end()));
}

NavigationThrottle* NavigationThrottleRunner::GetDeferringThrottle() const {
  if (next_index_ == 0)
    return nullptr;
  return throttles_[next_index_ - 1].get();
}

void NavigationThrottleRunner::AddThrottle(
    std::unique_ptr<NavigationThrottle> navigation_throttle) {
  if (navigation_throttle)
    throttles_.push_back(std::move(navigation_throttle));
}

void NavigationThrottleRunner::ProcessInternal() {
  DCHECK_NE(Event::NoEvent, current_event_);
  base::WeakPtr<NavigationThrottleRunner> weak_ref = weak_factory_.GetWeakPtr();
  for (size_t i = next_index_; i < throttles_.size(); ++i) {
    TRACE_EVENT1("navigation", GetEventName(current_event_), "throttle",
                 throttles_[i]->GetNameForLogging());
    NavigationThrottle::ThrottleCheckResult result =
        ExecuteNavigationEvent(throttles_[i].get(), current_event_);
    if (!weak_ref) {
      // The NavigationThrottle execution has destroyed this
      // NavigationThrottleRunner. Return immediately.
      return;
    }
    TRACE_EVENT_ASYNC_STEP_INTO0(
        "navigation", "NavigationHandle", handle_,
        base::StringPrintf("%s: %s: %d", GetEventName(current_event_),
                           throttles_[i]->GetNameForLogging(),
                           result.action()));
    switch (result.action()) {
      case NavigationThrottle::PROCEED:
        continue;

      case NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE:
      case NavigationThrottle::BLOCK_REQUEST:
      case NavigationThrottle::BLOCK_RESPONSE:
      case NavigationThrottle::CANCEL:
      case NavigationThrottle::CANCEL_AND_IGNORE:
        next_index_ = 0;
        InformDelegate(result);
        return;

      case NavigationThrottle::DEFER:
        next_index_ = i + 1;
        return;
    }
  }

  next_index_ = 0;
  InformDelegate(NavigationThrottle::PROCEED);
}

void NavigationThrottleRunner::InformDelegate(
    const NavigationThrottle::ThrottleCheckResult& result) {
  // Now that the event has executed, reset the current event to NoEvent since
  // we're no longer processing any event. Do it before the call to the
  // delegate, as it might lead to the deletion of this
  // NavigationThrottleRunner.
  Event event = current_event_;
  current_event_ = Event::NoEvent;
  delegate_->OnNavigationEventProcessed(event, result);
  // DO NOT ADD CODE AFTER THIS. The NavigationThrottleRunner might have been
  // deleted by the previous call.
}

}  // namespace content
