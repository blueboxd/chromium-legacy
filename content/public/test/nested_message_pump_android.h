// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_NESTED_MESSAGE_PUMP_ANDROID_H_
#define CONTENT_PUBLIC_TEST_NESTED_MESSAGE_PUMP_ANDROID_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_android.h"

namespace content {

// A nested message pump to be used for content browsertests and web tests
// on Android. It overrides the default UI message pump to allow nested loops.
class NestedMessagePumpAndroid : public base::MessagePumpForUI {
 public:
  NestedMessagePumpAndroid();
  ~NestedMessagePumpAndroid() override;

  void Run(Delegate*) override;
  void Quit() override;
  void Attach(Delegate*) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NestedMessagePumpAndroid);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_NESTED_MESSAGE_PUMP_ANDROID_H_
