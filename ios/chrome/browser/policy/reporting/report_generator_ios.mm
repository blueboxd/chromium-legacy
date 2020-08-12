// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/reporting/report_generator_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace enterprise_reporting {

void ReportGeneratorIOS::SetAndroidAppInfos(
    ReportGenerator::ReportRequest* basic_request) {
  // Not used on iOS.
}

}  // namespace enterprise_reporting
