// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_STREAM_MODEL_H_
#define COMPONENTS_FEED_CORE_V2_STREAM_MODEL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/proto/v2/wire/content_id.pb.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/core/v2/stream_model/ephemeral_change.h"
#include "components/feed/core/v2/stream_model/feature_tree.h"

namespace feedwire {
class DataOperation;
}  // namespace feedwire

namespace feed {
struct StreamModelUpdateRequest;

// An in-memory stream model.
class StreamModel {
 public:
  // Information about an update to the model.
  struct UiUpdate {
    struct SharedStateInfo {
      // The shared state's unique ID.
      std::string shared_state_id;
      // Whether the shared state was just modified or added.
      bool updated = false;
    };
    UiUpdate();
    ~UiUpdate();
    UiUpdate(const UiUpdate&);
    UiUpdate& operator=(const UiUpdate&);
    // Whether the list of content has changed. Use
    // |StreamModel::GetContentList()| to get the updated list of content.
    bool content_list_changed = false;
    // The list of shared states in the model.
    std::vector<SharedStateInfo> shared_states;
  };

  struct StoreUpdate {
    StoreUpdate();
    ~StoreUpdate();
    StoreUpdate(const StoreUpdate&);
    StoreUpdate(StoreUpdate&&);
    StoreUpdate& operator=(const StoreUpdate&);
    StoreUpdate& operator=(StoreUpdate&&);

    int32_t sequence_number = 0;
    std::vector<feedstore::DataOperation> operations;
  };

  class Observer {
   public:
    virtual ~Observer() = default;
    // Called when the UI model changes.
    virtual void OnUiUpdate(const UiUpdate& update) = 0;
  };
  class StoreObserver {
   public:
    // Called when the peristent store should be modified to reflect a model
    // change.
    virtual void OnStoreChange(const StoreUpdate& update) = 0;
  };

  StreamModel();
  ~StreamModel();

  StreamModel(const StreamModel& src) = delete;
  StreamModel& operator=(const StreamModel&) = delete;

  void SetObserver(Observer* observer);
  void SetStoreObserver(StoreObserver* store_observer);

  // Data access.

  // Returns the full list of content in the order it should be presented.
  const std::vector<ContentRevision>& GetContentList() const {
    return content_list_;
  }
  // Returns a list of all shared state IDs.
  std::vector<std::string> GetSharedStateIds() const;

  // Apply an update from the network or storage.
  void Update(std::unique_ptr<StreamModelUpdateRequest> update_request);

  // Returns the content identified by |ContentRevision|.
  const feedstore::Content* FindContent(ContentRevision revision) const;

  // Returns the shared state data identified by |id|.
  const std::string* FindSharedStateData(const std::string& id) const;

  // Apply |operations| to the model.
  void ExecuteOperations(std::vector<feedstore::DataOperation> operations);

  // Create a temporary change that may be undone or committed later.
  EphemeralChangeId CreateEphemeralChange(
      std::vector<feedstore::DataOperation> operations);
  // Commits a change. Returns false if the change does not exist.
  bool CommitEphemeralChange(EphemeralChangeId id);
  // Rejects a change. Returns false if the change does not exist.
  bool RejectEphemeralChange(EphemeralChangeId id);

  // Outputs a string representing the model state for debugging or testing.
  std::string DumpStateForTesting();

 private:
  struct SharedState {
    // Whether the data has been changed since the last call to |OnUiUpdate()|.
    bool updated = true;
    std::string data;
  };
  // The final feature tree after applying any ephemeral changes.
  // May link directly to |base_feature_tree_|.
  stream_model::FeatureTree* GetFinalFeatureTree();
  const stream_model::FeatureTree* GetFinalFeatureTree() const;

  void UpdateFlattenedTree();

  Observer* observer_ = nullptr;  // Unowned.
  StoreObserver* store_observer_ = nullptr;  // Unowned.
  stream_model::ContentIdMap id_map_;
  stream_model::FeatureTree base_feature_tree_{&id_map_};
  // |base_feature_tree_| with |ephemeral_changes_| applied.
  // Null if there are no ephemeral changes.
  std::unique_ptr<stream_model::FeatureTree> feature_tree_after_changes_;
  stream_model::EphemeralChangeList ephemeral_changes_;

  // The following data is associated with the stream, but lives outside of the
  // tree.

  std::string next_page_token_;    // TODO(harringtond): use this value.
  std::string consistency_token_;  // TODO(harringtond): use this value.
  base::Time last_added_time_;     // TODO(harringtond): use this value.
  base::flat_map<std::string, SharedState> shared_states_;
  int32_t next_structure_sequence_number_ = 0;

  // Current state of the flattened tree.
  // Updated after each tree change.
  std::vector<ContentRevision> content_list_;
};

}  // namespace feed
#endif  // COMPONENTS_FEED_CORE_V2_STREAM_MODEL_H_
