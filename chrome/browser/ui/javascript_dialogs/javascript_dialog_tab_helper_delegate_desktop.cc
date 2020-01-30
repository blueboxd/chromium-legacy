// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/javascript_dialogs/javascript_dialog_tab_helper_delegate_desktop.h"

#include <utility>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_dialog.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_dialog_tab_helper.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/app_modal/javascript_dialog_manager.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "ui/gfx/text_elider.h"

JavaScriptDialogTabHelperDelegateDesktop::
    JavaScriptDialogTabHelperDelegateDesktop(content::WebContents* web_contents)
    : web_contents_(web_contents) {}

JavaScriptDialogTabHelperDelegateDesktop::
    ~JavaScriptDialogTabHelperDelegateDesktop() {
  DCHECK(!tab_strip_model_being_observed_);
}

void JavaScriptDialogTabHelperDelegateDesktop::WillRunDialog() {
  BrowserList::AddObserver(this);
}

void JavaScriptDialogTabHelperDelegateDesktop::DidCloseDialog() {
  BrowserList::RemoveObserver(this);
}

void JavaScriptDialogTabHelperDelegateDesktop::SetTabNeedsAttention(
    bool attention) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  if (!browser) {
    // It's possible that the WebContents is no longer in the tab strip. If so,
    // just give up. https://crbug.com/786178#c7.
    return;
  }

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  SetTabNeedsAttentionImpl(
      attention, tab_strip_model,
      tab_strip_model->GetIndexOfWebContents(web_contents_));
}

bool JavaScriptDialogTabHelperDelegateDesktop::IsWebContentsForemost() {
  Browser* browser = BrowserList::GetInstance()->GetLastActive();
  DCHECK(browser);
  return browser->tab_strip_model()->GetActiveWebContents() == web_contents_;
}

bool JavaScriptDialogTabHelperDelegateDesktop::IsApp() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  return browser && browser->deprecated_is_app();
}

void JavaScriptDialogTabHelperDelegateDesktop::OnBrowserSetLastActive(
    Browser* browser) {
  JavaScriptDialogTabHelper::FromWebContents(web_contents_)
      ->BrowserActiveStateChanged();
}

void JavaScriptDialogTabHelperDelegateDesktop::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kReplaced) {
    auto* replace = change.GetReplace();
    if (replace->old_contents == web_contents_) {
      // At this point, this WebContents is no longer in the tabstrip. The usual
      // teardown will not be able to turn off the attention indicator, so that
      // must be done here.
      SetTabNeedsAttentionImpl(false, tab_strip_model, replace->index);

      JavaScriptDialogTabHelper::FromWebContents(web_contents_)
          ->CloseDialogWithReason(
              JavaScriptDialogTabHelper::DismissalCause::kTabSwitchedOut);
    }
  } else if (change.type() == TabStripModelChange::kRemoved) {
    for (const auto& contents : change.GetRemove()->contents) {
      if (contents.contents == web_contents_) {
        // We don't call TabStripModel::SetTabNeedsAttention because it causes
        // re-entrancy into TabStripModel and correctness of the |index|
        // parameter is dependent on observer ordering. This is okay in the
        // short term because the tab in question is being removed.
        // TODO(erikchen): Clean up TabStripModel observer API so that this
        // doesn't require re-entrancy and/or works correctly
        // https://crbug.com/842194.
        DCHECK(tab_strip_model_being_observed_);
        tab_strip_model_being_observed_->RemoveObserver(this);
        tab_strip_model_being_observed_ = nullptr;
        JavaScriptDialogTabHelper::FromWebContents(web_contents_)
            ->CloseDialogWithReason(
                JavaScriptDialogTabHelper::DismissalCause::kTabHelperDestroyed);
        break;
      }
    }
  }
}

void JavaScriptDialogTabHelperDelegateDesktop::SetTabNeedsAttentionImpl(
    bool attention,
    TabStripModel* tab_strip_model,
    int index) {
  tab_strip_model->SetTabNeedsAttentionAt(index, attention);
  if (attention) {
    tab_strip_model->AddObserver(this);
    tab_strip_model_being_observed_ = tab_strip_model;
  } else {
    DCHECK_EQ(tab_strip_model_being_observed_, tab_strip_model);
    tab_strip_model_being_observed_->RemoveObserver(this);
    tab_strip_model_being_observed_ = nullptr;
  }
}
