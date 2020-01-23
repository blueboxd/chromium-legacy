// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/texture_holder.h"

namespace blink {

namespace {
void ReleaseCallbackOnContextThread(
    std::unique_ptr<viz::SingleReleaseCallback> callback,
    const gpu::SyncToken sync_token) {
  callback->Run(sync_token, /* is_lost = */ false);
}
}  // namespace

TextureHolder::MailboxRef::MailboxRef(
    const gpu::SyncToken& sync_token,
    base::PlatformThreadRef context_thread_ref,
    scoped_refptr<base::SingleThreadTaskRunner> context_task_runner,
    std::unique_ptr<viz::SingleReleaseCallback> release_callback)
    : sync_token_(sync_token),
      context_thread_ref_(context_thread_ref),
      context_task_runner_(std::move(context_task_runner)),
      release_callback_(std::move(release_callback)) {
  DCHECK(!is_cross_thread() || sync_token_.verified_flush());
}

TextureHolder::MailboxRef::~MailboxRef() {
  if (context_thread_ref_ == base::PlatformThread::CurrentRef()) {
    ReleaseCallbackOnContextThread(std::move(release_callback_), sync_token_);
  } else {
    context_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ReleaseCallbackOnContextThread,
                                  std::move(release_callback_), sync_token_));
  }
}

}  // namespace blink
