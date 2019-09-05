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

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
// Feature to allow click to call gets processed on desktop.
extern const base::Feature kClickToCallUI;

// Feature to show click to call in context menu when selected text is a phone
// number.
extern const base::Feature kClickToCallContextMenuForSelectedText;
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

#endif  // CHROME_BROWSER_SHARING_CLICK_TO_CALL_FEATURE_H_
