// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_PROACTIVE_SUGGESTIONS_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_PROACTIVE_SUGGESTIONS_CLIENT_IMPL_H_

#include "ash/public/cpp/assistant/assistant_state_proxy.h"
#include "ash/public/cpp/assistant/default_voice_interaction_observer.h"
#include "ash/public/cpp/assistant/proactive_suggestions_client.h"
#include "base/macros.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/web_contents_observer.h"

class AssistantClient;

// A browser client which observes changes to the singleton BrowserList on
// behalf of Assistant to provide it with information necessary to retrieve
// proactive content suggestions.
class ProactiveSuggestionsClientImpl
    : public ash::ProactiveSuggestionsClient,
      public BrowserListObserver,
      public TabStripModelObserver,
      public content::WebContentsObserver,
      public ash::DefaultVoiceInteractionObserver {
 public:
  explicit ProactiveSuggestionsClientImpl(AssistantClient* client);
  ~ProactiveSuggestionsClientImpl() override;

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;
  void OnBrowserSetLastActive(Browser* browser) override;
  void OnBrowserNoLongerActive(Browser* browser) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tap_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  // DefaultVoiceInteractionObserver:
  void OnVoiceInteractionSettingsEnabled(bool enabled) override;
  void OnVoiceInteractionContextEnabled(bool enabled) override;

 private:
  void SetActiveBrowser(Browser* browser);
  void SetActiveContents(content::WebContents* contents);
  void SetActiveUrl(const GURL& url);

  void UpdateActiveState();

  ash::AssistantStateProxy assistant_state_;

  Browser* active_browser_ = nullptr;
  content::WebContents* active_contents_ = nullptr;
  GURL active_url_;

  DISALLOW_COPY_AND_ASSIGN(ProactiveSuggestionsClientImpl);
};

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_PROACTIVE_SUGGESTIONS_CLIENT_IMPL_H_
