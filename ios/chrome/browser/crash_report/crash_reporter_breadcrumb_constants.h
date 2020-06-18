// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_REPORTER_BREADCRUMB_CONSTANTS_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_REPORTER_BREADCRUMB_CONSTANTS_H_

// The maximum string length for breadcrumbs data. The breadcrumbs size cannot
// be larger than the maximum length of a single Breakpad product data value
// (currently 2550 bytes). This value should be large enough to include enough
//  events so that they are useful for diagnosing crashes.
constexpr unsigned long kMaxBreadcrumbsDataLength = 1530;

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_CRASH_REPORTER_BREADCRUMB_CONSTANTS_H_
