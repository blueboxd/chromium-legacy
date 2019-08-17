// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/proactive_suggestions_client_impl.h"

#include "ash/public/cpp/assistant/proactive_suggestions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/assistant/assistant_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "services/service_manager/public/cpp/connector.h"

ProactiveSuggestionsClientImpl::ProactiveSuggestionsClientImpl(
    AssistantClient* client) {
  // Initialize the Assistant state proxy.
  mojo::PendingRemote<ash::mojom::VoiceInteractionController> controller;
  client->RequestVoiceInteractionController(
      controller.InitWithNewPipeAndPassReceiver());
  assistant_state_.Init(std::move(controller));

  // We observe Assistant state to detect enabling/disabling of Assistant in
  // settings as well as enabling/disabling of screen context.
  assistant_state_.AddObserver(this);

  // We observe the singleton BrowserList to receive events pertaining to the
  // currently active browser.
  BrowserList::AddObserver(this);
}

ProactiveSuggestionsClientImpl::~ProactiveSuggestionsClientImpl() {
  WebContentsObserver::Observe(nullptr);

  if (active_browser_)
    active_browser_->tab_strip_model()->RemoveObserver(this);

  BrowserList::RemoveObserver(this);
  assistant_state_.RemoveObserver(this);
}

void ProactiveSuggestionsClientImpl::OnBrowserRemoved(Browser* browser) {
  if (browser == active_browser_)
    SetActiveBrowser(nullptr);
}

void ProactiveSuggestionsClientImpl::OnBrowserSetLastActive(Browser* browser) {
  SetActiveBrowser(browser);
}

void ProactiveSuggestionsClientImpl::OnBrowserNoLongerActive(Browser* browser) {
  if (browser == active_browser_)
    SetActiveBrowser(nullptr);
}

void ProactiveSuggestionsClientImpl::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // When the currently active browser tab changes, that indicates there has
  // been a change in the active contents.
  if (selection.active_tab_changed())
    SetActiveContents(tab_strip_model->GetActiveWebContents());
}

void ProactiveSuggestionsClientImpl::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  SetActiveUrl(active_contents_->GetURL());
}

void ProactiveSuggestionsClientImpl::OnVoiceInteractionSettingsEnabled(
    bool enabled) {
  // When Assistant is enabled/disabled in settings we may need to resume/pause
  // observation of the browser. We accomplish this by updating active state.
  UpdateActiveState();
}

void ProactiveSuggestionsClientImpl::OnVoiceInteractionContextEnabled(
    bool enabled) {
  // When Assistant screen context is enabled/disabled in settings we may need
  // to resume/pause observation of the browser. We accomplish this by updating
  // active state.
  UpdateActiveState();
}

void ProactiveSuggestionsClientImpl::SetActiveBrowser(Browser* browser) {
  if (browser == active_browser_)
    return;

  // Clean up bindings on the previously active browser.
  if (active_browser_)
    active_browser_->tab_strip_model()->RemoveObserver(this);

  active_browser_ = browser;

  // We need to update active state to conditionally observe the active browser.
  UpdateActiveState();
}

void ProactiveSuggestionsClientImpl::SetActiveContents(
    content::WebContents* contents) {
  if (contents == active_contents_)
    return;

  active_contents_ = contents;

  // We observe the active contents so as to detect navigation changes.
  WebContentsObserver::Observe(active_contents_);

  // Perform an initial sync of the active url.
  SetActiveUrl(active_contents_ ? active_contents_->GetURL() : GURL());
}

void ProactiveSuggestionsClientImpl::SetActiveUrl(const GURL& url) {
  if (url == active_url_)
    return;

  active_url_ = url;

  // TODO(dmblack): Retrieve proactive suggestions and notify observers.
}

void ProactiveSuggestionsClientImpl::UpdateActiveState() {
  if (!active_browser_) {
    SetActiveContents(nullptr);
    return;
  }

  auto* tab_strip_model = active_browser_->tab_strip_model();

  // We never observe browsers that are off the record and we never observe
  // browsers when the user has disabled either Assistant or screen context.
  if (active_browser_->profile()->IsOffTheRecord() ||
      !assistant_state_.settings_enabled().value_or(false) ||
      !assistant_state_.context_enabled().value_or(false)) {
    tab_strip_model->RemoveObserver(this);
    SetActiveContents(nullptr);
    return;
  }

  // We observe the tab strip associated with the active browser so as to detect
  // changes to the currently active tab.
  tab_strip_model->AddObserver(this);

  // Perform an initial sync of the active contents.
  SetActiveContents(tab_strip_model->GetActiveWebContents());
}
