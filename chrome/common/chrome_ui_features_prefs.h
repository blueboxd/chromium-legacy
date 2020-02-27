// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_UI_FEATURES_PREFS_H_
#define CHROME_COMMON_CHROME_UI_FEATURES_PREFS_H_

class PrefRegistrySimple;

namespace chrome_ui_features_prefs {

// Register preferences for Chrome UI Features.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace chrome_ui_features_prefs

#endif  // CHROME_COMMON_CHROME_UI_FEATURES_PREFS_H_
