// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_EXTENSION_ACTION_ACTION_INFO_TEST_UTIL_H_
#define CHROME_COMMON_EXTENSIONS_API_EXTENSION_ACTION_ACTION_INFO_TEST_UTIL_H_

#include <memory>

#include "extensions/common/api/extension_action/action_info.h"

namespace extensions {
class Extension;
class ScopedCurrentChannel;

// Retrieves the manifest key for the given action |type|.
const char* GetManifestKeyForActionType(ActionInfo::Type type);

// Given an |action_type|, returns the corresponding API name to be referenced
// from JavaScript.
const char* GetAPINameForActionType(ActionInfo::Type action_type);

// Retrieves the ActionInfo for the |extension| if and only if it
// corresponds to the specified |type|. This is useful for testing to ensure
// the type is specified correctly, since most production code is type-
// agnostic.
const ActionInfo* GetActionInfoOfType(const Extension& extension,
                                      ActionInfo::Type type);

// Returns a ScopedCurrentChannel object to use in tests if one is necessary for
// the given |action_type| specified in the manifest. This will only return
// non-null if the "action" manifest key is used.
// TODO(https://crbug.com/893373): Remove this once the "action" key is launched
// to stable.
std::unique_ptr<ScopedCurrentChannel> GetOverrideChannelForActionType(
    ActionInfo::Type action_type);

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_EXTENSION_ACTION_ACTION_INFO_TEST_UTIL_H_
