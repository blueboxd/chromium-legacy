// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/services.h"

#include <utility>

#include "content/public/utility/content_utility_client.h"

namespace content {

void HandleServiceRequestOnIOThread(
    mojo::GenericPendingReceiver receiver,
    base::SequencedTaskRunner* main_thread_task_runner) {
  // If the request was handled already, we should not reach this point.
  DCHECK(receiver.is_valid());
  GetContentClient()->utility()->RunIOThreadService(&receiver);

  if (receiver.is_valid()) {
    main_thread_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&HandleServiceRequestOnMainThread, std::move(receiver)));
  }
}

void HandleServiceRequestOnMainThread(mojo::GenericPendingReceiver receiver) {
  // If the request was handled already, we should not reach this point.
  DCHECK(receiver.is_valid());
  GetContentClient()->utility()->RunMainThreadService(std::move(receiver));
}

}  // namespace content
