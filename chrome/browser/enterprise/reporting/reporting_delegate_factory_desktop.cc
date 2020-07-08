// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_desktop.h"

#include "chrome/browser/enterprise/reporting/browser_report_generator_desktop.h"

namespace enterprise_reporting {

std::unique_ptr<BrowserReportGenerator::Delegate>
ReportingDelegateFactoryDesktop::GetBrowserReportGeneratorDelegate() {
  return std::make_unique<BrowserReportGeneratorDesktop>();
}

}  // namespace enterprise_reporting
