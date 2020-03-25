// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_REQUEST_DEFINITION_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_REQUEST_DEFINITION_H_

#include "build/build_config.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace enterprise_reporting {

namespace definition {

// Both ChromeOsUserReportRequest and ChromeDesktopReportRequest are used to
// upload usage data to DM Server. By the reference to this macro, most classes
// in enterprise_reporting namespace can share the same logic for various
// operation systems.
#if defined(OS_CHROMEOS)
using ReportRequest = enterprise_management::ChromeOsUserReportRequest;
#else
using ReportRequest = enterprise_management::ChromeDesktopReportRequest;
#endif

}  // namespace definition

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_REQUEST_DEFINITION_H_
