// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_FEATURE_FLAGS_H_
#define CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_FEATURE_FLAGS_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

// Feature to allow shared clipboard gets processed.
extern const base::Feature kSharedClipboardUI;

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
// Feature to enable handling remote copy messages.
extern const base::Feature kRemoteCopyReceiver;

// List of allowed origins to fetch images from, comma separated.
extern const base::FeatureParam<std::string> kRemoteCopyAllowedOrigins;

// Feature to enable image notifications for remote copy messages.
extern const base::Feature kRemoteCopyImageNotification;

// Feature to enable persistent notifications for remote copy messages.
extern const base::Feature kRemoteCopyPersistentNotification;

// Feature to enable progress notifications for remote copy messages.
extern const base::Feature kRemoteCopyProgressNotification;
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

#endif  // CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_FEATURE_FLAGS_H_
