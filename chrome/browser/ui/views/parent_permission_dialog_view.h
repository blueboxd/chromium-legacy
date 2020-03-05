// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PARENT_PERMISSION_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PARENT_PERMISSION_DIALOG_VIEW_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/browser/extensions/install_prompt_permissions.h"
#include "chrome/browser/ui/supervised_user/parent_permission_dialog.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view.h"

class Profile;

namespace extensions {
class Extension;
}  // namespace extensions

class ParentPermissionSection;

// Modal dialog that shows a dialog that prompts a parent for permission by
// asking them to enter their google account credentials.
class ParentPermissionDialogView : public views::BubbleDialogDelegateView {
 public:
  using DoneCallback = base::OnceCallback<void(
      internal::ParentPermissionDialogViewResult result)>;

  struct Params {
    Params();
    explicit Params(const Params& params);
    ~Params();

    // List of email addresses of parents for whom parent permission for
    // installation should be requested.  These should be the emails of parent
    // accounts that are permitted to approve extension installations
    // for the current child user.
    std::vector<base::string16> parent_permission_email_addresses;

    // If true, shows a message in the parent permission dialog that the
    // password entered was incorrect.
    bool show_parent_password_incorrect = true;

    // The icon to be displayed.
    gfx::ImageSkia icon;

    // The message to show.
    base::string16 message;

    // An optional extension whose permissions should be displayed
    const extensions::Extension* extension = nullptr;

    // The user's profile
    Profile* profile = nullptr;

    // The parent window to this window.
    gfx::NativeWindow window = nullptr;
  };

  ParentPermissionDialogView(std::unique_ptr<Params> params,
                             DoneCallback done_callback);

  ~ParentPermissionDialogView() override;

  ParentPermissionDialogView(const ParentPermissionDialogView&) = delete;
  ParentPermissionDialogView& operator=(const ParentPermissionDialogView&) =
      delete;

  // Shows the parent permission dialog.
  void ShowDialog();

  void set_selected_parent_permission_email_address(
      const base::string16& email_address) {
    selected_parent_permission_email_ = email_address;
  }

  void set_parent_permission_credential(const base::string16& credential) {
    parent_permission_credential_ = credential;
  }
  void CloseDialogView();

  // TODO(https://crbug.com/1058501):  Instead of doing this with closures, use
  // an interface shared with the views code whose implementation wraps the
  // underlying view, and delete the instance of that interface in the dtor of
  // this class.
  base::OnceClosure GetCloseDialogClosure();

 private:
  const Params& params() const { return *params_; }

  base::string16 GetActiveUserFirstName() const;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void AddedToWidget() override;

  // views::DialogDeleate:
  bool Cancel() override;
  bool Accept() override;

  // views::WidgetDelegate:
  base::string16 GetAccessibleWindowTitle() const override;
  ui::ModalType GetModalType() const override;
  bool ShouldShowCloseButton() const override;

  // Changes the widget size to accommodate the contents' preferred size.
  void ResizeWidget();

  // Creates the contents area that contains permissions and other extension
  // info.
  void CreateContents();

  void ShowDialogInternal();

  // Sets the |extension| to be optionally displayed in the dialog.  This
  // causes the view to show several extension properties including the
  // permissions, the icon and the extension name.
  void InitializeExtensionData(
      scoped_refptr<const extensions::Extension> extension);

  // Permissions ot be displayed in the prompt. Only populated
  // if an extension has been set.
  extensions::InstallPromptPermissions prompt_permissions_;

  std::unique_ptr<ParentPermissionSection> parent_permission_section_;

  base::string16 selected_parent_permission_email_;
  base::string16 parent_permission_credential_;

  // Configuration parameters for the prompt.
  std::unique_ptr<Params> params_;

  DoneCallback done_callback_;

  base::WeakPtrFactory<ParentPermissionDialogView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PARENT_PERMISSION_DIALOG_VIEW_H_
