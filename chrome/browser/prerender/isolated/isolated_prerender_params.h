// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_PARAMS_H_
#define CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_PARAMS_H_

#include <stdint.h>

#include "base/optional.h"
#include "base/time/time.h"
#include "url/gurl.h"

// Returns true if the Isolated Prerender feature is enabled.
bool IsolatedPrerenderIsEnabled();

// The maximum number of prefetches that should be done from predictions on a
// Google SRP. nullopt is returned for unlimited. Negative values given by the
// field trial return nullopt.
base::Optional<size_t> IsolatedPrerenderMaximumNumberOfPrefetches();

// Whether idle sockets should be closed after every prefetch.
bool IsolatedPrerenderCloseIdleSockets();

// The amount of time to allow before timing out an origin probe.
base::TimeDelta IsolatedPrerenderProbeTimeout();

// The amount of time to allow a prefetch to take before considering it a
// timeout error.
base::TimeDelta IsolatedPrefetchTimeoutDuration();

#endif  // CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_PARAMS_H_
