// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SERVICE_MANAGEMENT_API_DELEGATE_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SERVICE_MANAGEMENT_API_DELEGATE_H_

#include "extensions/browser/api/management/supervised_user_service_delegate.h"

namespace content {
class BrowserContext;
}

class ParentPermissionDialog;

namespace extensions {

class SupervisedUserServiceManagementAPIDelegate
    : public extensions::SupervisedUserServiceDelegate {
 public:
  SupervisedUserServiceManagementAPIDelegate();
  ~SupervisedUserServiceManagementAPIDelegate() override;

  // extensions::SupervisedUserServiceDelegate overrides

  bool IsSupervisedChildWhoMayInstallExtensions(
      content::BrowserContext* context) const override;

  bool IsExtensionAllowedByParent(
      const extensions::Extension& extension,
      content::BrowserContext* context) const override;

  void ShowParentPermissionDialogForExtension(
      const extensions::Extension& extension,
      content::BrowserContext* context,
      content::WebContents* contents,
      extensions::SupervisedUserServiceDelegate::
          ParentPermissionDialogDoneCallback done_callback) override;

 private:
  std::unique_ptr<ParentPermissionDialog> parent_permission_dialog_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SERVICE_MANAGEMENT_API_DELEGATE_H_
