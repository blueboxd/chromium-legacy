// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/metric_kit_subscriber.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kChromeMetricKitPayloadsDirectory = @"ChromeMetricKitPayloads";

namespace {

void WriteMetricPayloads(NSArray<MXMetricPayload*>* payloads)
    API_AVAILABLE(ios(13.0)) {
  NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                                       NSUserDomainMask, YES);
  NSString* documents_directory = [paths objectAtIndex:0];
  NSString* metric_kit_report_directory = [documents_directory
      stringByAppendingPathComponent:kChromeMetricKitPayloadsDirectory];
  base::FilePath metric_kit_report_path(
      base::SysNSStringToUTF8(metric_kit_report_directory));
  if (!base::CreateDirectory(metric_kit_report_path)) {
    return;
  }
  NSDateFormatter* formatter = [[NSDateFormatter alloc] init];
  [formatter setDateFormat:@"yyyyMMdd_HHmmss"];
  [formatter setTimeZone:[NSTimeZone timeZoneWithName:@"UTC"]];
  for (MXMetricPayload* payload : payloads) {
    NSDate* end_date = payload.timeStampEnd;
    NSString* file_name = [NSString
        stringWithFormat:@"%@.json", [formatter stringFromDate:end_date]];
    base::FilePath file_path(
        base::SysNSStringToUTF8([metric_kit_report_directory
            stringByAppendingPathComponent:file_name]));
    NSData* file_data = payload.JSONRepresentation;
    base::WriteFile(file_path, static_cast<const char*>(file_data.bytes),
                    file_data.length);
  }
}

}  // namespace

@implementation MetricKitSubscriber

+ (instancetype)sharedInstance {
  static MetricKitSubscriber* instance = [[MetricKitSubscriber alloc] init];
  return instance;
}

- (void)didReceiveMetricPayloads:(NSArray<MXMetricPayload*>*)payloads
    API_AVAILABLE(ios(13.0)) {
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
       base::ThreadPolicy::PREFER_BACKGROUND, base::MayBlock()},
      base::BindOnce(WriteMetricPayloads, payloads));
}

@end
