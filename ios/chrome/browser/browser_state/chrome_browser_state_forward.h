// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_STATE_CHROME_BROWSER_STATE_FORWARD_H_
#define IOS_CHROME_BROWSER_BROWSER_STATE_CHROME_BROWSER_STATE_FORWARD_H_

// This file is there to help move ChromeBrowserState to the global
// namespace (i.e. rename it to ChromeBrowserState).

class ChromeBrowserState;
enum class ChromeBrowserStateType;

namespace ios {
using ::ChromeBrowserState;
using ::ChromeBrowserStateType;
}  // namespace ios

#endif  // IOS_CHROME_BROWSER_BROWSER_STATE_CHROME_BROWSER_STATE_FORWARD_H_
