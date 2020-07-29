// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_COMMON_NEARBY_SHARE_PREFS_H_
#define CHROME_BROWSER_NEARBY_SHARING_COMMON_NEARBY_SHARE_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class PrefRegistrySimple;

namespace prefs {
extern const char kNearbySharingEnabledPrefName[];
extern const char kNearbySharingActiveProfilePrefName[];
extern const char kNearbySharingBackgroundVisibilityName[];
extern const char kNearbySharingDataUsageName[];
extern const char kNearbySharingDeviceIdPrefName[];
extern const char kNearbySharingDeviceNamePrefName[];
extern const char kNearbySharingFullNamePrefName[];
extern const char kNearbySharingIconUrlPrefName[];
extern const char kNearbySharingSchedulerDownloadDeviceDataPrefName[];
extern const char kNearbySharingSchedulerUploadDeviceNamePrefName[];
}  // namespace prefs

void RegisterNearbySharingPrefs(user_prefs::PrefRegistrySyncable* registry);

void RegisterNearbySharingLocalPrefs(PrefRegistrySimple* local_state);

#endif  // CHROME_BROWSER_NEARBY_SHARING_COMMON_NEARBY_SHARE_PREFS_H_
