// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REPUTATION_URL_ELISION_POLICY_H_
#define CHROME_BROWSER_REPUTATION_URL_ELISION_POLICY_H_

class GURL;

// Called by omnibox code (when enabled) to check whether |url| should be elided
// to show just the eTLD+1 due to failing any number of heuristics.
bool ShouldElideToRegistrableDomain(const GURL& url);

#endif  // CHROME_BROWSER_REPUTATION_URL_ELISION_POLICY_H_
