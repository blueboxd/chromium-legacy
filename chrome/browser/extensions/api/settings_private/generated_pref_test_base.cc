// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/generated_pref_test_base.h"

namespace extensions {
namespace settings_private {

void TestGeneratedPrefObserver::OnGeneratedPrefChanged(
    const std::string& pref_name) {
  updated_pref_name_ = pref_name;
}

}  // namespace settings_private
}  // namespace extensions
