// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_FAST_CALL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_FAST_CALL_H_

#include "base/compiler_specific.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class WebGLRenderingContextBase;
class WebGLFastCallHelper;
class WebGLScopedFastCall {
 public:
  ~WebGLScopedFastCall();

 private:
  friend class WebGLFastCallHelper;

  WebGLScopedFastCall(WebGLFastCallHelper* helper, bool* deopt_flag);

  WebGLFastCallHelper* helper_;
  bool* deopt_flag_;
};

// WebGLFastCallHelper assists with proper handling of console messages and
// interactions with DevTools when inside a FastCall. Inside a FastCall, objects
// may *not* be allocated on the managed heap, and calls may *not* be made back
// into V8. Both of these happen when WebGL encounters errors or warnings. To
// circumvent the issue, when inside a FastCall, this class stores warnings and
// notifications to DevTools in a deferred queue of events. Creating such an
// event will trigger V8's deopt mechanism. V8 will call the method again using
// the slow function callback, where it is safe to flush the events.
//
// Proper usage: the WebGL bindings may do one of the following:
//   A) Complete normal execution of a function while enqueueing deferred
//      events. Then, flush the deferred events at the top of the slow function
//      callback, and return immediately.
//   B) Signal an eager deopt in the fast function callback and return
//      immediately. This must be idempotent.
//      Then, execute the entire slow function callback.
//
// Note: Mixing the eager deopt and deferred message mechanisms is not allowed.
//
// Examples:
//  A)
//    void WebGLRenderingContextBase::DeferredDeoptExampleFastCallback(
//          args..., deopt_flag) {
//
//      auto scoped_call = fast_call_.EnterScoped(deopt_flag);
//      if (SomeConditionIsInvalid()) {
//        fast_call_.AddDeferredConsoleWarning("Something wrong happened");
//        fast_call_.AddDeferredWarningNotification();
//      }
//      DeferredDeoptExampleImpl(...args);
//    }
//
//    void WebGLRenderingContextBase::DeferredDeoptExampleSlowCallback(
//          args...) {
//
//      if (fast_call_.FlushDeferredEvents(this)) {
//        // We already executed DeferredDeoptExampleImpl, and had deferred
//        // events.
//        return;
//      }
//      if (SomeConditionIsInvalid()) {
//        LogImmediatelyToConsole("Something wrong happened");
//      }
//      DeferredDeoptExampleImpl(...args);
//    }
//
//
//  B)
//    void WebGLRenderingContextBase::EagerDeoptExampleFastCallback(
//          args..., deopt_flag) {
//
//      auto scoped_call = fast_call_.EnterScoped(deopt_flag);
//      if (SomeConditionIsInvalid()) {
//        fast_call_.EagerDeopt();
//        return;
//      }
//      EagerDeoptExampleImpl(...args);
//    }
//
//    void WebGLRenderingContextBase::EagerDeoptExampleSlowCallback {
//       if (SomeConditionIsInvalid()) {
//         LogImmediatelyToConsole("Something wrong happened");
//       }
//       EagerDeoptExampleImpl(...args);
//    }
//
class WebGLFastCallHelper {
 public:
  WebGLFastCallHelper();
  ~WebGLFastCallHelper();

  bool InFastCall() const { return in_fast_call_; }

  WebGLScopedFastCall EnterScoped(bool* deopt_flag = nullptr)
      WARN_UNUSED_RESULT {
    return WebGLScopedFastCall(this, deopt_flag);
  }

  void EagerDeopt();

  void AddDeferredConsoleWarning(const String& message);

  void AddDeferredErrorOrWarningNotification(const String& message);
  void AddDeferredErrorNotification(const String& error_type);
  void AddDeferredWarningNotification();

  bool FlushDeferredEvents(WebGLRenderingContextBase* context);

 private:
  friend class WebGLScopedFastCall;

  enum class DeoptType {
    None,
    Deferred,
    Eager,
  };

  enum class Notification {
    WarningOrError,
    Warning,
    Error,
  };

  void AddDeferredNotification(Notification type, const String& message = "");

  bool in_fast_call_ = false;
  DeoptType deopt_type_ = DeoptType::None;

  // Give these vectors inline capacity of 1. In the common case, only a single
  // message and notification is generated.
  Vector<String, 1> deferred_console_warnings_;
  Vector<std::pair<Notification, String>, 1> deferred_notifications_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_FAST_CALL_H_
