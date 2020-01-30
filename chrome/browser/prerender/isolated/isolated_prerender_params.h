// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_PARAMS_H_
#define CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_PARAMS_H_

#include <stdint.h>

#include "base/optional.h"
#include "url/gurl.h"

// Returns true if the Isolated Prerender feature is enabled.
bool IsolatedPrerenderIsEnabled();

// Returns the URL of the proxy server to use in isolated prerenders, if any.
base::Optional<GURL> IsolatedPrerenderProxyServer();

// Returns true if the proxy for Isolated Prerenders should replace the DRP
// custom proxy.
bool IsolatedPrerenderShouldReplaceDataReductionCustomProxy();

// The maximum number of prefetches that should be done from predictions on a
// Google SRP. nullopt is returned for unlimited. Negative values given by the
// field trial return nullopt.
base::Optional<size_t> IsolatedPrerenderMaximumNumberOfPrefetches();

#endif  // CHROME_BROWSER_PRERENDER_ISOLATED_ISOLATED_PRERENDER_PARAMS_H_
