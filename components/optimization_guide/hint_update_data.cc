// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/hint_update_data.h"

#include "components/optimization_guide/hint_cache_store.h"
#include "components/optimization_guide/proto/hint_cache.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"

namespace optimization_guide {

// static
std::unique_ptr<HintUpdateData> HintUpdateData::CreateComponentHintUpdateData(
    const base::Version& component_version) {
  std::unique_ptr<HintUpdateData> update_data(new HintUpdateData(
      base::Optional<base::Version>(component_version),
      base::Optional<base::Time>(), base::Optional<base::Time>()));
  return update_data;
}

// static
std::unique_ptr<HintUpdateData> HintUpdateData::CreateFetchedHintUpdateData(
    base::Time fetch_update_time,
    base::Time expiry_time) {
  std::unique_ptr<HintUpdateData> update_data(
      new HintUpdateData(base::Optional<base::Version>(),
                         base::Optional<base::Time>(fetch_update_time),
                         base::Optional<base::Time>(expiry_time)));
  return update_data;
}

HintUpdateData::HintUpdateData(base::Optional<base::Version> component_version,
                               base::Optional<base::Time> fetch_update_time,
                               base::Optional<base::Time> expiry_time)
    : component_version_(component_version),
      fetch_update_time_(fetch_update_time),
      expiry_time_(expiry_time),
      entries_to_save_(std::make_unique<EntryVector>()) {
  DCHECK_NE(!component_version_, !fetch_update_time_);

  if (component_version_.has_value()) {
    hint_entry_key_prefix_ =
        HintCacheStore::GetComponentHintEntryKeyPrefix(*component_version_);

    // Add a component metadata entry for the component's version.
    proto::StoreEntry metadata_component_entry;

    metadata_component_entry.set_entry_type(static_cast<proto::StoreEntryType>(
        HintCacheStore::StoreEntryType::kMetadata));
    metadata_component_entry.set_version(component_version_->GetString());
    entries_to_save_->emplace_back(
        HintCacheStore::GetMetadataTypeEntryKey(
            HintCacheStore::MetadataType::kComponent),
        std::move(metadata_component_entry));
  } else if (fetch_update_time_.has_value()) {
    hint_entry_key_prefix_ = HintCacheStore::GetFetchedHintEntryKeyPrefix();
    proto::StoreEntry metadata_fetched_entry;
    metadata_fetched_entry.set_entry_type(static_cast<proto::StoreEntryType>(
        HintCacheStore::StoreEntryType::kMetadata));
    metadata_fetched_entry.set_update_time_secs(
        fetch_update_time_->ToDeltaSinceWindowsEpoch().InSeconds());
    entries_to_save_->emplace_back(HintCacheStore::GetMetadataTypeEntryKey(
                                       HintCacheStore::MetadataType::kFetched),
                                   std::move(metadata_fetched_entry));
  } else {
    NOTREACHED();
  }
  // |this| may be modified on another thread after construction but all
  // future modifications, from that call forward, must be made on the same
  // thread.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

HintUpdateData::~HintUpdateData() {}

void HintUpdateData::MoveHintIntoUpdateData(proto::Hint&& hint) {
  // All future modifications must be made by the same thread. Note, |this| may
  // have been constructed on another thread.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!hint_entry_key_prefix_.empty());

  // To avoid any unnecessary copying, the hint is moved into proto::StoreEntry.
  HintCacheStore::EntryKey hint_entry_key = hint_entry_key_prefix_ + hint.key();
  proto::StoreEntry entry_proto;
  if (component_version()) {
    entry_proto.set_entry_type(static_cast<proto::StoreEntryType>(
        HintCacheStore::StoreEntryType::kComponentHint));
  } else if (fetch_update_time()) {
    DCHECK(expiry_time());
    entry_proto.set_expiry_time_secs(
        expiry_time_->ToDeltaSinceWindowsEpoch().InSeconds());
    entry_proto.set_entry_type(static_cast<proto::StoreEntryType>(
        HintCacheStore::StoreEntryType::kFetchedHint));
  }
  entry_proto.set_allocated_hint(new proto::Hint(std::move(hint)));
  entries_to_save_->emplace_back(std::move(hint_entry_key),
                                 std::move(entry_proto));
}

std::unique_ptr<EntryVector> HintUpdateData::TakeUpdateEntries() {
  // TakeUpdateEntries is not be sequence checked as it only gives up ownership
  // of the entries_to_save_ and does not modify any state.
  DCHECK(entries_to_save_);

  return std::move(entries_to_save_);
}

}  // namespace optimization_guide
