// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_prompt.h"

namespace permissions {

// TODO(crbug.com/1025609): Move the permission prompt implementations into
// //components/permissions. Right now this is used in unit tests to make sure
// the symbol is defined.
std::unique_ptr<PermissionPrompt> PermissionPrompt::Create(
    content::WebContents* web_contents,
    Delegate* delegate) {
  return nullptr;
}

}  // namespace permissions
