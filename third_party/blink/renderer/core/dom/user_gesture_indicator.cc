// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"

#include "base/time/default_clock.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

// User gestures timeout in 1 second.
const double kUserGestureTimeout = 1.0;

UserGestureToken::UserGestureToken(Status status)
    : consumable_gestures_(0),
      clock_(base::DefaultClock::GetInstance()),
      timestamp_(clock_->Now().ToDoubleT()),
      timeout_policy_(kDefault),
      was_forwarded_cross_process_(false) {
  if (status == kNewGesture || !UserGestureIndicator::CurrentTokenThreadSafe())
    consumable_gestures_++;
}

bool UserGestureToken::HasGestures() const {
  return consumable_gestures_ && !HasTimedOut();
}

void UserGestureToken::TransferGestureTo(UserGestureToken* other) {
  if (!HasGestures())
    return;
  consumable_gestures_--;
  other->consumable_gestures_++;
}

bool UserGestureToken::ConsumeGesture() {
  if (!consumable_gestures_)
    return false;
  consumable_gestures_--;
  return true;
}

void UserGestureToken::SetTimeoutPolicy(TimeoutPolicy policy) {
  if (HasGestures() && policy > timeout_policy_)
    timeout_policy_ = policy;
}

void UserGestureToken::ResetTimestamp() {
  timestamp_ = clock_->Now().ToDoubleT();
}

bool UserGestureToken::HasTimedOut() const {
  if (timeout_policy_ == kHasPaused)
    return false;
  return clock_->Now().ToDoubleT() - timestamp_ > kUserGestureTimeout;
}

bool UserGestureToken::WasForwardedCrossProcess() const {
  return was_forwarded_cross_process_;
}

void UserGestureToken::SetWasForwardedCrossProcess() {
  was_forwarded_cross_process_ = true;
}

// This enum is used in a histogram, so its values shouldn't change.
enum GestureMergeState {
  kOldTokenHasGesture = 1 << 0,
  kNewTokenHasGesture = 1 << 1,
  kGestureMergeStateEnd = 1 << 2,
};

UserGestureToken* UserGestureIndicator::root_token_ = nullptr;

void UserGestureIndicator::UpdateRootToken() {
  if (!root_token_)
    root_token_ = token_.get();
  else
    token_->TransferGestureTo(root_token_);
}

UserGestureIndicator::UserGestureIndicator(
    scoped_refptr<UserGestureToken> token) {
  if (!IsMainThread() || !token || token == root_token_)
    return;
  token_ = std::move(token);
  token_->ResetTimestamp();
  UpdateRootToken();
}

UserGestureIndicator::UserGestureIndicator(UserGestureToken::Status status) {
  if (!IsMainThread())
    return;
  token_ = base::AdoptRef(new UserGestureToken(status));
  UpdateRootToken();
}

UserGestureIndicator::~UserGestureIndicator() {
  if (IsMainThread() && token_ && token_ == root_token_)
    root_token_ = nullptr;
}

// static
bool UserGestureIndicator::ProcessingUserGesture() {
  if (auto* token = CurrentToken())
    return token->HasGestures();
  return false;
}

// static
bool UserGestureIndicator::ProcessingUserGestureThreadSafe() {
  return IsMainThread() && ProcessingUserGesture();
}

// static
bool UserGestureIndicator::ConsumeUserGesture() {
  if (auto* token = CurrentToken()) {
    if (token->ConsumeGesture())
      return true;
  }
  return false;
}

// static
bool UserGestureIndicator::ConsumeUserGestureThreadSafe() {
  return IsMainThread() && ConsumeUserGesture();
}

// static
UserGestureToken* UserGestureIndicator::CurrentToken() {
  DCHECK(IsMainThread());
  return root_token_;
}

// static
UserGestureToken* UserGestureIndicator::CurrentTokenThreadSafe() {
  return IsMainThread() ? CurrentToken() : nullptr;
}

// static
void UserGestureIndicator::SetTimeoutPolicy(
    UserGestureToken::TimeoutPolicy policy) {
  if (auto* token = CurrentTokenThreadSafe())
    token->SetTimeoutPolicy(policy);
}

// static
bool UserGestureIndicator::WasForwardedCrossProcess() {
  if (auto* token = CurrentTokenThreadSafe())
    return token->WasForwardedCrossProcess();
  return false;
}

// static
void UserGestureIndicator::SetWasForwardedCrossProcess() {
  if (auto* token = CurrentTokenThreadSafe())
    token->SetWasForwardedCrossProcess();
}

}  // namespace blink
