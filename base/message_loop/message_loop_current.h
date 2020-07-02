// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_LOOP_CURRENT_H_
#define BASE_MESSAGE_LOOP_MESSAGE_LOOP_CURRENT_H_

#include "base/task/current_thread.h"
#include "build/build_config.h"

// MessageLoopCurrent, MessageLoopCurrentForIO, and MessageLoopCurrentForUI are
// being replaced by CurrentThread, CurrentIOThread, and CurrentUIThread
// respectively (https://crbug.com/891670). You should no longer include this
// header nor use these classes. This file will go away as soon as we have
// migrated all uses.

namespace base {
using MessageLoopCurrent = CurrentThread;
using MessageLoopCurrentForIO = CurrentIOThread;

#if !defined(OS_NACL)
using MessageLoopCurrentForUI = CurrentUIThread;
#endif
}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_LOOP_CURRENT_H_
