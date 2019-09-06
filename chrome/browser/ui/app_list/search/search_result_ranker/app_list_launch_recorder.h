// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LIST_LAUNCH_RECORDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LIST_LAUNCH_RECORDER_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "third_party/metrics_proto/chrome_os_app_list_launch_event.pb.h"

namespace app_list {

class SearchController;

// AppListLaunchRecorder is a singleton that can be called to send hashed
// logging events to UMA. Typical usage is:
//
//   AppListLaunchRecorder::GetInstance()->Record(my_data);
//
// This should only be called from the browser UI thread.
class AppListLaunchRecorder {
 public:
  // Stores information about a single launch event to be logged by a call to
  // Record.
  struct LaunchInfo {
    LaunchInfo(metrics::ChromeOSAppListLaunchEventProto::LaunchType launch_type,
               metrics::ChromeOSAppListLaunchEventProto::SearchProviderType
                   search_provider_type,
               const std::string& target,
               const std::string& query,
               const std::string& domain,
               const std::string& app);
    LaunchInfo(const LaunchInfo& other);
    ~LaunchInfo();

    // Specifies which UI component this event was launched from.
    metrics::ChromeOSAppListLaunchEventProto::LaunchType launch_type;
    // Specifies which search provider created this event's result.
    metrics::ChromeOSAppListLaunchEventProto::SearchProviderType
        search_provider_type;
    // A string identifier of the item being launched, eg. an app ID or
    // filepath.
    std::string target;
    // The search query at the time of launch. If this is a zero-state launch
    // (eg. from the suggested chips), this should be the empty string.
    std::string query;
    // The last-visited domain at the time of launch.
    std::string domain;
    // The app ID of the last-opened app at the time of launch.
    std::string app;
  };

  using LaunchEventCallback =
      base::RepeatingCallback<void(const AppListLaunchRecorder::LaunchInfo&)>;
  using LaunchEventSubscription = base::CallbackList<void(
      const AppListLaunchRecorder::LaunchInfo&)>::Subscription;

  // Returns the instance of AppListLaunchRecorder.
  static AppListLaunchRecorder* GetInstance();

  // Registers a callback to be invoked on a call to Record().
  std::unique_ptr<LaunchEventSubscription> RegisterCallback(
      const LaunchEventCallback& callback);

 private:
  friend class base::NoDestructor<AppListLaunchRecorder>;

  // These are the clients of hashed logging:
  friend class app_list::SearchController;

  // Adds |launch_info| to the cache of launches to be hashed and provided to
  // the metrics service on a call to ProvideCurrentSessionData.
  void Record(const LaunchInfo& launch_info);

  AppListLaunchRecorder();
  ~AppListLaunchRecorder();

  base::CallbackList<void(const LaunchInfo&)> callback_list_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(AppListLaunchRecorder);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LIST_LAUNCH_RECORDER_H_
