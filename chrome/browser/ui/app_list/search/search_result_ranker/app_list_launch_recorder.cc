// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/app_list_launch_recorder.h"

#include <string>

#include "base/task/post_task.h"

namespace app_list {

using LaunchInfo = AppListLaunchRecorder::LaunchInfo;

AppListLaunchRecorder::LaunchInfo::LaunchInfo(
    metrics::ChromeOSAppListLaunchEventProto::LaunchType launch_type,
    metrics::ChromeOSAppListLaunchEventProto::SearchProviderType
        search_provider_type,
    const std::string& target,
    const std::string& query,
    const std::string& domain,
    const std::string& app)
    : launch_type(launch_type),
      search_provider_type(search_provider_type),
      target(target),
      query(query),
      domain(domain),
      app(app) {}

AppListLaunchRecorder::LaunchInfo::LaunchInfo(const LaunchInfo& other) =
    default;

AppListLaunchRecorder::LaunchInfo::~LaunchInfo() = default;

AppListLaunchRecorder* AppListLaunchRecorder::GetInstance() {
  static base::NoDestructor<AppListLaunchRecorder> recorder;
  return recorder.get();
}

AppListLaunchRecorder::AppListLaunchRecorder() = default;
AppListLaunchRecorder::~AppListLaunchRecorder() = default;

std::unique_ptr<AppListLaunchRecorder::LaunchEventSubscription>
AppListLaunchRecorder::RegisterCallback(const LaunchEventCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return callback_list_.Add(callback);
}

// Notifies all observers of the given |launch_info|. If the
// AppListMetricsProvider has been constructed, this will queue an
// AppListLaunchEvent to be provided to the metrics service.
void AppListLaunchRecorder::Record(const LaunchInfo& launch_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callback_list_.Notify(launch_info);
}

}  // namespace app_list
