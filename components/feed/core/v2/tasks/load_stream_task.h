// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TASKS_LOAD_STREAM_TASK_H_
#define COMPONENTS_FEED_CORE_V2_TASKS_LOAD_STREAM_TASK_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/tasks/load_stream_from_store_task.h"
#include "components/offline_pages/task/task.h"
#include "components/version_info/channel.h"

namespace feed {
class FeedStream;

// Loads the stream model from storage or network. If data is refreshed from the
// network, it is persisted to |FeedStore| by overwriting any existing stream
// data.
// This task has two modes, see |LoadStreamTask::LoadType|.
class LoadStreamTask : public offline_pages::Task {
 public:
  enum class LoadType {
    // Loads the stream model into memory. If successful, this directly forces a
    // model load in |FeedStream()| before completing the task.
    kInitialLoad,
    // Refreshes the stored stream data from the network. This will fail if the
    // model is already loaded.
    kBackgroundRefresh,
  };

  struct Result {
    Result();
    explicit Result(LoadStreamStatus status);
    ~Result();
    Result(const Result&);
    Result& operator=(const Result&);

    // Final status of loading the stream.
    LoadStreamStatus final_status = LoadStreamStatus::kNoStatus;
    // Status of just loading the stream from the persistent store, if that
    // was attempted.
    LoadStreamStatus load_from_store_status = LoadStreamStatus::kNoStatus;
    LoadType load_type;
    // Information about the network request, if one was made.
    base::Optional<NetworkResponseInfo> network_response_info;
  };

  LoadStreamTask(LoadType load_type,
                 FeedStream* stream,
                 base::OnceCallback<void(Result)> done_callback);
  ~LoadStreamTask() override;
  LoadStreamTask(const LoadStreamTask&) = delete;
  LoadStreamTask& operator=(const LoadStreamTask&) = delete;

 private:
  void Run() override;
  base::WeakPtr<LoadStreamTask> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void LoadFromStoreComplete(LoadStreamFromStoreTask::Result result);
  void QueryRequestComplete(FeedNetwork::QueryRequestResult result);
  void Done(LoadStreamStatus status);

  LoadType load_type_;
  FeedStream* stream_;  // Unowned.
  std::unique_ptr<LoadStreamFromStoreTask> load_from_store_task_;

  // Information to be stuffed in |Result|.
  LoadStreamStatus load_from_store_status_ = LoadStreamStatus::kNoStatus;
  base::Optional<NetworkResponseInfo> network_response_info_;

  base::TimeTicks fetch_start_time_;
  base::OnceCallback<void(Result)> done_callback_;
  base::WeakPtrFactory<LoadStreamTask> weak_ptr_factory_{this};
};
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TASKS_LOAD_STREAM_TASK_H_
