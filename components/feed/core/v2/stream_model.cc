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

namespace feed {

StreamModel::UiUpdate::UiUpdate() = default;
StreamModel::UiUpdate::~UiUpdate() = default;
StreamModel::UiUpdate::UiUpdate(const UiUpdate&) = default;
StreamModel::UiUpdate& StreamModel::UiUpdate::operator=(const UiUpdate&) =
    default;

StreamModel::StreamModel() = default;
StreamModel::~StreamModel() = default;

void StreamModel::SetObserver(Observer* observer) {
  DCHECK(!observer || !observer_)
      << "Attempting to set the observer multiple times";
  observer_ = observer;
}

const feedstore::Content* StreamModel::FindContent(
    ContentRevision revision) const {
  return GetFinalFeatureTree()->FindContent(revision);
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
  for (feedstore::DataOperation& operation : operations) {
    if (operation.has_structure()) {
      base_feature_tree_.ApplyStreamStructure(operation.structure());
    }
    if (operation.has_content()) {
      base_feature_tree_.AddContent(std::move(*operation.mutable_content()));
    }
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
  std::vector<ContentRevision> new_state =
      GetFinalFeatureTree()->GetVisibleContent();

  UiUpdate update;
  update.content_list_changed = content_list_ != new_state;

  content_list_ = std::move(new_state);
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

}  // namespace feed
