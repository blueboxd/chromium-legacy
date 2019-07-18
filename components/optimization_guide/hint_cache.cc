// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/hint_cache.h"

#include <algorithm>

#include "base/bind.h"
#include "components/optimization_guide/hint_update_data.h"
#include "components/optimization_guide/hints_processing_util.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "url/gurl.h"

namespace optimization_guide {

namespace {

// The default number of hints retained within the memory cache. When the limit
// is exceeded, the least recently used hint is purged from the cache.
const size_t kDefaultMaxMemoryCacheHints = 20;

}  // namespace

HintCache::HintCache(
    std::unique_ptr<HintCacheStore> hint_store,
    base::Optional<int> max_memory_cache_hints /*= base::Optional<int>()*/)
    : hint_store_(std::move(hint_store)),
      memory_cache_(
          std::max(max_memory_cache_hints.value_or(kDefaultMaxMemoryCacheHints),
                   1)) {
  DCHECK(hint_store_);
}

HintCache::~HintCache() = default;

void HintCache::Initialize(bool purge_existing_data,
                           base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  hint_store_->Initialize(
      purge_existing_data,
      base::BindOnce(&HintCache::OnStoreInitialized, base::Unretained(this),
                     std::move(callback)));
}

std::unique_ptr<HintUpdateData>
HintCache::MaybeCreateUpdateDataForComponentHints(
    const base::Version& version) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return hint_store_->MaybeCreateUpdateDataForComponentHints(version);
}

std::unique_ptr<HintUpdateData> HintCache::CreateUpdateDataForFetchedHints(
    base::Time update_time,
    base::Time expiry_time) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return hint_store_->CreateUpdateDataForFetchedHints(update_time, expiry_time);
}

void HintCache::UpdateComponentHints(
    std::unique_ptr<HintUpdateData> component_data,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(component_data);

  // Clear the memory cache prior to updating the store with the new component
  // data.
  memory_cache_.Clear();

  hint_store_->UpdateComponentHints(std::move(component_data),
                                    std::move(callback));
}

void HintCache::UpdateFetchedHints(
    std::unique_ptr<proto::GetHintsResponse> get_hints_response,
    base::Time update_time,
    base::OnceClosure callback) {
  base::Time expiry_time = update_time;
  if (get_hints_response->has_max_cache_duration()) {
    expiry_time += base::TimeDelta().FromSeconds(
        get_hints_response->max_cache_duration().seconds());
  } else {
    expiry_time += features::StoredFetchedHintsFreshnessDuration();
  }
  std::unique_ptr<HintUpdateData> fetched_hints_update_data =
      CreateUpdateDataForFetchedHints(update_time, expiry_time);
  ProcessHints(get_hints_response.get()->mutable_hints(),
               fetched_hints_update_data.get());
  hint_store_->UpdateFetchedHints(std::move(fetched_hints_update_data),
                                  std::move(callback));
}

void HintCache::ClearFetchedHints() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(hint_store_);
  // TODO(mcrouse): Update to remove only fetched hints from |memory_cache_|.
  memory_cache_.Clear();
  hint_store_->ClearFetchedHintsFromDatabase();
}

bool HintCache::HasHint(const std::string& host) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HintCacheStore::EntryKey hint_entry_key;
  return hint_store_->FindHintEntryKey(host, &hint_entry_key);
}

void HintCache::LoadHint(const std::string& host, HintLoadedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  HintCacheStore::EntryKey hint_entry_key;
  if (!hint_store_->FindHintEntryKey(host, &hint_entry_key)) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Search for the entry key in the memory cache; if it is not already there,
  // then asynchronously load it from the store and return.
  auto hint_it = memory_cache_.Get(hint_entry_key);
  if (hint_it == memory_cache_.end()) {
    hint_store_->LoadHint(
        hint_entry_key,
        base::BindOnce(&HintCache::OnLoadStoreHint, base::Unretained(this),
                       std::move(callback)));
    return;
  }

  // Run the callback with the hint from the memory cache.
  std::move(callback).Run(hint_it->second.get());
}

const proto::Hint* HintCache::GetHintIfLoaded(const std::string& host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Try to retrieve the hint entry key for the host. If no hint exists for the
  // host, then simply return.
  HintCacheStore::EntryKey hint_entry_key;
  if (!hint_store_->FindHintEntryKey(host, &hint_entry_key)) {
    return nullptr;
  }

  // Find the hint within the memory cache. It will only be available here if
  // it has been loaded recently enough to be retained within the MRU cache.
  auto hint_it = memory_cache_.Get(hint_entry_key);
  if (hint_it != memory_cache_.end()) {
    return hint_it->second.get();
  }

  return nullptr;
}

base::Time HintCache::FetchedHintsUpdateTime() const {
  if (!hint_store_) {
    return base::Time();
  }
  return hint_store_->FetchedHintsUpdateTime();
}

void HintCache::OnStoreInitialized(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run();
}

void HintCache::OnLoadStoreHint(HintLoadedCallback callback,
                                const HintCacheStore::EntryKey& hint_entry_key,
                                std::unique_ptr<proto::Hint> hint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!hint) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Check if the hint was cached in memory after the load was requested from
  // the store. This can occur if multiple loads for the same entry key occur
  // consecutively prior to any returning.
  auto hint_it = memory_cache_.Get(hint_entry_key);
  if (hint_it == memory_cache_.end()) {
    hint_it = memory_cache_.Put(hint_entry_key, std::move(hint));
  }

  std::move(callback).Run(hint_it->second.get());
}

}  // namespace optimization_guide
