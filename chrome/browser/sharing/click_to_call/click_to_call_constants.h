// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_CONSTANTS_H_
#define CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_CONSTANTS_H_

#include "base/time/time.h"

// Time limit for click to call message expiration.
// TODO(yasmo): Confirm this value with the team.
constexpr base::TimeDelta kSharingClickToCallMessageTTL =
    base::TimeDelta::FromMinutes(10);

#endif  // CHROME_BROWSER_SHARING_CLICK_TO_CALL_CLICK_TO_CALL_CONSTANTS_H_
