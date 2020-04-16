// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_VIEW_H_

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/profile_picker.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/window/dialog_delegate.h"

// Dialog widget that contains the Desktop Profile picker webui.
class ProfilePickerView : public views::DialogDelegateView {
 private:
  friend class ProfilePicker;

  // To display the Profile picker, use ProfilePicker::Show().
  ProfilePickerView();
  ~ProfilePickerView() override;

  enum InitState {
    kNotInitialized = 0,
    kInProgress = 1,
    kDone = 2,
  };

  // Displays the profile picker.
  void Display();
  // Hides the profile picker.
  void Clear();

  // On system profile creation success, it initializes the view.
  void OnSystemProfileCreated(Profile* system_profile,
                              Profile::CreateStatus status);

  // Creates and shows the dialog.
  void Init(Profile* guest_profile);

  // views::DialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  bool CanResize() const override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  base::string16 GetWindowTitle() const override;
  void WindowClosing() override;

  views::WebView* web_view_;
  InitState initialized_;
  base::WeakPtrFactory<ProfilePickerView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProfilePickerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_VIEW_H_
