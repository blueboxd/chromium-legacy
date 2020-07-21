// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/suggestions/blocklist_store.h"

#include <stddef.h>

#include <algorithm>
#include <set>
#include <string>

#include "base/base64.h"
#include "base/logging.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/suggestions/suggestions_pref_names.h"

using base::TimeDelta;
using base::TimeTicks;

namespace suggestions {
namespace {

void PopulateBlocklistSet(const SuggestionsBlocklist& blocklist_proto,
                          std::set<std::string>* blocklist_set) {
  blocklist_set->clear();
  for (int i = 0; i < blocklist_proto.urls_size(); ++i) {
    blocklist_set->insert(blocklist_proto.urls(i));
  }
}

void PopulateBlocklistProto(const std::set<std::string>& blocklist_set,
                            SuggestionsBlocklist* blocklist_proto) {
  blocklist_proto->Clear();
  for (auto& url : blocklist_set) {
    blocklist_proto->add_urls(url);
  }
}

}  // namespace

BlocklistStore::BlocklistStore(PrefService* profile_prefs,
                               const base::TimeDelta& upload_delay)
    : pref_service_(profile_prefs), upload_delay_(upload_delay) {
  DCHECK(pref_service_);
}

BlocklistStore::~BlocklistStore() = default;

bool BlocklistStore::BlocklistUrl(const GURL& url) {
  if (!url.is_valid())
    return false;

  SuggestionsBlocklist blocklist_proto;
  LoadBlocklist(&blocklist_proto);
  std::set<std::string> blocklist_set;
  PopulateBlocklistSet(blocklist_proto, &blocklist_set);

  bool success = false;
  if (blocklist_set.insert(url.spec()).second) {
    PopulateBlocklistProto(blocklist_set, &blocklist_proto);
    success = StoreBlocklist(blocklist_proto);
  } else {
    // |url| was already in the blocklist.
    success = true;
  }

  if (success) {
    // Update the blocklist time.
    blocklist_times_[url.spec()] = TimeTicks::Now();
  }

  return success;
}

bool BlocklistStore::GetTimeUntilReadyForUpload(TimeDelta* delta) {
  SuggestionsBlocklist blocklist;
  LoadBlocklist(&blocklist);
  if (!blocklist.urls_size())
    return false;

  // Note: the size is non-negative.
  if (blocklist_times_.size() < static_cast<size_t>(blocklist.urls_size())) {
    // A url is not in the timestamp map: it's candidate for upload. This can
    // happen after a restart. Another (undesired) case when this could happen
    // is if more than one instance were created.
    *delta = TimeDelta::FromSeconds(0);
    return true;
  }

  // Find the minimum blocklist time. Note: blocklist_times_ is NOT empty since
  // blocklist is non-empty and blocklist_times_ contains as many items.
  TimeDelta min_delay = TimeDelta::Max();
  for (const auto& kv : blocklist_times_) {
    min_delay =
        std::min(upload_delay_ - (TimeTicks::Now() - kv.second), min_delay);
  }
  DCHECK(min_delay != TimeDelta::Max());
  *delta = std::max(min_delay, TimeDelta::FromSeconds(0));

  return true;
}

bool BlocklistStore::GetTimeUntilURLReadyForUpload(const GURL& url,
                                                   TimeDelta* delta) {
  auto it = blocklist_times_.find(url.spec());
  if (it != blocklist_times_.end()) {
    // The url is in the timestamps map.
    *delta = std::max(upload_delay_ - (TimeTicks::Now() - it->second),
                      TimeDelta::FromSeconds(0));
    return true;
  }

  // The url still might be in the blocklist.
  SuggestionsBlocklist blocklist;
  LoadBlocklist(&blocklist);
  for (int i = 0; i < blocklist.urls_size(); ++i) {
    if (blocklist.urls(i) == url.spec()) {
      *delta = TimeDelta::FromSeconds(0);
      return true;
    }
  }

  return false;
}

bool BlocklistStore::GetCandidateForUpload(GURL* url) {
  SuggestionsBlocklist blocklist;
  LoadBlocklist(&blocklist);

  for (int i = 0; i < blocklist.urls_size(); ++i) {
    bool is_candidate = true;
    auto it = blocklist_times_.find(blocklist.urls(i));
    if (it != blocklist_times_.end() &&
        TimeTicks::Now() < it->second + upload_delay_) {
      // URL was added too recently.
      is_candidate = false;
    }
    if (is_candidate) {
      GURL blocklisted(blocklist.urls(i));
      url->Swap(&blocklisted);
      return true;
    }
  }

  return false;
}

bool BlocklistStore::RemoveUrl(const GURL& url) {
  if (!url.is_valid())
    return false;
  const std::string removal_candidate = url.spec();

  SuggestionsBlocklist blocklist;
  LoadBlocklist(&blocklist);

  bool removed = false;
  SuggestionsBlocklist updated_blocklist;
  for (int i = 0; i < blocklist.urls_size(); ++i) {
    if (blocklist.urls(i) == removal_candidate) {
      removed = true;
    } else {
      updated_blocklist.add_urls(blocklist.urls(i));
    }
  }

  if (removed && StoreBlocklist(updated_blocklist)) {
    blocklist_times_.erase(url.spec());
    return true;
  }

  return false;
}

void BlocklistStore::FilterSuggestions(SuggestionsProfile* profile) {
  if (!profile->suggestions_size())
    return;  // Empty profile, nothing to filter.

  SuggestionsBlocklist blocklist_proto;
  if (!LoadBlocklist(&blocklist_proto)) {
    // There was an error loading the blocklist. The blocklist was cleared and
    // there's nothing to be done about it.
    return;
  }
  if (!blocklist_proto.urls_size())
    return;  // Empty blocklist, nothing to filter.

  std::set<std::string> blocklist_set;
  PopulateBlocklistSet(blocklist_proto, &blocklist_set);

  // Populate the filtered suggestions.
  SuggestionsProfile filtered_profile;
  filtered_profile.set_timestamp(profile->timestamp());
  for (int i = 0; i < profile->suggestions_size(); ++i) {
    if (blocklist_set.find(profile->suggestions(i).url()) ==
        blocklist_set.end()) {
      // This suggestion is not blocklisted.
      ChromeSuggestion* suggestion = filtered_profile.add_suggestions();
      // Note: swapping!
      suggestion->Swap(profile->mutable_suggestions(i));
    }
  }

  // Swap |profile| and |filtered_profile|.
  profile->Swap(&filtered_profile);
}

// static
void BlocklistStore::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kSuggestionsBlocklist, std::string());
}

