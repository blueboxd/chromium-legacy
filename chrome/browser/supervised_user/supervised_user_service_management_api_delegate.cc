// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_service_management_api_delegate.h"

#include <memory>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/supervised_user/parent_permission_dialog.h"

namespace {

void OnParentPermissionDialogComplete(
    std::unique_ptr<ParentPermissionDialog> dialog,
    extensions::SupervisedUserServiceDelegate::
        ParentPermissionDialogDoneCallback delegate_done_callback,
    ParentPermissionDialog::Result result) {
  switch (result) {
    case ParentPermissionDialog::Result::kParentPermissionReceived:
      std::move(delegate_done_callback)
          .Run(extensions::SupervisedUserServiceDelegate::
                   ParentPermissionDialogResult::kParentPermissionReceived);
      break;
    case ParentPermissionDialog::Result::kParentPermissionCanceled:
      std::move(delegate_done_callback)
          .Run(extensions::SupervisedUserServiceDelegate::
                   ParentPermissionDialogResult::kParentPermissionCanceled);
      break;
    case ParentPermissionDialog::Result::kParentPermissionFailed:
      std::move(delegate_done_callback)
          .Run(extensions::SupervisedUserServiceDelegate::
                   ParentPermissionDialogResult::kParentPermissionFailed);
      break;
  }
}

}  // namespace

namespace extensions {

SupervisedUserServiceManagementAPIDelegate::
    SupervisedUserServiceManagementAPIDelegate() = default;

SupervisedUserServiceManagementAPIDelegate::
    ~SupervisedUserServiceManagementAPIDelegate() = default;

bool SupervisedUserServiceManagementAPIDelegate::
    IsSupervisedChildWhoMayInstallExtensions(
        content::BrowserContext* context) const {
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForBrowserContext(context);

  return supervised_user_service->IsChild() &&
         supervised_user_service->CanInstallExtensions();
}

bool SupervisedUserServiceManagementAPIDelegate::IsExtensionAllowedByParent(
    const extensions::Extension& extension,
    content::BrowserContext* context) const {
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForBrowserContext(context);
  return IsSupervisedChildWhoMayInstallExtensions(context) &&
         supervised_user_service->IsExtensionAllowed(extension);
}

void SupervisedUserServiceManagementAPIDelegate::
    ShowParentPermissionDialogForExtension(
        const extensions::Extension& extension,
        content::BrowserContext* context,
        content::WebContents* contents,
        ParentPermissionDialogDoneCallback done_callback) {
  std::unique_ptr<ParentPermissionDialog> dialog =
      std::make_unique<ParentPermissionDialog>(
          Profile::FromBrowserContext(context));

  // Cache the pointer so we can show the dialog after we pass
  // ownership to the callback.
  ParentPermissionDialog* dialog_ptr = dialog.get();

  // Ownership of the dialog passes to the callback.  This allows us
  // to have as many instances of the dialog as calls to the management
  // API.
  ParentPermissionDialog::DoneCallback inner_done_callback =
      base::BindOnce(&::OnParentPermissionDialogComplete, std::move(dialog),
                     std::move(done_callback));

  // This is safe because moving a unique_ptr doesn't change the underlying
  // object's address.
  dialog_ptr->ShowPromptForExtensionInstallation(
      contents, &extension, SkBitmap(), std::move(inner_done_callback));
}

}  // namespace extensions
