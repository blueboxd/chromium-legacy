// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_REPORTING_REPORT_GENERATOR_IOS_H_
#define IOS_CHROME_BROWSER_POLICY_REPORTING_REPORT_GENERATOR_IOS_H_

#include "components/enterprise/browser/reporting/report_generator.h"

namespace enterprise_reporting {

/**
 * iOS implementation of the report generator delegate.
 */
class ReportGeneratorIOS : public ReportGenerator::Delegate {
 public:
  ReportGeneratorIOS() = default;
  ReportGeneratorIOS(const ReportGeneratorIOS&) = delete;
  ReportGeneratorIOS& operator=(const ReportGeneratorIOS&) = delete;
  ~ReportGeneratorIOS() override = default;

  // ReportGenerator::Delegate implementation.
  void SetAndroidAppInfos(
      ReportGenerator::ReportRequest* basic_request) override;
};

}  // namespace enterprise_reporting

#endif  // IOS_CHROME_BROWSER_POLICY_REPORTING_REPORT_GENERATOR_IOS_H_
