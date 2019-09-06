// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/app_list_launch_metrics_provider.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_list_launch_recorder.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_list_launch_recorder_state.pb.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_list_launch_recorder_util.h"
#include "components/metrics/metrics_log.h"
#include "crypto/sha2.h"
#include "third_party/metrics_proto/chrome_os_app_list_launch_event.pb.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace app_list {
namespace {

using LaunchInfo = ::app_list::AppListLaunchRecorder::LaunchInfo;

using ::metrics::ChromeOSAppListLaunchEventProto;
using ::metrics::ChromeUserMetricsExtension;
using ::metrics::MetricsLog;

// Length of the user secret string, in bytes.
constexpr size_t kSecretSize = 32;

// Tries to serialize the given AppListLaunchRecorderStateProto to disk at the
// given |filepath|.
void SaveStateToDisk(const base::FilePath& filepath,
                     const AppListLaunchRecorderStateProto& proto) {
  std::string proto_str;
  if (!proto.SerializeToString(&proto_str)) {
    LOG(DFATAL) << "Error serializing AppListLaunchRecorderStateProto.";
    LogMetricsProviderError(MetricsProviderError::kStateToProtoError);
    return;
  }

  bool write_result = false;
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    write_result = base::ImportantFileWriter::WriteFileAtomically(
        filepath, proto_str, "AppListLaunchMetricsProvider");
  }

  if (!write_result) {
    LOG(DFATAL) << "Error writing AppListLaunchRecorderStateProto.";
    LogMetricsProviderError(MetricsProviderError::kStateWriteError);
  }
}

// Tries to load an |AppListLaunchRecorderStateProto| from the given filepath.
// If it fails, returns nullopt.
base::Optional<AppListLaunchRecorderStateProto> LoadStateFromDisk(
    const base::FilePath& filepath) {
  std::string proto_str;
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    // If the state file doesn't exist, it's not necessarily an error: this may
    // be the first time the provider is being run.
    if (!base::PathExists(filepath))
      return base::nullopt;

    if (!base::ReadFileToString(filepath, &proto_str)) {
      LOG(DFATAL) << "Error reading AppListLaunchRecorderStateProto.";
      LogMetricsProviderError(MetricsProviderError::kStateReadError);
      return base::nullopt;
    }
  }

  AppListLaunchRecorderStateProto proto;
  if (!proto.ParseFromString(proto_str)) {
    LOG(DFATAL) << "Error parsing AppListLaunchRecorderStateProto.";
    LogMetricsProviderError(MetricsProviderError::kStateFromProtoError);
    return base::nullopt;
  }

  return proto;
}

// Returns the file path of the primary user's profile, if it exists and is not
// a guest session or system profile. Otherwise, returns base::nullopt.
base::Optional<base::FilePath> GetProfileDir() {
  // We do not handle multiprofile, events from all logged-in profiles are
  // logged under the primary user.
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  // Only enable if the current profile is a regular profile, not a guest
  // session or the login screen.
  if (!profile || profile->IsGuestSession() || profile->IsSystemProfile())
    return base::nullopt;
  return profile->GetPath();
}

// Generates a cryptographically secure random secret of size |kSecretSize|.
Secret GenerateSecret() {
  const auto secret = base::UnguessableToken::Create().ToString();
  DCHECK_EQ(secret.size(), kSecretSize);
  return {secret};
}

// Generates a random user ID.
uint64_t GenerateUserID() {
  // This is analogous to how the UMA client ID is generated in
  // metrics::MetricsStateManager.
  return MetricsLog::Hash(base::GenerateGUID());
}

// Generates a proto containing a new secret and user ID.
AppListLaunchRecorderStateProto GenerateStateProto() {
  AppListLaunchRecorderStateProto proto;
  proto.set_secret(GenerateSecret().value);
  proto.set_recurrence_ranker_user_id(GenerateUserID());
  return proto;
}

// Returns the user secret stored in |proto|.
Secret GetSecretFromProto(const AppListLaunchRecorderStateProto& proto) {
  DCHECK(proto.has_secret() && !proto.secret().empty());
  return {proto.secret()};
}

// Returns the first 8 bytes of the SHA256 hash of |secret| concatenated with
// |value|.
uint64_t HashWithSecret(const std::string& value, const Secret& secret) {
  DCHECK(!secret.value.empty());
  uint64_t hash;
  crypto::SHA256HashString(secret.value + value, &hash, sizeof(uint64_t));
  return hash;
}

}  // namespace

int AppListLaunchMetricsProvider::kMaxEventsPerUpload = 100;
char AppListLaunchMetricsProvider::kStateProtoFilename[] =
    "app_list_launch_recorder_state.pb";

AppListLaunchMetricsProvider::AppListLaunchMetricsProvider(
    base::RepeatingCallback<base::Optional<base::FilePath>()>
        get_profile_dir_callback)
    : get_profile_dir_callback_(get_profile_dir_callback),
      init_state_(InitState::DISABLED),
      secret_(base::nullopt),
      user_id_(base::nullopt) {}

AppListLaunchMetricsProvider::AppListLaunchMetricsProvider()
    : AppListLaunchMetricsProvider(base::BindRepeating(GetProfileDir)) {}

