// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_DELEGATE_IMPL_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_DELEGATE_IMPL_H_

#include "extensions/browser/supervised_user_extensions_delegate.h"

namespace content {
class BrowserContext;
}

class ParentPermissionDialog;

namespace extensions {

class SupervisedUserExtensionsDelegateImpl
    : public extensions::SupervisedUserExtensionsDelegate {
 public:
  SupervisedUserExtensionsDelegateImpl();
  ~SupervisedUserExtensionsDelegateImpl() override;

  // extensions::SupervisedUserExtensionsDelegate overrides
  bool IsChild(content::BrowserContext* context) const override;

  bool IsSupervisedChildWhoMayInstallExtensions(
      content::BrowserContext* context) const override;

  bool IsExtensionAllowedByParent(
      const extensions::Extension& extension,
      content::BrowserContext* context) const override;

  void ShowParentPermissionDialogForExtension(
      const extensions::Extension& extension,
      content::BrowserContext* context,
      content::WebContents* contents,
      extensions::SupervisedUserExtensionsDelegate::
          ParentPermissionDialogDoneCallback done_callback) override;

  void ShowExtensionEnableBlockedByParentDialogForExtension(
      const extensions::Extension* extension,
      content::WebContents* contents,
      base::OnceClosure done_callback) override;

  void RecordExtensionEnableBlockedByParentDialogUmaMetric() override;

 private:
  std::unique_ptr<ParentPermissionDialog> parent_permission_dialog_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_DELEGATE_IMPL_H_
