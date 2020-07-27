// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_fast_call.h"

#include "base/check.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

WebGLScopedFastCall::WebGLScopedFastCall(WebGLFastCallHelper* helper,
                                         bool* deopt_flag)
    : helper_(helper), deopt_flag_(deopt_flag) {
  DCHECK(!helper_->in_fast_call_);
  helper_->in_fast_call_ = true;
  helper_->deopt_type_ = WebGLFastCallHelper::DeoptType::None;
}

WebGLScopedFastCall::~WebGLScopedFastCall() {
  DCHECK(helper_->in_fast_call_);
  helper_->in_fast_call_ = false;
  if (helper_->deopt_type_ != WebGLFastCallHelper::DeoptType::None) {
    CHECK(deopt_flag_);
    *deopt_flag_ = true;
  }
}

WebGLFastCallHelper::WebGLFastCallHelper() = default;
WebGLFastCallHelper::~WebGLFastCallHelper() = default;

void WebGLFastCallHelper::EagerDeopt() {
  DCHECK(in_fast_call_);
  DCHECK_EQ(deopt_type_, DeoptType::None);
  deopt_type_ = DeoptType::Eager;
}

void WebGLFastCallHelper::AddDeferredConsoleWarning(const String& message) {
  DCHECK(in_fast_call_);
  DCHECK_NE(deopt_type_, DeoptType::Eager);
  deopt_type_ = DeoptType::Deferred;
  deferred_console_warnings_.push_back(message);
}

void WebGLFastCallHelper::AddDeferredErrorOrWarningNotification(
    const String& message) {
  AddDeferredNotification(Notification::WarningOrError, message);
}

void WebGLFastCallHelper::AddDeferredErrorNotification(
    const String& error_type) {
  AddDeferredNotification(Notification::Error, error_type);
}

void WebGLFastCallHelper::AddDeferredWarningNotification() {
  AddDeferredNotification(Notification::Warning);
}

void WebGLFastCallHelper::AddDeferredNotification(Notification type,
                                                  const String& message) {
  DCHECK(in_fast_call_);
  DCHECK_NE(deopt_type_, DeoptType::Eager);
  deopt_type_ = DeoptType::Deferred;
  deferred_notifications_.emplace_back(type, message);
}

bool WebGLFastCallHelper::FlushDeferredEvents(
    WebGLRenderingContextBase* context) {
  DCHECK(!in_fast_call_);

  bool any_events = false;

  for (const auto& message : deferred_console_warnings_) {
    any_events = true;
    context->PrintWarningToConsole(message);
  }

  for (const auto& notification : deferred_notifications_) {
    any_events = true;
    switch (notification.first) {
      case Notification::WarningOrError:
        probe::DidFireWebGLErrorOrWarning(context->canvas(),
                                          notification.second);
        break;
      case Notification::Error:
        probe::DidFireWebGLError(context->canvas(), notification.second);
        break;

      case Notification::Warning:
        probe::DidFireWebGLWarning(context->canvas());
        break;

      default:
        NOTREACHED();
    }
  }
  deferred_console_warnings_.clear();
  deferred_notifications_.clear();

  // If there were any deferred events, then we must have deopt'ed to defer
  // logging.
  DCHECK(!any_events || deopt_type_ == DeoptType::Deferred);
  return any_events;
}

}  // namespace blink
