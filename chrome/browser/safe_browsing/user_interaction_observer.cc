// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/user_interaction_observer.h"

#include <string>

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "components/safe_browsing/core/features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

namespace safe_browsing {

// If true, a delayed warning will be shown when the user clicks on the page.
// If false, the warning won't be shown, but a metric will be recorded on the
// first click.
const base::FeatureParam<bool> kEnableMouseClicks{&kDelayedWarnings, "mouse",
                                                  /*default_value=*/false};

const char kDelayedWarningsHistogram[] = "SafeBrowsing.DelayedWarnings.Event";

namespace {
const char kWebContentsUserDataKey[] =
    "web_contents_safe_browsing_user_interaction_observer";

void RecordUMA(DelayedWarningEvent event) {
  base::UmaHistogramEnumeration(kDelayedWarningsHistogram, event);
}

}  // namespace

SafeBrowsingUserInteractionObserver::SafeBrowsingUserInteractionObserver(
    content::WebContents* web_contents,
    const security_interstitials::UnsafeResource& resource,
    bool is_main_frame,
    scoped_refptr<SafeBrowsingUIManager> ui_manager)
    : content::WebContentsObserver(web_contents),
      web_contents_(web_contents),
      resource_(resource),
      ui_manager_(ui_manager) {
  DCHECK(base::FeatureList::IsEnabled(kDelayedWarnings));
  key_press_callback_ =
      base::BindRepeating(&SafeBrowsingUserInteractionObserver::HandleKeyPress,
                          base::Unretained(this));
  mouse_event_callback_ = base::BindRepeating(
      &SafeBrowsingUserInteractionObserver::HandleMouseEvent,
      base::Unretained(this));
  // Pass a callback to the render widget host instead of implementing
  // WebContentsObserver::DidGetUserInteraction(). The reason for this is that
  // render widget host handles keyboard events earlier and the callback can
  // indicate that it wants the key press to be ignored.
  // (DidGetUserInteraction() can only observe and not cancel the event.)
  web_contents->GetRenderViewHost()->GetWidget()->AddKeyPressEventCallback(
      key_press_callback_);
  web_contents->GetRenderViewHost()->GetWidget()->AddMouseEventCallback(
      mouse_event_callback_);
  RecordUMA(DelayedWarningEvent::kPageLoaded);
}

SafeBrowsingUserInteractionObserver::~SafeBrowsingUserInteractionObserver() {
  web_contents_->GetRenderViewHost()->GetWidget()->RemoveKeyPressEventCallback(
      key_press_callback_);
  web_contents_->GetRenderViewHost()->GetWidget()->RemoveMouseEventCallback(
      mouse_event_callback_);
  if (!interstitial_shown_) {
    RecordUMA(DelayedWarningEvent::kWarningNotShown);
  }
}

// static
void SafeBrowsingUserInteractionObserver::CreateForWebContents(
    content::WebContents* web_contents,
    const security_interstitials::UnsafeResource& resource,
    bool is_main_frame,
    scoped_refptr<SafeBrowsingUIManager> ui_manager) {
  // This method is called for all unsafe resources on |web_contents|. Only
  // create an observer if there isn't one.
  // TODO(crbug.com/1057157): The observer should observe all unsafe resources
  // instead of the first one only.
  if (FromWebContents(web_contents)) {
    return;
  }
  auto observer = std::make_unique<SafeBrowsingUserInteractionObserver>(
      web_contents, resource, is_main_frame, ui_manager);
  web_contents->SetUserData(kWebContentsUserDataKey, std::move(observer));
}

// static
SafeBrowsingUserInteractionObserver*
SafeBrowsingUserInteractionObserver::FromWebContents(
    content::WebContents* web_contents) {
  return static_cast<SafeBrowsingUserInteractionObserver*>(
      web_contents->GetUserData(kWebContentsUserDataKey));
}

void SafeBrowsingUserInteractionObserver::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  old_host->GetWidget()->RemoveKeyPressEventCallback(key_press_callback_);
  new_host->GetWidget()->AddKeyPressEventCallback(key_press_callback_);

  old_host->GetWidget()->RemoveMouseEventCallback(mouse_event_callback_);
  new_host->GetWidget()->AddMouseEventCallback(mouse_event_callback_);
}

void SafeBrowsingUserInteractionObserver::WebContentsDestroyed() {
  CleanUp();
}

void SafeBrowsingUserInteractionObserver::DidStartNavigation(
    content::NavigationHandle* handle) {
  // Ignore subframe navigations and same document navigations. These don't
  // show full page interstitials.
  if (!handle->IsInMainFrame() || handle->IsSameDocument()) {
    return;
  }
  web_contents()->RemoveUserData(kWebContentsUserDataKey);
}

void SafeBrowsingUserInteractionObserver::DidToggleFullscreenModeForTab(
    bool entered_fullscreen,
    bool will_cause_resize) {
  // This class is only instantiated upon a navigation. If a page is in
  // fullscreen mode, any navigation away from it should exit fullscreen. This
  // means that this class is never instantiated while the current web contents
  // is in fullscreen mode, so |entered_fullscreen| should never be false when
  // this method is called for the first time. However, we don't know if it's
  // guaranteed for a page to not be in fullscreen upon navigation, so we just
  // ignore this event if the page exited fullscreen.
  if (!entered_fullscreen) {
    return;
  }
  // IMPORTANT: Store the web contents pointer in a temporary because |this| is
  // deleted after ShowInterstitial().
  content::WebContents* contents = web_contents();
  ShowInterstitial(DelayedWarningEvent::kWarningShownOnFullscreenAttempt);
  // Exit fullscreen only after navigating to the interstitial. We don't want to
  // interfere with an ongoing fullscreen request.
  contents->ExitFullscreen(will_cause_resize);
  // DO NOT add code past this point. |this| is destroyed.
}

bool SafeBrowsingUserInteractionObserver::HandleKeyPress(
    const content::NativeWebKeyboardEvent& event) {
  ShowInterstitial(DelayedWarningEvent::kWarningShownOnKeypress);
  // DO NOT add code past this point. |this| is destroyed.
  return true;
}

bool SafeBrowsingUserInteractionObserver::HandleMouseEvent(
    const blink::WebMouseEvent& event) {
  if (event.GetType() != blink::WebInputEvent::Type::kMouseDown) {
    return false;
  }
  // If warning isn't enabled for mouse clicks, still record the first time when
  // the user clicks.
  if (!kEnableMouseClicks.Get()) {
    if (!mouse_click_with_no_warning_recorded_) {
      RecordUMA(DelayedWarningEvent::kWarningNotTriggeredOnMouseClick);
      mouse_click_with_no_warning_recorded_ = true;
    }
    return false;
  }
  ShowInterstitial(DelayedWarningEvent::kWarningShownOnMouseClick);
  // DO NOT add code past this point. |this| is destroyed.
  return true;
}

void SafeBrowsingUserInteractionObserver::ShowInterstitial(
    DelayedWarningEvent event) {
  // Show the interstitial.
  DCHECK(!interstitial_shown_);
  interstitial_shown_ = true;
  CleanUp();
  RecordUMA(event);
  SafeBrowsingUIManager::StartDisplayingBlockingPage(ui_manager_, resource_);
  // DO NOT add code past this point. |this| is destroyed.
}

void SafeBrowsingUserInteractionObserver::CleanUp() {
  web_contents_->GetRenderViewHost()->GetWidget()->RemoveKeyPressEventCallback(
      key_press_callback_);
  web_contents_->GetRenderViewHost()->GetWidget()->RemoveMouseEventCallback(
      mouse_event_callback_);
}

}  // namespace safe_browsing
