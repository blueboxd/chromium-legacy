// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_H_
#define CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_H_

#include "base/time/time.h"

// This file is to have common definitions that are to be shared by
// browser and child process.

namespace content {

// Constants for error messages.
extern const char kServiceWorkerRegisterErrorPrefix[];
extern const char kServiceWorkerUpdateErrorPrefix[];
extern const char kServiceWorkerUnregisterErrorPrefix[];
extern const char kServiceWorkerGetRegistrationErrorPrefix[];
extern const char kServiceWorkerGetRegistrationsErrorPrefix[];
extern const char kServiceWorkerFetchScriptError[];
extern const char kServiceWorkerBadHTTPResponseError[];
extern const char kServiceWorkerSSLError[];
extern const char kServiceWorkerBadMIMEError[];
extern const char kServiceWorkerNoMIMEError[];
extern const char kServiceWorkerRedirectError[];
extern const char kServiceWorkerAllowed[];
extern const char kServiceWorkerCopyScriptError[];

// Constants for invalid identifiers.
static const int kInvalidEmbeddedWorkerThreadId = -1;
static const int64_t kInvalidServiceWorkerResourceId = -1;

// The HTTP cache is bypassed for Service Worker scripts if the last network
// fetch occurred over 24 hours ago.
static constexpr base::TimeDelta kServiceWorkerScriptMaxCacheAge =
    base::TimeDelta::FromHours(24);

}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_H_
