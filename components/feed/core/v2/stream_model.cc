// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/stream_model.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/content_id.pb.h"
#include "components/feed/core/v2/stream_model_update_request.h"

namespace feed {
namespace {
bool HasClearAll(const std::vector<feedstore::StreamStructure>& structures) {
  for (const feedstore::StreamStructure& data : structures) {
    if (data.operation() == feedstore::StreamStructure::CLEAR_ALL)
      return true;
  }
  return false;
}
}  // namespace
StreamModel::UiUpdate::UiUpdate() = default;
StreamModel::UiUpdate::~UiUpdate() = default;
StreamModel::UiUpdate::UiUpdate(const UiUpdate&) = default;
StreamModel::UiUpdate& StreamModel::UiUpdate::operator=(const UiUpdate&) =
    default;
StreamModel::StoreUpdate::StoreUpdate() = default;
StreamModel::StoreUpdate::~StoreUpdate() = default;
StreamModel::StoreUpdate::StoreUpdate(const StoreUpdate&) = default;
StreamModel::StoreUpdate& StreamModel::StoreUpdate::operator=(
    const StoreUpdate&) = default;
StreamModel::StoreUpdate::StoreUpdate(StoreUpdate&&) = default;
StreamModel::StoreUpdate& StreamModel::StoreUpdate::operator=(StoreUpdate&&) =
    default;

StreamModel::StreamModel() = default;
StreamModel::~StreamModel() = default;

void StreamModel::SetStoreObserver(StoreObserver* store_observer) {
  DCHECK(!store_observer || !store_observer_)
      << "Attempting to set store_observer multiple times";
  store_observer_ = store_observer;
}

void StreamModel::SetObserver(Observer* observer) {
  DCHECK(!observer || !observer_)
      << "Attempting to set the observer multiple times";
  observer_ = observer;
}

const feedstore::Content* StreamModel::FindContent(
    ContentRevision revision) const {
  return GetFinalFeatureTree()->FindContent(revision);
}
const std::string* StreamModel::FindSharedStateData(
    const std::string& id) const {
  auto iter = shared_states_.find(id);
  if (iter != shared_states_.end()) {
    return &iter->second.data;
  }
  return nullptr;
}

std::vector<std::string> StreamModel::GetSharedStateIds() const {
  std::vector<std::string> ids;
  for (auto& entry : shared_states_) {
    ids.push_back(entry.first);
  }
  return ids;
}

void StreamModel::Update(
    std::unique_ptr<StreamModelUpdateRequest> update_request) {
  feedstore::StreamData& stream_data = update_request->stream_data;
  std::vector<feedstore::StreamStructure>& stream_structures =
      update_request->stream_structures;
  if (HasClearAll(stream_structures)) {
    shared_states_.clear();
  }

  // Update the feature tree.
  for (const feedstore::StreamStructure& structure : stream_structures) {
    base_feature_tree_.ApplyStreamStructure(structure);
  }
  for (feedstore::Content& content : update_request->content) {
    base_feature_tree_.AddContent(std::move(content));
  }

  // Update non-tree data.
  next_page_token_ = stream_data.next_page_token();
  last_added_time_ =
      base::Time::UnixEpoch() +
      base::TimeDelta::FromMilliseconds(stream_data.last_added_time_millis());
  consistency_token_ = stream_data.consistency_token();

  for (feedstore::StreamSharedState& shared_state :
       update_request->shared_states) {
    std::string id = ContentIdString(shared_state.content_id());
    if (!shared_states_.contains(id)) {
      shared_states_[id].data =
          std::move(*shared_state.mutable_shared_state_data());
    }
  }

  // Set next_structure_sequence_number_ when doing the initial load.
  if (update_request->source ==
      StreamModelUpdateRequest::Source::kInitialLoadFromStore) {
    next_structure_sequence_number_ =
        update_request->max_structure_sequence_number + 1;
  }

  // TODO(harringtond): Some StreamData fields not yet used.
  //    next_action_id - do we need to load the model before uploading
  //         actions? If not, we probably will want to move this out of
  //         StreamData.
  //    content_id - probably just ignore for now

  UpdateFlattenedTree();
}

EphemeralChangeId StreamModel::CreateEphemeralChange(
    std::vector<feedstore::DataOperation> operations) {
  const EphemeralChangeId id =
      ephemeral_changes_.AddEphemeralChange(std::move(operations))->id();

  UpdateFlattenedTree();

  return id;
}

void StreamModel::ExecuteOperations(
    std::vector<feedstore::DataOperation> operations) {
  for (const feedstore::DataOperation& operation : operations) {
    if (operation.has_structure()) {
      base_feature_tree_.ApplyStreamStructure(operation.structure());
    }
    if (operation.has_content()) {
      base_feature_tree_.AddContent(operation.content());
    }
  }

  if (store_observer_) {
    StoreUpdate store_update;
    store_update.operations = std::move(operations);
    store_update.sequence_number = next_structure_sequence_number_++;
    store_observer_->OnStoreChange(std::move(store_update));
  }

  UpdateFlattenedTree();
}

bool StreamModel::CommitEphemeralChange(EphemeralChangeId id) {
  std::unique_ptr<stream_model::EphemeralChange> change =
      ephemeral_changes_.Remove(id);
  if (!change)
    return false;

  // Note: it's possible that the does change even upon commit because it
  // may change the order that operations are applied. ExecuteOperations
  // will ensure observers are updated.
  ExecuteOperations(change->GetOperations());
  return true;
}

bool StreamModel::RejectEphemeralChange(EphemeralChangeId id) {
  if (ephemeral_changes_.Remove(id)) {
    UpdateFlattenedTree();
    return true;
  }
  return false;
}

void StreamModel::UpdateFlattenedTree() {
  if (ephemeral_changes_.GetChangeList().empty()) {
    feature_tree_after_changes_.reset();
  } else {
    feature_tree_after_changes_ =
        ApplyEphemeralChanges(base_feature_tree_, ephemeral_changes_);
  }
  // Update list of visible content.
  std::vector<ContentRevision> new_state =
      GetFinalFeatureTree()->GetVisibleContent();
  const bool content_list_changed = content_list_ != new_state;
  content_list_ = std::move(new_state);

  // Pack and send UiUpdate.
  UiUpdate update;
  update.content_list_changed = content_list_changed;
  for (auto& entry : shared_states_) {
    SharedState& shared_state = entry.second;
    UiUpdate::SharedStateInfo info;
    info.shared_state_id = entry.first;
    info.updated = shared_state.updated;
    update.shared_states.push_back(std::move(info));

    shared_state.updated = false;
  }

  if (observer_)
    observer_->OnUiUpdate(update);
}

stream_model::FeatureTree* StreamModel::GetFinalFeatureTree() {
  return feature_tree_after_changes_ ? feature_tree_after_changes_.get()
                                     : &base_feature_tree_;
}
const stream_model::FeatureTree* StreamModel::GetFinalFeatureTree() const {
  return const_cast<StreamModel*>(this)->GetFinalFeatureTree();
}

std::string StreamModel::DumpStateForTesting() {
  std::stringstream ss;
  ss << "StreamModel{\n";
  ss << "next_page_token='" << next_page_token_ << "'\n";
  ss << "consistency_token='" << consistency_token_ << "'\n";
  for (auto& entry : shared_states_) {
    ss << "shared_state[" << entry.first << "]='" << entry.second.data << "'\n";
  }
  ss << GetFinalFeatureTree()->DumpStateForTesting();
  ss << "}StreamModel\n";
  return ss.str();
}

}  // namespace feed