// Test seam. For simplicity of mock creation.
BlocklistStore::BlocklistStore() = default;

bool BlocklistStore::LoadBlocklist(SuggestionsBlocklist* blocklist) {
  DCHECK(blocklist);

  const std::string base64_blocklist_data =
      pref_service_->GetString(prefs::kSuggestionsBlocklist);
  if (base64_blocklist_data.empty()) {
    blocklist->Clear();
    return false;
  }

  // If the decode process fails, assume the pref value is corrupt and clear it.
  std::string blocklist_data;
  if (!base::Base64Decode(base64_blocklist_data, &blocklist_data) ||
      !blocklist->ParseFromString(blocklist_data)) {
    VLOG(1) << "Suggestions blocklist data in profile pref is corrupt, "
            << " clearing it.";
    blocklist->Clear();
    ClearBlocklist();
    return false;
  }

  return true;
}

bool BlocklistStore::StoreBlocklist(const SuggestionsBlocklist& blocklist) {
  std::string blocklist_data;
  if (!blocklist.SerializeToString(&blocklist_data))
    return false;

  std::string base64_blocklist_data;
  base::Base64Encode(blocklist_data, &base64_blocklist_data);

  pref_service_->SetString(prefs::kSuggestionsBlocklist, base64_blocklist_data);
  return true;
}

void BlocklistStore::ClearBlocklist() {
  pref_service_->ClearPref(prefs::kSuggestionsBlocklist);
}

}  // namespace suggestions
