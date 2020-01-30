// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/javascript_dialogs/javascript_dialog_tab_helper_delegate_android.h"

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"

JavaScriptDialogTabHelperDelegateAndroid::
    JavaScriptDialogTabHelperDelegateAndroid(content::WebContents* web_contents)
    : web_contents_(web_contents) {}

JavaScriptDialogTabHelperDelegateAndroid::
    ~JavaScriptDialogTabHelperDelegateAndroid() {}

void JavaScriptDialogTabHelperDelegateAndroid::WillRunDialog() {}

void JavaScriptDialogTabHelperDelegateAndroid::DidCloseDialog() {}

void JavaScriptDialogTabHelperDelegateAndroid::SetTabNeedsAttention(
    bool attention) {}

bool JavaScriptDialogTabHelperDelegateAndroid::IsWebContentsForemost() {
  TabModel* tab_model = TabModelList::GetTabModelForWebContents(web_contents_);
  if (tab_model) {
    return tab_model->IsCurrentModel() &&
           tab_model->GetActiveWebContents() == web_contents_;
  } else {
    // If tab model is not found (e.g. single tab model), fall back to check
    // whether the tab for this web content is interactive.
    TabAndroid* tab = TabAndroid::FromWebContents(web_contents_);
    return tab && tab->IsUserInteractable();
  }
}

bool JavaScriptDialogTabHelperDelegateAndroid::IsApp() {
  return false;
}
