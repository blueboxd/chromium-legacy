// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_types.h"

namespace content {

const char kServiceWorkerRegisterErrorPrefix[] =
    "Failed to register a ServiceWorker: ";
const char kServiceWorkerUpdateErrorPrefix[] =
    "Failed to update a ServiceWorker: ";
const char kServiceWorkerUnregisterErrorPrefix[] =
    "Failed to unregister a ServiceWorkerRegistration: ";
const char kServiceWorkerGetRegistrationErrorPrefix[] =
    "Failed to get a ServiceWorkerRegistration: ";
const char kServiceWorkerGetRegistrationsErrorPrefix[] =
    "Failed to get ServiceWorkerRegistration objects: ";
const char kServiceWorkerFetchScriptError[] =
    "An unknown error occurred when fetching the script.";
const char kServiceWorkerBadHTTPResponseError[] =
    "A bad HTTP response code (%d) was received when fetching the script.";
const char kServiceWorkerSSLError[] =
    "An SSL certificate error occurred when fetching the script.";
const char kServiceWorkerBadMIMEError[] =
    "The script has an unsupported MIME type ('%s').";
const char kServiceWorkerNoMIMEError[] =
    "The script does not have a MIME type.";
const char kServiceWorkerRedirectError[] =
    "The script resource is behind a redirect, which is disallowed.";
const char kServiceWorkerAllowed[] = "Service-Worker-Allowed";
const char kServiceWorkerCopyScriptError[] =
    "An unknown error occurred when copying the script.";
}  // namespace content
