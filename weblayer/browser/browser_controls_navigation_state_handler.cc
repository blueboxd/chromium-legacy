// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browser_controls_navigation_state_handler.h"

#include "base/feature_list.h"
#include "components/security_state/content/content_utils.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "weblayer/browser/browser_controls_navigation_state_handler_delegate.h"

namespace weblayer {
namespace {
const base::Feature kImmediatelyHideBrowserControlsForTest{
    "ImmediatelyHideBrowserControlsForTest", base::FEATURE_DISABLED_BY_DEFAULT};

// The time that must elapse after a navigation before the browser controls can
// be hidden. This value matches what chrome has in
// TabStateBrowserControlsVisibilityDelegate.
base::TimeDelta GetBrowserControlsAllowHideDelay() {
  if (base::FeatureList::IsEnabled(kImmediatelyHideBrowserControlsForTest))
    return base::TimeDelta();

  return base::TimeDelta::FromSeconds(3);
}

}  // namespace

BrowserControlsNavigationStateHandler::BrowserControlsNavigationStateHandler(
    content::WebContents* web_contents,
    BrowserControlsNavigationStateHandlerDelegate* delegate)
    : WebContentsObserver(web_contents), delegate_(delegate) {}

BrowserControlsNavigationStateHandler::
    ~BrowserControlsNavigationStateHandler() = default;

void BrowserControlsNavigationStateHandler::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame() &&
      !navigation_handle->IsSameDocument()) {
    forced_show_during_load_timer_.Stop();
    SetForceShowDuringLoad(true);
  }
}

void BrowserControlsNavigationStateHandler::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame()) {
    if (navigation_handle->HasCommitted())
      is_showing_error_page_ = navigation_handle->IsErrorPage();
    delegate_->OnUpdateBrowserControlsStateBecauseOfProcessSwitch(
        navigation_handle->HasCommitted());
  }
}

void BrowserControlsNavigationStateHandler::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  const bool is_main_frame =
      render_frame_host->GetMainFrame() == render_frame_host;
  if (is_main_frame)
    ScheduleStopDelayedForceShow();
}

void BrowserControlsNavigationStateHandler::DidFailLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code) {
  if (render_frame_host->IsCurrent() &&
      (render_frame_host == web_contents()->GetMainFrame())) {
    UpdateState();
  }
}

void BrowserControlsNavigationStateHandler::DidChangeVisibleSecurityState() {
  UpdateState();
}

void BrowserControlsNavigationStateHandler::RenderProcessGone(
    base::TerminationStatus status) {
  UpdateState();
  // TODO: this likely needs to force visibility (because we won't get anything
  // from the renderer).
}

void BrowserControlsNavigationStateHandler::OnRendererUnresponsive(
    content::RenderProcessHost* render_process_host) {
  UpdateState();
  // TODO: this likely needs to force visibility (because we won't get anything
  // from the renderer).
}

void BrowserControlsNavigationStateHandler::SetForceShowDuringLoad(bool value) {
  if (force_show_during_load_ == value)
    return;

  force_show_during_load_ = value;
  UpdateState();
}

void BrowserControlsNavigationStateHandler::ScheduleStopDelayedForceShow() {
  forced_show_during_load_timer_.Start(
      FROM_HERE, GetBrowserControlsAllowHideDelay(),
      base::BindOnce(
          &BrowserControlsNavigationStateHandler::SetForceShowDuringLoad,
          base::Unretained(this), false));
}

void BrowserControlsNavigationStateHandler::UpdateState() {
  const content::BrowserControlsState current_state = CalculateCurrentState();
  if (current_state == last_state_)
    return;
  last_state_ = current_state;
  delegate_->OnBrowserControlsStateStateChanged(*last_state_);
}

content::BrowserControlsState
BrowserControlsNavigationStateHandler::CalculateCurrentState() {
  // TODO(sky): handle checks for unresponsive/responsive.
  // TODO(sky): this needs to force SHOWN if a11y enabled, see
  // AccessibilityUtil.isAccessibilityEnabled().
  if (force_show_during_load_ || web_contents()->IsFullscreen() ||
      web_contents()->IsFocusedElementEditable() ||
      web_contents()->IsBeingDestroyed() || web_contents()->IsCrashed()) {
    return content::BROWSER_CONTROLS_STATE_SHOWN;
  }

  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();
  if (!entry || entry->GetPageType() != content::PAGE_TYPE_NORMAL)
    return content::BROWSER_CONTROLS_STATE_SHOWN;

  if (entry->GetURL().SchemeIs(content::kChromeUIScheme))
    return content::BROWSER_CONTROLS_STATE_SHOWN;

  const security_state::SecurityLevel security_level =
      security_state::GetSecurityLevel(
          *security_state::GetVisibleSecurityState(web_contents()),
          /* used_policy_installed_certificate= */ false);
  switch (security_level) {
    case security_state::WARNING:
    case security_state::DANGEROUS:
      return content::BROWSER_CONTROLS_STATE_SHOWN;

    case security_state::NONE:
    case security_state::SECURE:
    case security_state::SECURE_WITH_POLICY_INSTALLED_CERT:
    case security_state::SECURITY_LEVEL_COUNT:
      break;
  }

  return content::BROWSER_CONTROLS_STATE_BOTH;
}

}  // namespace weblayer
