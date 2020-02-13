// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SAVE_UPDATE_WITH_ACCOUNT_STORE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SAVE_UPDATE_WITH_ACCOUNT_STORE_VIEW_H_

#include "chrome/browser/ui/passwords/bubble_controllers/save_update_with_account_store_bubble_controller.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/editable_combobox/editable_combobox_listener.h"
#include "ui/views/view.h"

namespace views {
class Combobox;
class EditableCombobox;
class ToggleImageButton;
}  // namespace views

// A view offering the user the ability to save or update credentials (depending
// on |is_update_bubble|) either in the profile and/or account stores. Contains
// a username and password field, and in case of a saving a destination picker.
// In addition, it contains a "Save"/"Update" button and a "Never"/"Nope"
// button.
class PasswordSaveUpdateWithAccountStoreView
    : public PasswordBubbleViewBase,
      public views::ButtonListener,
      public views::EditableComboboxListener,
      public views::ComboboxListener {
 public:
  PasswordSaveUpdateWithAccountStoreView(content::WebContents* web_contents,
                                         views::View* anchor_view,
                                         DisplayReason reason);

  views::Combobox* DestinationDropdownForTesting() {
    return destination_dropdown_;
  }

 private:
  ~PasswordSaveUpdateWithAccountStoreView() override;

  // PasswordBubbleViewBase
  PasswordBubbleControllerBase* GetController() override;
  const PasswordBubbleControllerBase* GetController() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::ComboboxListener:
  // User for the destination combobox.
  void OnPerformAction(views::Combobox* combobox) override;

  // views::EditableComboboxListener:
  // Used for both the username and password editable comboboxes.
  void OnContentChanged(views::EditableCombobox* editable_combobox) override;

  // PasswordBubbleViewBase:
  gfx::Size CalculatePreferredSize() const override;
  views::View* GetInitiallyFocusedView() override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  gfx::ImageSkia GetWindowIcon() override;
  bool ShouldShowWindowIcon() const override;
  bool ShouldShowCloseButton() const override;

  // View:
  void AddedToWidget() override;
  void OnThemeChanged() override;

  void TogglePasswordVisibility();
  void UpdateUsernameAndPasswordInModel();
  void UpdateDialogButtons();
  std::unique_ptr<views::View> CreateFooterView();

  SaveUpdateWithAccountStoreBubbleController controller_;

  // True iff it is an update password bubble on creation. False iff it is a
  // save bubble.
  const bool is_update_bubble_;

  views::Combobox* destination_dropdown_;

  views::EditableCombobox* username_dropdown_;
  views::ToggleImageButton* password_view_button_;

  // The view for the password value.
  views::EditableCombobox* password_dropdown_;

  bool are_passwords_revealed_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_SAVE_UPDATE_WITH_ACCOUNT_STORE_VIEW_H_
