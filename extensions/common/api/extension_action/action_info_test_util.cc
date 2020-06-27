// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/extension_action/action_info_test_util.h"

#include "components/version_info/channel.h"
#include "extensions/common/features/feature_channel.h"
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

const char* GetAPINameForActionType(ActionInfo::Type action_type) {
  const char* api_name = nullptr;
  switch (action_type) {
    case ActionInfo::TYPE_BROWSER:
      api_name = "browserAction";
      break;
    case ActionInfo::TYPE_PAGE:
      api_name = "pageAction";
      break;
    case ActionInfo::TYPE_ACTION:
      api_name = "action";
      break;
  }

  return api_name;
}

const ActionInfo* GetActionInfoOfType(const Extension& extension,
                                      ActionInfo::Type type) {
  const ActionInfo* action_info =
      ActionInfo::GetExtensionActionInfo(&extension);
  return (action_info && action_info->type == type) ? action_info : nullptr;
}

std::unique_ptr<ScopedCurrentChannel> GetOverrideChannelForActionType(
    ActionInfo::Type action_type) {
  std::unique_ptr<ScopedCurrentChannel> channel;
  // The "action" key is currently restricted to canary. Use a fake channel iff
  // it would be restricted otherwise. This way, we still get all-channel
  // coverage for browser and page actions, and cover all channels that "action"
  // is supported in.
  constexpr version_info::Channel kMaxChannelForActionKey =
      version_info::Channel::CANARY;

  switch (action_type) {
    case ActionInfo::TYPE_ACTION:
      if (GetCurrentChannel() > kMaxChannelForActionKey) {
        channel =
            std::make_unique<ScopedCurrentChannel>(kMaxChannelForActionKey);
      }
      break;
    case ActionInfo::TYPE_PAGE:
    case ActionInfo::TYPE_BROWSER:
      break;
  }
  return channel;
}

}  // namespace extensions
