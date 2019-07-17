// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_H_
#define CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_H_

#include "base/time/time.h"

// This file is to have common definitions that are to be shared by
// browser and child process.

namespace content {

// The HTTP cache is bypassed for Service Worker scripts if the last network
// fetch occurred over 24 hours ago.
static constexpr base::TimeDelta kServiceWorkerScriptMaxCacheAge =
    base::TimeDelta::FromHours(24);

}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_H_
