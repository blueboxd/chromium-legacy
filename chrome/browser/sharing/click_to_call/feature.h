// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_CLICK_TO_CALL_FEATURE_H_
#define CHROME_BROWSER_SHARING_CLICK_TO_CALL_FEATURE_H_

#include "base/feature_list.h"
#include "build/build_config.h"

#if defined(OS_ANDROID)
// Feature to allow devices to receive the click to call message.
extern const base::Feature kClickToCallReceiver;
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
// Feature to allow click to call gets processed on desktop.
extern const base::Feature kClickToCallUI;
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)

#endif  // CHROME_BROWSER_SHARING_CLICK_TO_CALL_FEATURE_H_
