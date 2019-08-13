// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_ui_controller.h"

#include <utility>

#include "chrome/browser/sharing/sharing_dialog.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"

SharingUiController::App::App(const gfx::VectorIcon& icon,
                              base::string16 name,
                              std::string identifier)
    : icon(icon), name(std::move(name)), identifier(std::move(identifier)) {}

SharingUiController::App::App(App&& other) = default;

SharingUiController::App::~App() = default;

SharingUiController::SharingUiController(content::WebContents* web_contents)
    : web_contents_(web_contents) {}

void SharingUiController::ShowNewDialog() {
  if (dialog_)
    dialog_->Hide();

  // Treat the dialog as closed as the process of closing the native widget
  // might be async.
  dialog_ = nullptr;

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  auto* window = browser ? browser->window() : nullptr;
  if (!window)
    return;

  dialog_ = DoShowDialog(window);
  UpdateIcon();
}

void SharingUiController::UpdateIcon() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  auto* window = browser ? browser->window() : nullptr;
  if (!window)
    return;

  auto* icon_container = window->GetOmniboxPageActionIconContainer();
  if (icon_container)
    icon_container->UpdatePageActionIcon(GetIconType());
}

void SharingUiController::OnDialogClosed(SharingDialog* dialog) {
  // Ignore already replaced dialogs.
  if (dialog != dialog_)
    return;

  dialog_ = nullptr;
  UpdateIcon();
}

void SharingUiController::ShowErrorDialog() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  if (!browser)
    return;

  if (web_contents_ == browser->tab_strip_model()->GetActiveWebContents())
    ShowNewDialog();
}

void SharingUiController::StartLoading() {
  is_loading_ = true;
  send_failed_ = false;
  UpdateIcon();
}

void SharingUiController::StopLoading(bool send_failed) {
  is_loading_ = false;
  send_failed_ = send_failed;
  UpdateIcon();

  if (send_failed)
    ShowErrorDialog();
}

void SharingUiController::InvalidateOldDialog() {
  is_loading_ = false;
  send_failed_ = false;
  ShowNewDialog();
}