AppListLaunchMetricsProvider::~AppListLaunchMetricsProvider() = default;

void AppListLaunchMetricsProvider::OnRecordingEnabled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We do not perform actual initialization or set to InitState::ENABLED here.
  // Initialization must occur after the browser thread has finished
  // initializing, which may not be the case when OnRecordingEnabled is
  // called. Instead, initialization happens on the first call to
  // OnAppListLaunch.
  init_state_ = InitState::UNINITIALIZED;

  subscription_ = AppListLaunchRecorder::GetInstance()->RegisterCallback(
      base::BindRepeating(&AppListLaunchMetricsProvider::OnAppListLaunch,
                          weak_factory_.GetWeakPtr()));
}

void AppListLaunchMetricsProvider::OnRecordingDisabled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  init_state_ = InitState::DISABLED;
  launch_info_cache_.clear();
  secret_.reset();
  user_id_.reset();
  subscription_.reset();
}

void AppListLaunchMetricsProvider::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& profile_dir = get_profile_dir_callback_.Run();
  if (!profile_dir) {
    OnRecordingDisabled();
    return;
  }
  const base::FilePath& proto_filepath = profile_dir.value().AppendASCII(
      AppListLaunchMetricsProvider::kStateProtoFilename);

  PostTaskAndReplyWithResult(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&LoadStateFromDisk, proto_filepath),
      base::BindOnce(&AppListLaunchMetricsProvider::OnStateLoaded,
                     weak_factory_.GetWeakPtr(), proto_filepath));
}

void AppListLaunchMetricsProvider::OnStateLoaded(
    const base::FilePath& proto_filepath,
    const base::Optional<AppListLaunchRecorderStateProto>& proto) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (proto) {
    secret_ = GetSecretFromProto(proto.value());
    user_id_ = proto.value().recurrence_ranker_user_id();
  } else {
    // Either there is no state proto saved, or it was corrupt. Generate a new
    // secret and ID regardless. We log an 'error' to UMA despite this not
    // always being an error, because this happening too frequently would
    // indicate an issue with the system.
    LogMetricsProviderError(MetricsProviderError::kNoStateProto);

    AppListLaunchRecorderStateProto new_proto = GenerateStateProto();
    PostTask(FROM_HERE, {base::ThreadPool(), base::MayBlock()},
             base::BindOnce(&SaveStateToDisk, proto_filepath, new_proto));

    secret_ = GetSecretFromProto(new_proto);
    user_id_ = new_proto.recurrence_ranker_user_id();
  }

  if (!user_id_) {
    LogMetricsProviderError(MetricsProviderError::kInvalidUserId);
    OnRecordingDisabled();
  } else if (!secret_ || secret_.value().value.size() != kSecretSize) {
    LogMetricsProviderError(MetricsProviderError::kInvalidSecret);
    OnRecordingDisabled();
  } else {
    init_state_ = InitState::ENABLED;
  }
}

void AppListLaunchMetricsProvider::ProvideCurrentSessionData(
    ChromeUserMetricsExtension* uma_proto) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (init_state_ == InitState::ENABLED) {
    for (const auto& launch_info : launch_info_cache_) {
      CreateLaunchEvent(launch_info,
                        uma_proto->add_chrome_os_app_list_launch_event());
    }
    launch_info_cache_.clear();
  }
}

void AppListLaunchMetricsProvider::OnAppListLaunch(
    const LaunchInfo& launch_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (init_state_ == InitState::UNINITIALIZED) {
    init_state_ = InitState::INIT_STARTED;
    Initialize();
  }

  if (init_state_ == InitState::DISABLED ||
      launch_info_cache_.size() >= static_cast<size_t>(kMaxEventsPerUpload))
    return;

  if (launch_info.launch_type ==
      ChromeOSAppListLaunchEventProto::LAUNCH_TYPE_UNSPECIFIED) {
    LogMetricsProviderError(MetricsProviderError::kLaunchTypeUnspecified);
    return;
  }

  launch_info_cache_.push_back(launch_info);

  // We want the metric to reflect the number of uploads affected, not the
  // number of dropped events, so only log an error when max events is first
  // exceeded.
  if (launch_info_cache_.size() == static_cast<size_t>(kMaxEventsPerUpload))
    LogMetricsProviderError(MetricsProviderError::kMaxEventsPerUploadExceeded);
}

void AppListLaunchMetricsProvider::CreateLaunchEvent(
    const LaunchInfo& launch_info,
    ChromeOSAppListLaunchEventProto* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(secret_ && user_id_);

  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);

  // Unhashed data.
  event->set_recurrence_ranker_user_id(user_id_.value());
  event->set_hour(now.hour);
  event->set_search_query_length(launch_info.query.size());
  event->set_launch_type(launch_info.launch_type);
  event->set_search_provider_type(launch_info.search_provider_type);

  // Hashed data.
  event->set_hashed_target(HashWithSecret(launch_info.target, secret_.value()));
  event->set_hashed_query(HashWithSecret(launch_info.query, secret_.value()));
  event->set_hashed_domain(HashWithSecret(launch_info.domain, secret_.value()));
  event->set_hashed_app(HashWithSecret(launch_info.app, secret_.value()));
}

}  // namespace app_list
