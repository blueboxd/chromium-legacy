// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/optimization_guide_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace optimization_guide {
namespace prefs {

// A dictionary pref that stores counts for the number of times a hint was
// attempted to be loaded and counts for the number of times a hint was actually
// loaded, broken down by hint source.
const char kHintLoadedCounts[] = "optimization_guide.hint_loaded_counts";

// A pref that stores the last time a hints fetch was attempted. This limits the
// frequency that hints are fetched and prevents a crash loop that continually
// fetches hints on startup.
const char kHintsFetcherLastFetchAttempt[] =
    "optimization_guide.hintsfetcher.last_fetch_attempt";

// A dictionary pref that stores the set of hosts that cannot have hints fetched
// for until visited again after DataSaver was enabled. If The hash of the host
// is in the dictionary, then it is on the blacklist and should not be used, the
// |value| in the key-value pair is not used.
const char kHintsFetcherTopHostBlacklist[] =
    "optimization_guide.hintsfetcher.top_host_blacklist";

// An integer pref that stores the state of the blacklist for the top host
// provider for blacklisting hosts after DataSaver is enabled. The state maps to
// the HintsFetcherTopHostBlacklistState enum.
const char kHintsFetcherTopHostBlacklistState[] =
    "optimization_guide.hintsfetcher.top_host_blacklist_state";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kHintLoadedCounts, PrefRegistry::LOSSY_PREF);
  registry->RegisterInt64Pref(
      kHintsFetcherLastFetchAttempt,
      base::Time().ToDeltaSinceWindowsEpoch().InMicroseconds(),
      PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(kHintsFetcherTopHostBlacklist,
                                   PrefRegistry::LOSSY_PREF);
  registry->RegisterIntegerPref(
      kHintsFetcherTopHostBlacklistState,
      static_cast<int>(HintsFetcherTopHostBlacklistState::kNotInitialized),
      PrefRegistry::LOSSY_PREF);
}

}  // namespace prefs
}  // namespace optimization_guide
