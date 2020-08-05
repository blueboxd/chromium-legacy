// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/reporting/reporting_delegate_factory_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace enterprise_reporting {

std::unique_ptr<BrowserReportGenerator::Delegate>
ReportingDelegateFactoryIOS::GetBrowserReportGeneratorDelegate() {
  // TODO(crbug.com/1066495): Finish iOS CBCM implementation.
  return nullptr;
}

std::unique_ptr<ProfileReportGenerator::Delegate>
ReportingDelegateFactoryIOS::GetProfileReportGeneratorDelegate() {
  // TODO(crbug.com/1066495): Finish iOS CBCM implementation.
  return nullptr;
}

std::unique_ptr<ReportGenerator::Delegate>
ReportingDelegateFactoryIOS::GetReportGeneratorDelegate() {
  // TODO(crbug.com/1066495): Finish iOS CBCM implementation.
  return nullptr;
}

std::unique_ptr<ReportScheduler::Delegate>
ReportingDelegateFactoryIOS::GetReportSchedulerDelegate() {
  // TODO(crbug.com/1066495): Finish iOS CBCM implementation.
  return nullptr;
}

}  // namespace enterprise_reporting
