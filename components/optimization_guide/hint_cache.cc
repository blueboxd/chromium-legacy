// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/hint_cache.h"

#include <algorithm>

#include "base/bind.h"
#include "base/time/default_clock.h"
#include "components/optimization_guide/hints_processing_util.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/store_update_data.h"
#include "url/gurl.h"

namespace optimization_guide {

namespace {

// The default number of host-keyed hints retained within the host keyed cache.
// When the limit is exceeded, the least recently used hint is purged from
// |host_keyed_cache_|.
const size_t kDefaultMaxMemoryCacheHostKeyedHints = 20;

}  // namespace

HintCache::HintCache(
    std::unique_ptr<OptimizationGuideStore> optimization_guide_store,
    base::Optional<int>
        max_memory_cache_host_keyed_hints /*= base::Optional<int>()*/)
    : optimization_guide_store_(std::move(optimization_guide_store)),
      host_keyed_cache_(std::max(max_memory_cache_host_keyed_hints.value_or(
                                     kDefaultMaxMemoryCacheHostKeyedHints),
                                 1)),
      url_keyed_hint_cache_(features::MaxURLKeyedHintCacheSize()),
      clock_(base::DefaultClock::GetInstance()) {
  DCHECK(optimization_guide_store_);
}

HintCache::~HintCache() = default;

void HintCache::Initialize(bool purge_existing_data,
                           base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  optimization_guide_store_->Initialize(
      purge_existing_data,
      base::BindOnce(&HintCache::OnStoreInitialized, base::Unretained(this),
                     std::move(callback)));
}

std::unique_ptr<StoreUpdateData>
HintCache::MaybeCreateUpdateDataForComponentHints(
    const base::Version& version) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return optimization_guide_store_->MaybeCreateUpdateDataForComponentHints(
      version);
}

std::unique_ptr<StoreUpdateData> HintCache::CreateUpdateDataForFetchedHints(
    base::Time update_time) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return optimization_guide_store_->CreateUpdateDataForFetchedHints(
      update_time);
}

void HintCache::UpdateComponentHints(
    std::unique_ptr<StoreUpdateData> component_data,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(component_data);

  // Clear the host-keyed cache prior to updating the store with the new
  // component data.
  host_keyed_cache_.Clear();

  optimization_guide_store_->UpdateComponentHints(std::move(component_data),
                                                  std::move(callback));
}

void HintCache::UpdateFetchedHints(
    std::unique_ptr<proto::GetHintsResponse> get_hints_response,
    base::Time update_time,
    const base::flat_set<GURL>& urls_fetched,
    base::OnceClosure callback) {
  std::unique_ptr<StoreUpdateData> fetched_hints_update_data =
      CreateUpdateDataForFetchedHints(update_time);

  for (const GURL& url : urls_fetched) {
    if (IsValidURLForURLKeyedHint(url))
      url_keyed_hint_cache_.Put(url.spec(), nullptr);
  }

  ProcessAndCacheHints(get_hints_response.get()->mutable_hints(),
                       fetched_hints_update_data.get());
  optimization_guide_store_->UpdateFetchedHints(
      std::move(fetched_hints_update_data), std::move(callback));
}

void HintCache::PurgeExpiredFetchedHints() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(optimization_guide_store_);

  optimization_guide_store_->PurgeExpiredFetchedHints();
}

void HintCache::ClearFetchedHints() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(optimization_guide_store_);
  // TODO(mcrouse): Update to remove only fetched hints from
  // |host_keyed_cache_|.
  host_keyed_cache_.Clear();
  url_keyed_hint_cache_.Clear();
  optimization_guide_store_->ClearFetchedHintsFromDatabase();
}

bool HintCache::HasHint(const std::string& host) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OptimizationGuideStore::EntryKey hint_entry_key;
  return optimization_guide_store_->FindHintEntryKey(host, &hint_entry_key);
}

void HintCache::LoadHint(const std::string& host, HintLoadedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  OptimizationGuideStore::EntryKey hint_entry_key;
  if (!optimization_guide_store_->FindHintEntryKey(host, &hint_entry_key)) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Search for the entry key in the host-keyed cache; if it is not already
  // there, then asynchronously load it from the store and return.
  auto hint_it = host_keyed_cache_.Get(hint_entry_key);
  if (hint_it == host_keyed_cache_.end()) {
    optimization_guide_store_->LoadHint(
        hint_entry_key,
        base::BindOnce(&HintCache::OnLoadStoreHint, base::Unretained(this),
                       std::move(callback)));
    return;
  }

  // Run the callback with the hint from the host-keyed cache.
  std::move(callback).Run(hint_it->second.get()->hint());
}

