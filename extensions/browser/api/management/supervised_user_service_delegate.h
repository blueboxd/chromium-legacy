// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_MANAGEMENT_SUPERVISED_USER_SERVICE_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_MANAGEMENT_SUPERVISED_USER_SERVICE_DELEGATE_H_

#include "base/callback.h"
#include "extensions/common/extension.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace extensions {

class SupervisedUserServiceDelegate {
 public:
  virtual ~SupervisedUserServiceDelegate() = default;

  // Returns true if |context| represents a supervised child account
  // who may install extensions with parent permission.
  virtual bool IsSupervisedChildWhoMayInstallExtensions(
      content::BrowserContext* context) const = 0;

  // Returns true if the current child user is allowed to install the specified
  // |extension|.
  virtual bool IsExtensionAllowedByParent(
      const extensions::Extension& extension,
      content::BrowserContext* context) const = 0;

  // Result of the parent permission dialog invocation.
  enum class ParentPermissionDialogResult {
    kParentPermissionReceived,
    kParentPermissionCanceled,
    kParentPermissionFailed,
  };

  using ParentPermissionDialogDoneCallback =
      base::OnceCallback<void(ParentPermissionDialogResult)>;

  // Show a parent permission dialog for |extension| and call |done_callback|
  // when it completes.
  virtual void ShowParentPermissionDialogForExtension(
      const extensions::Extension& extension,
      content::BrowserContext* context,
      content::WebContents* contents,
      ParentPermissionDialogDoneCallback done_callback) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_MANAGEMENT_SUPERVISED_USER_SERVICE_DELEGATE_H_
