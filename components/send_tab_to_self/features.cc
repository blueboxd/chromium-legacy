// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/features.h"

#include "components/sync/base/sync_prefs.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_driver_switches.h"

namespace send_tab_to_self {

const base::Feature kSendTabToSelfShowSendingUI{
    "SendTabToSelfShowSendingUI", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSendTabToSelfBroadcast{"SendTabToSelfBroadcast",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSendTabToSelfWhenSignedIn{
    "SendTabToSelfWhenSignedIn", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsReceivingEnabledByUserOnThisDevice(PrefService* prefs) {
  syncer::SyncPrefs sync_prefs(prefs);
  return base::FeatureList::IsEnabled(switches::kSyncSendTabToSelf) &&
         sync_prefs.GetSelectedTypes().Has(syncer::UserSelectableType::kTabs);
}

bool EnabledOnSignIn() {
  return base::FeatureList::IsEnabled(switches::kSyncSendTabToSelf) &&
         base::FeatureList::IsEnabled(kSendTabToSelfWhenSignedIn);
}

}  // namespace send_tab_to_self
