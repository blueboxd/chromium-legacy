// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/feed_stream.h"

#include "base/bind.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/v2/feed_network.h"
#include "components/feed/core/v2/feed_stream_background.h"
#include "components/prefs/pref_service.h"

namespace feed {

FeedStream::FeedStream(
    PrefService* profile_prefs,
    FeedNetwork* feed_network,
    base::Clock* clock,
    base::TickClock* tick_clock,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : profile_prefs_(profile_prefs),
      feed_network_(feed_network),
      clock_(clock),
      tick_clock_(tick_clock),
      background_task_runner_(background_task_runner),
      background_(std::make_unique<FeedStreamBackground>()),
      task_queue_(this) {
  // TODO(harringtond): Use these members.
  (void)clock_;
  (void)tick_clock_;
  (void)feed_network_;
}

FeedStream::~FeedStream() {
  // Delete |background_| in the background sequence.
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce([](std::unique_ptr<FeedStreamBackground> background) {},
                     std::move(background_)));
}

void FeedStream::SetArticlesListVisible(bool is_visible) {
  profile_prefs_->SetBoolean(prefs::kArticlesListVisible, is_visible);
}

bool FeedStream::IsArticlesListVisible() {
  return profile_prefs_->GetBoolean(prefs::kArticlesListVisible);
}

void FeedStream::OnTaskQueueIsIdle() {}

}  // namespace feed
