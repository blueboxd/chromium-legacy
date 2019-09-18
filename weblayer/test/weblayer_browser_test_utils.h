// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_TEST_WEBLAYER_BROWSER_TEST_UTILS_H_
#define WEBLAYER_TEST_WEBLAYER_BROWSER_TEST_UTILS_H_

class GURL;

namespace weblayer {
class Shell;

// Navigates |shell| to |url| and wait for successful navigation.
void NavigateAndWait(const GURL& url, Shell* shell);

}  // namespace weblayer

#endif  // WEBLAYER_TEST_WEBLAYER_BROWSER_TEST_UTILS_H_
