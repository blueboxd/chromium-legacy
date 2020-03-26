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
#include "content/public/browser/web_contents.h"

namespace {

void OnParentPermissionDialogComplete(
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
  ParentPermissionDialog::DoneCallback inner_done_callback = base::BindOnce(
      &::OnParentPermissionDialogComplete, std::move(done_callback));

  parent_permission_dialog_ =
      ParentPermissionDialog::CreateParentPermissionDialogForExtension(
          Profile::FromBrowserContext(context), contents,
          contents->GetTopLevelNativeWindow(), gfx::ImageSkia(), &extension,
          std::move(inner_done_callback));
  parent_permission_dialog_->ShowDialog();
}

}  // namespace extensions
