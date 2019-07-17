// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/fcm_constants.h"

const char kFCMScope[] = "GCM";

const char kSharingFCMAppID[] = "com.google.chrome.sharing.fcm";

const constexpr base::TimeDelta kRegistrationExpiration =
    base::TimeDelta::FromDays(1);
