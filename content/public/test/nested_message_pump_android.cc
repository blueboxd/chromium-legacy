// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/nested_message_pump_android.h"

#include "base/android/jni_android.h"
#include "content/public/test/android/test_support_content_jni_headers/NestedSystemMessageHandler_jni.h"

namespace content {

NestedMessagePumpAndroid::NestedMessagePumpAndroid() = default;
NestedMessagePumpAndroid::~NestedMessagePumpAndroid() = default;

// We need to override Run() instead of using the implementation from
// base::MessagePumpForUI because one of the side-effects of
// dispatchOneMessage() is calling Looper::pollOnce(). If that happens while we
// are inside Alooper_pollOnce(), we get a crash because Android Looper
// isn't re-entrant safe. Instead, we keep the entire run loop in Java (in
// MessageQueue.next()).
void NestedMessagePumpAndroid::Run(base::MessagePump::Delegate* delegate) {
  auto* env = base::android::AttachCurrentThread();
  SetDelegate(delegate);
  ResetShouldQuit();
  ScheduleWork();

  while (!ShouldQuit()) {
    // Dispatch the first available Java message and one or more C++ messages as
    // a side effect. If there are no Java messages available, this call will
    // block until one becomes available (while continuing to process C++ work).
    bool ret = Java_NestedSystemMessageHandler_dispatchOneMessage(env);
    CHECK(ret) << "Error running java message loop, tests will likely fail.";
  }
}

void NestedMessagePumpAndroid::Quit() {
  // Wake up the Java message dispatcher to exit the loop in Run().
  auto* env = base::android::AttachCurrentThread();
  Java_NestedSystemMessageHandler_postQuitMessage(env);
  MessagePumpForUI::Quit();
}

// Since this pump allows Run() to be called on the UI thread, there's no need
// to also attach the pump on that thread. Making attach a no-op also prevents
// the top level run loop from being considered a nested one.
void NestedMessagePumpAndroid::Attach(Delegate*) {}

}  // namespace content
