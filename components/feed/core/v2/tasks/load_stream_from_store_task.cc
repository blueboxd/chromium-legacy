// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/tasks/load_stream_from_store_task.h"

#include <algorithm>
#include <utility>

#include "base/time/clock.h"
#include "components/feed/core/proto/v2/store.pb.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/feed/core/v2/proto_util.h"
#include "components/feed/core/v2/public/feed_stream_api.h"
#include "components/feed/core/v2/scheduling.h"
#include "components/feed/core/v2/stream_model_update_request.h"

namespace feed {

LoadStreamFromStoreTask::Result::Result() = default;
LoadStreamFromStoreTask::Result::~Result() = default;
LoadStreamFromStoreTask::Result::Result(Result&&) = default;
LoadStreamFromStoreTask::Result& LoadStreamFromStoreTask::Result::operator=(
    Result&&) = default;

LoadStreamFromStoreTask::LoadStreamFromStoreTask(
    FeedStore* store,
    const base::Clock* clock,
    UserClass user_class,
    base::OnceCallback<void(Result)> callback)
    : store_(store),
      clock_(clock),
      user_class_(user_class),
      result_callback_(std::move(callback)),
      update_request_(std::make_unique<StreamModelUpdateRequest>()) {}

LoadStreamFromStoreTask::~LoadStreamFromStoreTask() = default;

void LoadStreamFromStoreTask::Run() {
  store_->LoadStream(
      base::BindOnce(&LoadStreamFromStoreTask::LoadStreamDone, GetWeakPtr()));
}

void LoadStreamFromStoreTask::LoadStreamDone(
    FeedStore::LoadStreamResult result) {
  if (result.read_error) {
    Complete(LoadStreamStatus::kFailedWithStoreError);
    return;
  }
  if (result.stream_structures.empty()) {
    Complete(LoadStreamStatus::kNoStreamDataInStore);
    return;
  }
  if (!ignore_staleness_) {
    const base::TimeDelta content_age =
        clock_->Now() - feedstore::GetLastAddedTime(result.stream_data);
    if (content_age < base::TimeDelta()) {
      Complete(LoadStreamStatus::kDataInStoreIsStaleTimestampInFuture);
      return;
    } else if (ShouldWaitForNewContent(user_class_, true, content_age)) {
      Complete(LoadStreamStatus::kDataInStoreIsStale);
      return;
    }
  }

  // TODO(harringtond): Add other failure cases?

  std::vector<ContentId> referenced_content_ids;
  for (const feedstore::StreamStructureSet& structure_set :
       result.stream_structures) {
    for (const feedstore::StreamStructure& structure :
         structure_set.structures()) {
      if (structure.type() == feedstore::StreamStructure::CONTENT) {
        referenced_content_ids.push_back(structure.content_id());
      }
    }
  }

  store_->ReadContent(
      std::move(referenced_content_ids), {result.stream_data.shared_state_id()},
      base::BindOnce(&LoadStreamFromStoreTask::LoadContentDone, GetWeakPtr()));

  update_request_->stream_data = std::move(result.stream_data);

  // Move stream structures into the update request.
  // These need sorted by sequence number, and then inserted into
  // |update_request_->stream_structures|.
  std::sort(result.stream_structures.begin(), result.stream_structures.end(),
            [](const feedstore::StreamStructureSet& a,
               const feedstore::StreamStructureSet& b) {
              return a.sequence_number() < b.sequence_number();
            });

  for (feedstore::StreamStructureSet& structure_set :
       result.stream_structures) {
    update_request_->max_structure_sequence_number =
        structure_set.sequence_number();
    for (feedstore::StreamStructure& structure :
         *structure_set.mutable_structures()) {
      update_request_->stream_structures.push_back(std::move(structure));
    }
  }
}

void LoadStreamFromStoreTask::LoadContentDone(
    std::vector<feedstore::Content> content,
    std::vector<feedstore::StreamSharedState> shared_states) {
  update_request_->content = std::move(content);
  update_request_->shared_states = std::move(shared_states);

  update_request_->source =
      StreamModelUpdateRequest::Source::kInitialLoadFromStore;

  Complete(LoadStreamStatus::kLoadedFromStore);
}

void LoadStreamFromStoreTask::Complete(LoadStreamStatus status) {
  Result task_result;
  task_result.status = status;
  if (status == LoadStreamStatus::kLoadedFromStore) {
    task_result.update_request = std::move(update_request_);
  }
  std::move(result_callback_).Run(std::move(task_result));
  TaskComplete();
}

}  // namespace feed
