// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/extension_action/action_info_test_util.h"

#include "extensions/common/manifest_constants.h"

namespace extensions {

const char* GetManifestKeyForActionType(ActionInfo::Type type) {
  const char* action_key = nullptr;
  switch (type) {
    case ActionInfo::TYPE_BROWSER:
      action_key = manifest_keys::kBrowserAction;
      break;
    case ActionInfo::TYPE_PAGE:
      action_key = manifest_keys::kPageAction;
      break;
    case ActionInfo::TYPE_ACTION:
      action_key = manifest_keys::kAction;
      break;
  }

  return action_key;
}

const ActionInfo* GetActionInfoOfType(const Extension& extension,
                                      ActionInfo::Type type) {
  const ActionInfo* action_info = ActionInfo::GetAnyActionInfo(&extension);
  return (action_info && action_info->type == type) ? action_info : nullptr;
}

}  // namespace extensions
