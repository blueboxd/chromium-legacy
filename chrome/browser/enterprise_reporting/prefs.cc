// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise_reporting/prefs.h"

#include "base/feature_list.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"

namespace enterprise_reporting {

const char kLastUploadTimestamp[] =
    "enterprise_reporting.last_upload_timestamp";

void RegisterPrefs(PrefRegistrySimple* registry) {
  // This is also registered as a Profile pref which will be removed after
  // the migration.
  registry->RegisterBooleanPref(prefs::kCloudReportingEnabled, false);
  registry->RegisterTimePref(kLastUploadTimestamp, base::Time());
}

}  // namespace enterprise_reporting