const proto::Hint* HintCache::GetHostKeyedHintIfLoaded(
    const std::string& host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Try to retrieve the hint entry key for the host. If no hint exists for the
  // host, then simply return.
  OptimizationGuideStore::EntryKey hint_entry_key;
  if (!optimization_guide_store_->FindHintEntryKey(host, &hint_entry_key))
    return nullptr;

  // Find the hint within the host-keyed cache. It will only be available here
  // if it has been loaded recently enough to be retained within the MRU cache.
  auto hint_it = host_keyed_cache_.Get(hint_entry_key);
  if (hint_it == host_keyed_cache_.end())
    return nullptr;

  MemoryHint* hint = hint_it->second.get();
  // Make sure the hint is not expired before propagating it up.
  if (hint->expiry_time().has_value() && *hint->expiry_time() < clock_->Now()) {
    // The hint is expired so remove it from the cache.
    host_keyed_cache_.Erase(hint_it);
    return nullptr;
  }

  return hint->hint();
}

proto::Hint* HintCache::GetURLKeyedHint(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsValidURLForURLKeyedHint(url))
    return nullptr;

  auto hint_it = url_keyed_hint_cache_.Get(url.spec());
  if (hint_it == url_keyed_hint_cache_.end())
    return nullptr;

  if (!hint_it->second)
    return nullptr;

  MemoryHint* hint = hint_it->second.get();
  DCHECK(hint->expiry_time().has_value());
  if (*hint->expiry_time() > clock_->Now())
    return hint->hint();

  // The hint is expired so remove it from the cache.
  url_keyed_hint_cache_.Erase(hint_it);
  return nullptr;
}

bool HintCache::HasURLKeyedEntryForURL(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsValidURLForURLKeyedHint(url))
    return false;

  auto hint_it = url_keyed_hint_cache_.Get(url.spec());
  if (hint_it == url_keyed_hint_cache_.end())
    return false;

  // The url-keyed hint for the URL was requested but no hint was returned so
  // return true.
  if (!hint_it->second)
    return true;

  MemoryHint* hint = hint_it->second.get();
  DCHECK(hint->expiry_time().has_value());
  if (*hint->expiry_time() > clock_->Now())
    return true;

  // The hint is expired so remove it from the cache.
  url_keyed_hint_cache_.Erase(hint_it);
  return false;
}

base::Time HintCache::GetFetchedHintsUpdateTime() const {
  if (!optimization_guide_store_) {
    return base::Time();
  }
  return optimization_guide_store_->GetFetchedHintsUpdateTime();
}

void HintCache::OnStoreInitialized(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run();
}

void HintCache::OnLoadStoreHint(
    HintLoadedCallback callback,
    const OptimizationGuideStore::EntryKey& hint_entry_key,
    std::unique_ptr<MemoryHint> hint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!hint) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Check if the hint was cached in the host-keyed cache after the load was
  // requested from the store. This can occur if multiple loads for the same
  // entry key occur consecutively prior to any returning.
  auto hint_it = host_keyed_cache_.Get(hint_entry_key);
  if (hint_it == host_keyed_cache_.end()) {
    hint_it = host_keyed_cache_.Put(hint_entry_key, std::move(hint));
  }

  std::move(callback).Run(hint_it->second.get()->hint());
}

bool HintCache::ProcessAndCacheHints(
    google::protobuf::RepeatedPtrField<proto::Hint>* hints,
    optimization_guide::StoreUpdateData* update_data) {
  // If there's no update data, then there's nothing to do.
  if (!update_data)
    return false;
  bool processed_hints_to_store = false;
  // Process each hint in the the hint configuration. The hints are mutable
  // because once processing is completed on each individual hint, it is moved
  // into the component update data. This eliminates the need to make any
  // additional copies of the hints.
  for (auto& hint : *hints) {
    const std::string& hint_key = hint.key();
    // Validate configuration keys.
    DCHECK(!hint_key.empty());
    if (hint_key.empty())
      continue;

    if (hint.page_hints().empty())
      continue;

    base::Time expiry_time =
        hint.has_max_cache_duration()
            ? clock_->Now() + base::TimeDelta().FromSeconds(
                                  hint.max_cache_duration().seconds())
            : clock_->Now() + features::URLKeyedHintValidCacheDuration();

    switch (hint.key_representation()) {
      case proto::HOST_SUFFIX:
        update_data->MoveHintIntoUpdateData(std::move(hint));
        processed_hints_to_store = true;
        break;
      case proto::FULL_URL:
        if (IsValidURLForURLKeyedHint(GURL(hint_key))) {
          url_keyed_hint_cache_.Put(
              hint_key,
              std::make_unique<MemoryHint>(expiry_time, std::move(hint)));
        }
        break;
      case proto::REPRESENTATION_UNSPECIFIED:
        NOTREACHED();
        break;
    }
  }
  return processed_hints_to_store;
}

void HintCache::SetClockForTesting(const base::Clock* clock) {
  clock_ = clock;
}

void HintCache::AddHintForTesting(const GURL& url,
                                  std::unique_ptr<proto::Hint> hint) {
  if (IsValidURLForURLKeyedHint(url)) {
    url_keyed_hint_cache_.Put(
        url.spec(),
        std::make_unique<MemoryHint>(
            base::Time::Now() + base::TimeDelta::FromDays(7), std::move(hint)));
  }
}

}  // namespace optimization_guide
