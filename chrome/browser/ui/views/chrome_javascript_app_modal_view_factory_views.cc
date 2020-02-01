// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/javascript_dialogs/chrome_javascript_app_modal_dialog_view_factory.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/javascript_dialogs/app_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

#if defined(USE_X11)
#include "chrome/browser/ui/views/javascript_app_modal_dialog_views_x11.h"
#else
#include "chrome/browser/ui/blocked_content/popunder_preventer.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/javascript_dialogs/views/app_modal_dialog_view_views.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace {

#if !defined(USE_X11)
class ChromeJavaScriptAppModalDialogViews
    : public javascript_dialogs::AppModalDialogViewViews {
 public:
  explicit ChromeJavaScriptAppModalDialogViews(
      javascript_dialogs::AppModalDialogController* parent)
      : javascript_dialogs::AppModalDialogViewViews(parent),
        popunder_preventer_(parent->web_contents()) {}
  ~ChromeJavaScriptAppModalDialogViews() override = default;

 private:
  PopunderPreventer popunder_preventer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeJavaScriptAppModalDialogViews);
};
#endif

javascript_dialogs::AppModalDialogView* CreateNativeJavaScriptDialog(
    javascript_dialogs::AppModalDialogController* dialog) {
  javascript_dialogs::AppModalDialogViewViews* d = nullptr;
#if defined(USE_X11)
  d = new JavaScriptAppModalDialogViewsX11(dialog);
#else
  d = new ChromeJavaScriptAppModalDialogViews(dialog);
#endif
  dialog->web_contents()->GetDelegate()->ActivateContents(
      dialog->web_contents());
  gfx::NativeWindow parent_window =
      dialog->web_contents()->GetTopLevelNativeWindow();
#if defined(USE_AURA)
  if (!parent_window->GetRootWindow()) {
    // When we are part of a WebContents that isn't actually being displayed
    // on the screen, we can't actually attach to it.
    parent_window = nullptr;
  }
#endif
  constrained_window::CreateBrowserModalDialogViews(d, parent_window);
  return d;
}

}  // namespace

void InstallChromeJavaScriptAppModalDialogViewFactory() {
  javascript_dialogs::AppModalDialogManager::GetInstance()
      ->SetNativeDialogFactory(
          base::BindRepeating(&CreateNativeJavaScriptDialog));
}
