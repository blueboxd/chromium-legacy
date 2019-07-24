// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_PREFS_H_
#define COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_PREFS_H_

#include <string>
#include <vector>

#include "base/macros.h"

class PrefService;
class PrefRegistrySimple;

namespace syncer {

// Use this for determining if a cache guid was recently used by this device.
class DeviceInfoPrefs {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit DeviceInfoPrefs(PrefService* pref_service);
  ~DeviceInfoPrefs();

  bool IsRecentLocalCacheGuid(const std::string& cache_guid) const;
  void AddLocalCacheGuid(const std::string& cache_guid);

 private:
  PrefService* const pref_service_;

  DISALLOW_COPY_AND_ASSIGN(DeviceInfoPrefs);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_PREFS_H_
