// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LIST_LAUNCH_METRICS_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LIST_LAUNCH_METRICS_PROVIDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_list_launch_recorder.h"
#include "components/metrics/metrics_provider.h"
#include "third_party/metrics_proto/chrome_os_app_list_launch_event.pb.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

class ChromeMetricsServiceClient;

namespace app_list {

// Stores a user's random secret. This struct exists to make clear at the type
// system level what is a secret and what isn't.
struct Secret {
  std::string value;
};

class AppListLaunchMetricsProviderTest;
class AppListLaunchRecorderStateProto;

// AppListLaunchMetricsProvider is responsible for filling out the
// |app_list_launch_event| section of the UMA proto. This class should not be
// instantiated directly except by the ChromeMetricsServiceClient. Instead,
// logging events should be sent to AppListLaunchRecorder.
class AppListLaunchMetricsProvider : public metrics::MetricsProvider {
 public:
  ~AppListLaunchMetricsProvider() override;

  // metrics::MetricsProvider:
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  friend class ::ChromeMetricsServiceClient;
  friend class app_list::AppListLaunchMetricsProviderTest;
  FRIEND_TEST_ALL_PREFIXES(AppListLaunchMetricsProviderTest, EventsAreCapped);

  // Filename for the the state proto within the user's home directory. This is
  // just the basename, not the full path.
  static char kStateProtoFilename[];

  // Beyond this number of OnAppListLaunch calls between successive calls to
  // ProvideCurrentSessionData, we stop logging.
  static int kMaxEventsPerUpload;

  enum class InitState { UNINITIALIZED, INIT_STARTED, ENABLED, DISABLED };

  // The constructors are private so that this class can only be instantied by
  // ChromeMetricsServiceClient and tests. The class has two constructors so
  // that the tests can override the profile dir.
  AppListLaunchMetricsProvider();

  AppListLaunchMetricsProvider(
      base::RepeatingCallback<base::Optional<base::FilePath>()>
          get_profile_dir_callback);

  // Records the information in |launch_info|, to be converted into a hashed
  // form and logged to UMA. Actual logging occurs periodically, so it is not
  // guaranteed that any call to this method will be logged.
  //
  // OnAppListLaunch should only be called from the browser thread, after it has
  // finished initializing.
  void OnAppListLaunch(const AppListLaunchRecorder::LaunchInfo& launch_info);

  // Starts initialization if needed. This involves reading the secret from
  // disk.
  void Initialize();

  // Populates this object's internal state from the given state |proto|, and
  // finishes initialisation. This is called by Initialize() and passed the
  // state proto that has been loaded from disk, if it exists. OnStateLoaded may
  // write a new proto to disk if, for example, the given |proto| is invalid.
  void OnStateLoaded(
      const base::FilePath& proto_filepath,
      const base::Optional<AppListLaunchRecorderStateProto>& proto);

  // Converts |launch_info| into a hashed |event| proto ready for logging.
  void CreateLaunchEvent(const AppListLaunchRecorder::LaunchInfo& launch_info,
                         metrics::ChromeOSAppListLaunchEventProto* event);

  // A function that returns the directory of the current profile. This is a
  // callback so that it can be faked out for testing.
  const base::RepeatingCallback<base::Optional<base::FilePath>()>
      get_profile_dir_callback_;

  // Subscription for receiving logging event callbacks from HashedLogger.
  std::unique_ptr<AppListLaunchRecorder::LaunchEventSubscription> subscription_;

  // Cache of input launch data, to be hashed and returned when
  // ProvideCurrentSessionData() is called.
  std::vector<AppListLaunchRecorder::LaunchInfo> launch_info_cache_;

  // The initialization state of the AppListLaunchMetricsProvider. Events are
  // only provided on a call to ProvideCurrentSessionData() once this is
  // enabled, and nothing is recorded if this is disabled.
  InitState init_state_;

  // Secret per-user salt concatenated with values before hashing. Serialized
  // to disk.
  base::Optional<Secret> secret_;

  // Per-user per-client ID used only for AppListLaunchMetricsProvider.
  // Serialized to disk.
  base::Optional<uint64_t> user_id_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AppListLaunchMetricsProvider> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppListLaunchMetricsProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LIST_LAUNCH_METRICS_PROVIDER_H_
