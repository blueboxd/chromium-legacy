// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_METRIC_KIT_SUBSCRIBING_UTIL_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_METRIC_KIT_SUBSCRIBING_UTIL_H_

// Enables MetricKit payloads collection and saving.
// Payloads will be saved in JSON files in application Documents directory.
//
// Checks if the collection is enabled in experimental settings.
// Changing the value of the setting will require restarting Chrome.
void EnableMetricKitReportCollection();

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_METRIC_KIT_SUBSCRIBING_UTIL_H_
