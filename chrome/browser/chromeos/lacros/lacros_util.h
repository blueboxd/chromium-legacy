// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LACROS_LACROS_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_LACROS_LACROS_UTIL_H_

namespace version_info {
enum class Channel;
}  // namespace version_info

namespace lacros_util {

// Returns true if lacros is allowed for the current user type, chrome channel,
// etc.
bool IsLacrosAllowed();

// As above, but takes a channel. Exposed for testing.
bool IsLacrosAllowed(version_info::Channel channel);

}  // namespace lacros_util

#endif  // CHROME_BROWSER_CHROMEOS_LACROS_LACROS_UTIL_H_
