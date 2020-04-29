// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mouse_lock_dispatcher.h"

#include "base/check.h"
#include "third_party/blink/public/common/input/web_input_event.h"

namespace content {

MouseLockDispatcher::MouseLockDispatcher()
    : pending_lock_request_(false),
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
  if (!mouse_lock_context_)
    return false;

  lock_mouse_callback_ = std::move(callback);
  // Unretained is safe because |this| owns the mojo::Remote
  mouse_lock_context_->RequestMouseLockChange(
      request_unadjusted_movement,
      base::BindOnce(&MouseLockDispatcher::OnChangeLockAck,
                     base::Unretained(this)));
  return true;
}

void MouseLockDispatcher::FlushContextPipeForTesting() {
  if (mouse_lock_context_)
    mouse_lock_context_.FlushForTesting();
}

void MouseLockDispatcher::UnlockMouse(LockTarget* target) {
  if (IsMouseLockedTo(target)) {
    mouse_lock_context_.reset();
    target->OnMouseLockLost();
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
  return mouse_lock_context_ && target_ == target;
}

bool MouseLockDispatcher::WillHandleMouseEvent(
    const blink::WebMouseEvent& event) {
  if (mouse_lock_context_ && target_)
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
    blink::mojom::PointerLockResult result,
    mojo::PendingRemote<blink::mojom::PointerLockContext> context) {
  DCHECK(!mouse_lock_context_ && pending_lock_request_);

  pending_lock_request_ = false;
  if (pending_unlock_request_ && !context) {
    // We have sent an unlock request after the lock request. However, since
    // the lock request has failed, the unlock request will be ignored by the
    // browser side and there won't be any response to it.
    pending_unlock_request_ = false;
  }

  if (context) {
    mouse_lock_context_.Bind(std::move(context));
    // The browser might unlock the mouse for many reasons including closing
    // the tab, the user hitting esc, the page losing focus, and more.
    mouse_lock_context_.set_disconnect_handler(base::BindOnce(
        &MouseLockDispatcher::OnMouseLockLost, base::Unretained(this)));
  }

  if (lock_mouse_callback_)
    std::move(lock_mouse_callback_).Run(result);

  LockTarget* last_target = target_;
  if (!mouse_lock_context_)
    target_ = nullptr;

  // Callbacks made after all state modification to prevent reentrant errors
  // such as OnLockMouseACK() synchronously calling LockMouse().

  if (last_target)
    last_target->OnLockMouseACK(result ==
                                blink::mojom::PointerLockResult::kSuccess);
}

void MouseLockDispatcher::OnMouseLockLost() {
  DCHECK(mouse_lock_context_ && !pending_lock_request_);
  mouse_lock_context_.reset();
  pending_unlock_request_ = false;

  LockTarget* last_target = target_;
  target_ = nullptr;

  // Callbacks made after all state modification to prevent reentrant errors
  // such as OnMouseLockLost() synchronously calling LockMouse().

  if (last_target)
    last_target->OnMouseLockLost();
}

}  // namespace content
