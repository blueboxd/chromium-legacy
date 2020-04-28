// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_TEST_DEVTOOLS_LIST_FETCHER_H_
#define FUCHSIA_BASE_TEST_DEVTOOLS_LIST_FETCHER_H_

#include "base/values.h"

namespace cr_fuchsia {

// Outside of anonymous namespace so deprecated net::URLFetcher can friend it
class DevToolsListFetcher;

// Returns the JSON value of the list URL for the DevTools service listening
// on port |port| on localhost. Returns an empty value on error.
base::Value GetDevToolsListFromPort(uint16_t port);

}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_TEST_DEVTOOLS_LIST_FETCHER_H_
