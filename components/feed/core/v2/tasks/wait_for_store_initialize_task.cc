// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/tasks/wait_for_store_initialize_task.h"

#include "components/feed/core/v2/feed_store.h"

namespace feed {

WaitForStoreInitializeTask::WaitForStoreInitializeTask(FeedStore* store)
    : store_(store) {}
WaitForStoreInitializeTask::~WaitForStoreInitializeTask() = default;

void WaitForStoreInitializeTask::Run() {
  // |this| stays alive as long as the |store_|, so Unretained is safe.
  store_->Initialize(base::BindOnce(&WaitForStoreInitializeTask::TaskComplete,
                                    base::Unretained(this)));
}

}  // namespace feed
