// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_JAVASCRIPT_APP_MODAL_DIALOG_VIEWS_X11_H_
#define CHROME_BROWSER_UI_VIEWS_JAVASCRIPT_APP_MODAL_DIALOG_VIEWS_X11_H_

#include <memory>

#include "base/macros.h"
#include "components/javascript_dialogs/views/app_modal_dialog_view_views.h"

class JavascriptAppModalEventBlockerX11;
class PopunderPreventer;

// AppModalDialogView implementation for linux desktop.
class JavaScriptAppModalDialogViewsX11
    : public javascript_dialogs::AppModalDialogViewViews {
 public:
  explicit JavaScriptAppModalDialogViewsX11(
      javascript_dialogs::AppModalDialogController* parent);
  ~JavaScriptAppModalDialogViewsX11() override;

  // JavaScriptAppModalDialogViews:
  void ShowAppModalDialog() override;

  // views::DialogDelegate:
  void WindowClosing() override;

 private:
  // Blocks events to other browser windows while the dialog is open.
  std::unique_ptr<JavascriptAppModalEventBlockerX11> event_blocker_x11_;

  std::unique_ptr<PopunderPreventer> popunder_preventer_;

  DISALLOW_COPY_AND_ASSIGN(JavaScriptAppModalDialogViewsX11);
};

#endif  // CHROME_BROWSER_UI_VIEWS_JAVASCRIPT_APP_MODAL_DIALOG_VIEWS_X11_H_
