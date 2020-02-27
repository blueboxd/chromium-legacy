// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_FEED_STREAM_H_
#define COMPONENTS_FEED_CORE_V2_FEED_STREAM_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "components/feed/core/v2/feed_stream_api.h"
#include "components/offline_pages/task/task_queue.h"

class PrefService;

namespace base {
class Clock;
class TickClock;
}  // namespace base

namespace feed {
class FeedNetwork;
class FeedStreamBackground;

// Implements FeedStreamApi. |FeedStream| additionally exposes functionality
// needed by other classes within the Feed component.
class FeedStream : public FeedStreamApi,
                   public offline_pages::TaskQueue::Delegate {
 public:
  FeedStream(PrefService* profile_prefs,
             FeedNetwork* feed_network,
             base::Clock* clock,
             base::TickClock* tick_clock,
             scoped_refptr<base::SequencedTaskRunner> background_task_runner);
  ~FeedStream() override;

  FeedStream(const FeedStream&) = delete;
  FeedStream& operator=(const FeedStream&) = delete;

  // FeedStreamApi.
  void SetArticlesListVisible(bool is_visible) override;
  bool IsArticlesListVisible() override;

  // offline_pages::TaskQueue::Delegate.
  void OnTaskQueueIsIdle() override;

  // Provides access to |FeedStreamBackground|.
  // PostTask's to |background_callback| in the background thread. When
  // complete, executes |foreground_result_callback| with the result.
  template <typename R1, typename R2>
  bool RunInBackgroundAndReturn(
      const base::Location& from_here,
      base::OnceCallback<R1(FeedStreamBackground*)> background_callback,
      base::OnceCallback<void(R2)> foreground_result_callback) {
    return base::PostTaskAndReplyWithResult(
        background_task_runner_.get(), from_here,
        base::BindOnce(std::move(background_callback), background_.get()),
        std::move(foreground_result_callback));
  }

 private:
  PrefService* profile_prefs_;
  FeedNetwork* feed_network_;
  base::Clock* clock_;
  base::TickClock* tick_clock_;

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  // Owned, but should only be accessed with |background_task_runner_|.
  std::unique_ptr<FeedStreamBackground> background_;

  offline_pages::TaskQueue task_queue_;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_FEED_STREAM_H_
