// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/user_interaction_observer.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "components/safe_browsing/core/features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

namespace safe_browsing {

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
  // Pass a callback to the render widget host instead of implementing
  // WebContentsObserver::DidGetUserInteraction(). The reason for this is that
  // render widget host handles keyboard events earlier and the callback can
  // indicate that it wants the key press to be ignored.
  // (DidGetUserInteraction() can only observe and not cancel the event.)
  web_contents->GetRenderViewHost()->GetWidget()->AddKeyPressEventCallback(
      key_press_callback_);

  RecordUMA(DelayedWarningEvent::kPageLoaded);
}

SafeBrowsingUserInteractionObserver::~SafeBrowsingUserInteractionObserver() {
  web_contents_->GetRenderViewHost()->GetWidget()->RemoveKeyPressEventCallback(
      key_press_callback_);
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
  DCHECK(!FromWebContents(web_contents));
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

bool SafeBrowsingUserInteractionObserver::HandleKeyPress(
    const content::NativeWebKeyboardEvent& event) {
  CleanUp();
  // Show the interstitial.
  interstitial_shown_ = true;
  RecordUMA(DelayedWarningEvent::kWarningShownOnKeypress);
  SafeBrowsingUIManager::StartDisplayingBlockingPage(ui_manager_, resource_);
  // DO NOT add code past this point. |this| is destroyed.
  return true;
}

void SafeBrowsingUserInteractionObserver::CleanUp() {
  web_contents_->GetRenderViewHost()->GetWidget()->RemoveKeyPressEventCallback(
      key_press_callback_);
}

}  // namespace safe_browsing
