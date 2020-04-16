// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_view.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/google_chrome_strings.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "chrome/browser/shell_integration_win.h"
#include "ui/base/win/shell.h"
#include "ui/views/win/hwnd_util.h"
#endif

namespace {

ProfilePickerView* g_profile_picker_view = nullptr;
constexpr int kWindowWidth = 800;
constexpr int kWindowHeight = 600;
}  // namespace

// static
void ProfilePicker::Show() {
  if (!g_profile_picker_view)
    g_profile_picker_view = new ProfilePickerView();

  g_profile_picker_view->Display();
}

// static
void ProfilePicker::Hide() {
  if (g_profile_picker_view)
    g_profile_picker_view->Clear();
}

ProfilePickerView::ProfilePickerView()
    : web_view_(nullptr), initialized_(InitState::kNotInitialized) {
  DialogDelegate::SetButtons(ui::DIALOG_BUTTON_NONE);
  DialogDelegate::set_use_custom_frame(false);
  // TODO(crbug.com/1063856): Add |RecordDialogCreation|.
}

ProfilePickerView::~ProfilePickerView() = default;

void ProfilePickerView::Display() {
  if (initialized_ == kNotInitialized) {
    initialized_ = kInProgress;
    g_browser_process->profile_manager()->CreateProfileAsync(
        ProfileManager::GetSystemProfilePath(),
        base::BindRepeating(&ProfilePickerView::OnSystemProfileCreated,
                            weak_ptr_factory_.GetWeakPtr()),
        /*name=*/base::string16(), /*icon_url=*/std::string());
    return;
  }

  if (initialized_ == kInProgress)
    return;

  GetWidget()->Activate();
}

void ProfilePickerView::Clear() {
  if (initialized_ == kDone) {
    GetWidget()->Close();
    return;
  }

  WindowClosing();
  DeleteDelegate();
}

void ProfilePickerView::OnSystemProfileCreated(Profile* system_profile,
                                               Profile::CreateStatus status) {
  DCHECK_NE(status, Profile::CREATE_STATUS_LOCAL_FAIL);
  if (status != Profile::CREATE_STATUS_INITIALIZED)
    return;

  Init(system_profile);
}

void ProfilePickerView::Init(Profile* system_profile) {
  web_view_ = new views::WebView(system_profile);
  AddChildView(web_view_);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  DialogDelegate::CreateDialogWidget(this, nullptr, nullptr);

#if defined(OS_WIN)
  // Set the app id for the user manager to the app id of its parent.
  ui::win::SetAppIdForWindow(
      shell_integration::win::GetChromiumModelIdForProfile(
          system_profile->GetPath()),
      views::HWNDForWidget(GetWidget()));
#endif

  web_view_->LoadInitialURL(GURL(chrome::kChromeUIMdUserManagerUrl));
  GetWidget()->Show();
  web_view_->RequestFocus();
  initialized_ = InitState::kDone;
}

gfx::Size ProfilePickerView::CalculatePreferredSize() const {
  return gfx::Size(kWindowWidth, kWindowHeight);
}

bool ProfilePickerView::CanResize() const {
  return true;
}

bool ProfilePickerView::CanMaximize() const {
  return true;
}

bool ProfilePickerView::CanMinimize() const {
  return true;
}

base::string16 ProfilePickerView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
}

void ProfilePickerView::WindowClosing() {
  // Now that the window is closed, we can allow a new one to be opened.
  // (WindowClosing comes in asynchronously from the call to Close() and we
  // may have already opened a new instance).
  if (g_profile_picker_view == this)
    g_profile_picker_view = nullptr;
}
