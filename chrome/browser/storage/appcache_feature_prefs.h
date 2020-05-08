// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_APPCACHE_FEATURE_PREFS_H_
#define CHROME_BROWSER_STORAGE_APPCACHE_FEATURE_PREFS_H_

class PrefRegistrySimple;

namespace appcache_feature_prefs {

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace appcache_feature_prefs

#endif  // CHROME_BROWSER_STORAGE_APPCACHE_FEATURE_PREFS_H_
