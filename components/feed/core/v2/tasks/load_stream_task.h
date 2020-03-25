// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TASKS_LOAD_STREAM_TASK_H_
#define COMPONENTS_FEED_CORE_V2_TASKS_LOAD_STREAM_TASK_H_

#include "components/offline_pages/task/task.h"

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/feed/core/v2/feed_network.h"

namespace feed {
class FeedStream;

// Loads the stream model from storage or network.
// TODO(harringtond): This is ultra-simplified so that we have something in
// in place temporarily. Right now, we just always fetch from network.
class LoadStreamTask : public offline_pages::Task {
 public:
  explicit LoadStreamTask(FeedStream* stream, base::OnceClosure done_callback);
  ~LoadStreamTask() override;
  LoadStreamTask(const LoadStreamTask&) = delete;
  LoadStreamTask& operator=(const LoadStreamTask&) = delete;

 private:
  void Run() override;
  base::WeakPtr<LoadStreamTask> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void QueryRequestComplete(FeedNetwork::QueryRequestResult result);
  void Done();

  FeedStream* stream_;  // Unowned.
  base::TimeTicks fetch_start_time_;
  base::OnceClosure done_callback_;
  base::WeakPtrFactory<LoadStreamTask> weak_ptr_factory_{this};
};
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TASKS_LOAD_STREAM_TASK_H_
