// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NETWORK_SERVICE_INSTANCE_IMPL_H_
#define CONTENT_BROWSER_NETWORK_SERVICE_INSTANCE_IMPL_H_

#include "base/callback.h"
#include "base/callback_list.h"
#include "content/common/content_export.h"

namespace content {

// Creates the network::NetworkService object on the IO thread directly instead
// of trying to go through the ServiceManager.
CONTENT_EXPORT void ForceCreateNetworkServiceDirectlyForTesting();

// Resets the interface ptr to the network service.
CONTENT_EXPORT void ResetNetworkServiceForTesting();

// Registers |handler| to run (on UI thread) after NetworkServicePtr encounters
// an error.  Note that there are no ordering guarantees wrt error handlers for
// other interfaces (e.g. NetworkContextPtr and/or URLLoaderFactoryPtr).
//
// Can only be called on the UI thread.  No-op if NetworkService is disabled.
CONTENT_EXPORT std::unique_ptr<base::CallbackList<void()>::Subscription>
RegisterNetworkServiceCrashHandler(base::RepeatingClosure handler);

// Corresponds to the "NetworkServiceAvailability" histogram enumeration type in
// src/tools/metrics/histograms/enums.xml.
//
// DO NOT REORDER OR CHANGE THE MEANING OF THESE VALUES.
enum class NetworkServiceAvailability {
  AVAILABLE = 0,
  NOT_CREATED = 1,
  NOT_BOUND = 2,
  ENCOUNTERED_ERROR = 3,
  NOT_RESPONDING = 4,
  kMaxValue = NOT_RESPONDING
};

// TODO(http://crbug.com/934317): Remove these when done debugging renderer
// hangs.
CONTENT_EXPORT NetworkServiceAvailability GetNetworkServiceAvailability();
CONTENT_EXPORT base::TimeDelta GetTimeSinceLastNetworkServiceCrash();

}  // namespace content

#endif  // CONTENT_BROWSER_NETWORK_SERVICE_INSTANCE_IMPL_H_
