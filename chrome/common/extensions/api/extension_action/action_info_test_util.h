// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_EXTENSION_ACTION_ACTION_INFO_TEST_UTIL_H_
#define CHROME_COMMON_EXTENSIONS_API_EXTENSION_ACTION_ACTION_INFO_TEST_UTIL_H_

#include "chrome/common/extensions/api/extension_action/action_info.h"

namespace extensions {
class Extension;

// Retrieves the manifest key for the given action |type|.
const char* GetManifestKeyForActionType(ActionInfo::Type type);

// Retrieves the ActionInfo for the |extension| if and only if it
// corresponds to the specified |type|. This is useful for testing to ensure
// the type is specified correctly, since most production code is type-
// agnostic.
const ActionInfo* GetActionInfoOfType(const Extension& extension,
                                      ActionInfo::Type type);

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_EXTENSION_ACTION_ACTION_INFO_TEST_UTIL_H_
