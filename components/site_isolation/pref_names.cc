// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_isolation/pref_names.h"

namespace site_isolation {
namespace prefs {

// A list of origins that were heuristically determined to need process
// isolation. For example, an origin may be placed on this list in response to
// the user typing a password on it.
const char kUserTriggeredIsolatedOrigins[] =
    "site_isolation.user_triggered_isolated_origins";

}  // namespace prefs
}  // namespace site_isolation
