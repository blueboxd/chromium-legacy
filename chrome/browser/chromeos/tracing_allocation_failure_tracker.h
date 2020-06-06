// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_TRACING_ALLOCATION_FAILURE_TRACKER_H_
#define CHROME_BROWSER_CHROMEOS_TRACING_ALLOCATION_FAILURE_TRACKER_H_

namespace chromeos {

// Sets up additional debugging around
// ProducerClient::InitSharedMemoryIfNeeded's allocation failures, in order to
// investigate crbug.com/1074115.
// TODO(crbug.com/1074115): Remove this function and this file once
// investigation is complete.
void SetUpTracingAllocatorFailureTracker();

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_TRACING_ALLOCATION_FAILURE_TRACKER_H_
