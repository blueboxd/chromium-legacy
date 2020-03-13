// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mouse_lock_dispatcher.h"

#include "base/logging.h"
#include "third_party/blink/public/common/input/web_input_event.h"

namespace content {

MouseLockDispatcher::MouseLockDispatcher()
    : mouse_locked_(false),
      pending_lock_request_(false),
      pending_unlock_request_(false),
      target_(nullptr) {}

MouseLockDispatcher::~MouseLockDispatcher() = default;

bool MouseLockDispatcher::LockMouse(
    LockTarget* target,
    blink::WebLocalFrame* requester_frame,
    blink::WebWidgetClient::PointerLockCallback callback,
    bool request_unadjusted_movement) {
  if (MouseLockedOrPendingAction())
    return false;

  pending_lock_request_ = true;
  target_ = target;

  lock_mouse_callback_ = std::move(callback);

  SendLockMouseRequest(requester_frame, request_unadjusted_movement);
  return true;
}

bool MouseLockDispatcher::ChangeMouseLock(
    LockTarget* target,
    blink::WebLocalFrame* requester_frame,
    blink::WebWidgetClient::PointerLockCallback callback,
    bool request_unadjusted_movement) {
  if (pending_lock_request_ || pending_unlock_request_)
    return false;

  pending_lock_request_ = true;
  target_ = target;

  lock_mouse_callback_ = std::move(callback);

  SendChangeLockRequest(requester_frame, request_unadjusted_movement);
  return true;
}

void MouseLockDispatcher::UnlockMouse(LockTarget* target) {
  if (target && target == target_ && !pending_unlock_request_) {
    pending_unlock_request_ = true;

    SendUnlockMouseRequest();
  }
}

void MouseLockDispatcher::OnLockTargetDestroyed(LockTarget* target) {
  if (target == target_) {
    UnlockMouse(target);
    target_ = nullptr;
  }
}

void MouseLockDispatcher::ClearLockTarget() {
  OnLockTargetDestroyed(target_);
}

bool MouseLockDispatcher::IsMouseLockedTo(LockTarget* target) {
  return mouse_locked_ && target_ == target;
}

bool MouseLockDispatcher::WillHandleMouseEvent(
    const blink::WebMouseEvent& event) {
  if (mouse_locked_ && target_)
    return target_->HandleMouseLockedInputEvent(event);
  return false;
}

void MouseLockDispatcher::OnChangeLockAck(
    blink::mojom::PointerLockResult result) {
  pending_lock_request_ = false;
  if (lock_mouse_callback_) {
    std::move(lock_mouse_callback_).Run(result);
  }
}

void MouseLockDispatcher::OnLockMouseACK(
    blink::mojom::PointerLockResult result) {
  DCHECK(!mouse_locked_ && pending_lock_request_);

  mouse_locked_ = result == blink::mojom::PointerLockResult::kSuccess;
  pending_lock_request_ = false;
  if (pending_unlock_request_ && !mouse_locked_) {
    // We have sent an unlock request after the lock request. However, since
    // the lock request has failed, the unlock request will be ignored by the
    // browser side and there won't be any response to it.
    pending_unlock_request_ = false;
  }

  if (lock_mouse_callback_) {
    std::move(lock_mouse_callback_).Run(result);
  }

  LockTarget* last_target = target_;
  if (!mouse_locked_)
    target_ = nullptr;

  // Callbacks made after all state modification to prevent reentrant errors
  // such as OnLockMouseACK() synchronously calling LockMouse().

  if (last_target)
    last_target->OnLockMouseACK(result ==
                                blink::mojom::PointerLockResult::kSuccess);
}

void MouseLockDispatcher::OnMouseLockLost() {
  DCHECK(mouse_locked_ && !pending_lock_request_);

  mouse_locked_ = false;
  pending_unlock_request_ = false;

  LockTarget* last_target = target_;
  target_ = nullptr;

  // Callbacks made after all state modification to prevent reentrant errors
  // such as OnMouseLockLost() synchronously calling LockMouse().

  if (last_target)
    last_target->OnMouseLockLost();
}

}  // namespace content
