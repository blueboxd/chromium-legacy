// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/tile_service_prefs.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace query_tiles {

constexpr char kBackoffEntryKey[] = "query_tiles.backoff_entry_key";

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kBackoffEntryKey);
}
}  // namespace query_tiles
