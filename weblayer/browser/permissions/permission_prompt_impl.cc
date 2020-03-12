// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "components/permissions/permission_prompt.h"

namespace permissions {

#if !defined(OS_ANDROID)
// TODO(crbug.com/1025609): //components/permissions does not have a
// PermissionPrompt::Create() implementation for desktop platforms, so we define
// one here.
std::unique_ptr<PermissionPrompt> PermissionPrompt::Create(
    content::WebContents* web_contents,
    Delegate* delegate) {
  return nullptr;
}
#endif

}  // namespace permissions
