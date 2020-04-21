// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/metric_kit_subscriber.h"

#import <Foundation/Foundation.h>
#import <MetricKit/MetricKit.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/ios/wait_util.h"
#include "base/test/task_environment.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
base::FilePath MetricKitReportDirectory() {
  NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                                       NSUserDomainMask, YES);
  NSString* documents_directory = [paths objectAtIndex:0];
  NSString* metric_kit_report_directory = [documents_directory
      stringByAppendingPathComponent:kChromeMetricKitPayloadsDirectory];

  return base::FilePath(base::SysNSStringToUTF8(metric_kit_report_directory));
}
}

class MetricKitSubscriberTest : public PlatformTest {
 public:
  MetricKitSubscriberTest() {
    base::DeleteFile(MetricKitReportDirectory(), true);
  }

  ~MetricKitSubscriberTest() override {
    base::DeleteFile(MetricKitReportDirectory(), true);
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

// Tests that Metrickit reports are correctly saved in the document directory.
TEST_F(MetricKitSubscriberTest, SaveReport) {
  if (@available(iOS 13, *)) {
    id mock_report = OCMClassMock([MXMetricPayload class]);
    NSDate* date = [NSDate date];
    std::string file_data("report content");
    NSData* data = [NSData dataWithBytes:file_data.c_str()
                                  length:file_data.size()];
    NSDateFormatter* formatter = [[NSDateFormatter alloc] init];
    [formatter setDateFormat:@"yyyyMMdd_HHmmss"];
    [formatter setTimeZone:[NSTimeZone timeZoneWithName:@"UTC"]];
    NSString* file_name =
        [NSString stringWithFormat:@"%@.json", [formatter stringFromDate:date]];

    base::FilePath file_path =
        MetricKitReportDirectory().Append(base::SysNSStringToUTF8(file_name));
    OCMStub([mock_report timeStampEnd]).andReturn(date);
    OCMStub([mock_report JSONRepresentation]).andReturn(data);
    NSArray* array = @[ mock_report ];
    [[MetricKitSubscriber sharedInstance] didReceiveMetricPayloads:array];

    EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForFileOperationTimeout, ^bool() {
          base::RunLoop().RunUntilIdle();
          return base::PathExists(file_path);
        }));
    std::string content;
    base::ReadFileToString(file_path, &content);
    EXPECT_EQ(content, file_data);
  }
}
