// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/mailbox_texture_holder.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/skia_texture_holder.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace blink {

MailboxTextureHolder::MailboxTextureHolder(
    const gpu::Mailbox& mailbox,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    scoped_refptr<MailboxRef> mailbox_ref,
    const SkImageInfo& sk_image_info,
    GLenum texture_target,
    bool is_origin_top_left)
    : TextureHolder(std::move(context_provider_wrapper),
                    std::move(mailbox_ref),
                    is_origin_top_left),
      mailbox_(mailbox),
      sk_image_info_(sk_image_info),
      texture_target_(texture_target) {
  DCHECK(mailbox_.IsSharedImage());
}

void MailboxTextureHolder::Sync(MailboxSyncMode mode) {
  gpu::SyncToken sync_token = mailbox_ref()->sync_token();

  if (IsCrossThread()) {
    // Was originally created on another thread. Should already have a sync
    // token from the original source context, already verified if needed.
    DCHECK(sync_token.HasData());
    DCHECK(mode != kVerifiedSyncToken || sync_token.verified_flush());
    return;
  }

  if (!ContextProviderWrapper())
    return;

  TRACE_EVENT0("blink", "MailboxTextureHolder::Sync");

  gpu::gles2::GLES2Interface* gl =
      ContextProviderWrapper()->ContextProvider()->ContextGL();

  if (mode == kOrderingBarrier) {
    if (!did_issue_ordering_barrier_) {
      gl->OrderingBarrierCHROMIUM();
      did_issue_ordering_barrier_ = true;
    }
    return;
  }

  if (!sync_token.HasData()) {
    if (mode == kVerifiedSyncToken) {
      gl->GenSyncTokenCHROMIUM(sync_token.GetData());
    } else {
      gl->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
    }
    mailbox_ref()->set_sync_token(sync_token);
    return;
  }

  // At this point we have a pre-existing sync token. We just need to verify
  // it if needed.  Providing a verified sync token when unverified is requested
  // is fine.
  if (mode == kVerifiedSyncToken && !sync_token.verified_flush()) {
    int8_t* token_data = sync_token.GetData();
    // TODO(junov): Batch this verification in the case where there are multiple
    // offscreen canvases being committed.
    gl->ShallowFlushCHROMIUM();
    gl->VerifySyncTokensCHROMIUM(&token_data, 1);
    sync_token.SetVerifyFlush();
    mailbox_ref()->set_sync_token(sync_token);
  }
}

bool MailboxTextureHolder::IsValid() const {
  if (IsCrossThread()) {
    // If context is is from another thread, validity cannot be verified.
    // Just assume valid. Potential problem will be detected later.
    return true;
  }
  return !!ContextProviderWrapper();
}

bool MailboxTextureHolder::IsCrossThread() const {
  return mailbox_ref()->is_cross_thread();
}

MailboxTextureHolder::~MailboxTextureHolder() = default;

}  // namespace blink
