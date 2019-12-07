// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chrome_bubble_manager.h"

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/bubble/bubble_controller.h"
#include "components/bubble/bubble_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"

ChromeBubbleManager::ChromeBubbleManager(TabStripModel* tab_strip_model)
    : tab_strip_model_(tab_strip_model) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(tab_strip_model_);
  tab_strip_model_->AddObserver(this);
}

ChromeBubbleManager::~ChromeBubbleManager() {
  tab_strip_model_->RemoveObserver(this);
}

void ChromeBubbleManager::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kRemoved) {
    CloseAllBubbles(BUBBLE_CLOSE_TABDETACHED);
    // Any bubble that didn't close should update its anchor position.
    UpdateAllBubbleAnchors();
  }

  if (tab_strip_model->empty() || !selection.active_tab_changed())
    return;

  if (selection.old_contents)
    CloseAllBubbles(BUBBLE_CLOSE_TABSWITCHED);

  if (selection.new_contents)
    Observe(selection.new_contents);
}

void ChromeBubbleManager::FrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  // When a frame is destroyed, bubbles spawned by that frame should default to
  // being closed, so that they can't traverse any references they hold to the
  // destroyed frame.
  CloseBubblesOwnedBy(render_frame_host);
}

void ChromeBubbleManager::DidToggleFullscreenModeForTab(
    bool entered_fullscreen,
    bool will_cause_resize) {
  CloseAllBubbles(BUBBLE_CLOSE_FULLSCREEN_TOGGLED);
  // Any bubble that didn't close should update its anchor position.
  UpdateAllBubbleAnchors();
}

void ChromeBubbleManager::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  if (!load_details.is_same_document)
    CloseAllBubbles(BUBBLE_CLOSE_NAVIGATED);
}
