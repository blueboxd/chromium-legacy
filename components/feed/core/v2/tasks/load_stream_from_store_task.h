// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TASKS_LOAD_STREAM_FROM_STORE_TASK_H_
#define COMPONENTS_FEED_CORE_V2_TASKS_LOAD_STREAM_FROM_STORE_TASK_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_store.h"
#include "components/offline_pages/task/task.h"

namespace base {
class Clock;
}

namespace feed {
struct StreamModelUpdateRequest;

// Attempts to load stream data from persistent storage.
class LoadStreamFromStoreTask : public offline_pages::Task {
 public:
  struct Result {
    Result();
    ~Result();
    Result(Result&&);
    Result& operator=(Result&&);
    LoadStreamStatus status = LoadStreamStatus::kNoStatus;
    std::unique_ptr<StreamModelUpdateRequest> update_request;
  };

  LoadStreamFromStoreTask(FeedStore* store,
                          const base::Clock* clock,
                          UserClass user_class,
                          base::OnceCallback<void(Result)> callback);
  ~LoadStreamFromStoreTask() override;
  LoadStreamFromStoreTask(const LoadStreamFromStoreTask&) = delete;
  LoadStreamFromStoreTask& operator=(const LoadStreamFromStoreTask&) = delete;

  void IgnoreStalenessForTesting() { ignore_staleness_ = true; }

 private:
  void Run() override;

  void LoadStreamDone(FeedStore::LoadStreamResult);
  void LoadContentDone(std::vector<feedstore::Content> content,
                       std::vector<feedstore::StreamSharedState> shared_states);
  void Complete(LoadStreamStatus status);

  base::WeakPtr<LoadStreamFromStoreTask> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  FeedStore* store_;  // Unowned.
  const base::Clock* clock_;
  UserClass user_class_;
  bool ignore_staleness_ = false;
  base::OnceCallback<void(Result)> result_callback_;

  std::unique_ptr<StreamModelUpdateRequest> update_request_;

  base::WeakPtrFactory<LoadStreamFromStoreTask> weak_ptr_factory_{this};
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TASKS_LOAD_STREAM_FROM_STORE_TASK_H_
