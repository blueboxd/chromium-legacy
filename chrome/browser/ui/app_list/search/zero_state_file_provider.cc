// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/zero_state_file_provider.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_notifier.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.h"
#include "chrome/browser/ui/app_list/search/zero_state_file_result.h"

using file_manager::file_tasks::FileTasksObserver;

namespace app_list {
namespace {

using StringResults = std::vector<std::pair<std::string, float>>;
using FilePathResults = std::vector<std::pair<base::FilePath, float>>;

constexpr int kMaxLocalFiles = 10;

// Given the output of RecurrenceRanker::RankTopN, filter out all results that
// don't exist on disk. Returns the filtered results, with each result string
// converted to a base::FilePath.
FilePathResults FilterNonexistentFiles(const StringResults& ranker_results) {
  FilePathResults results;
  for (const auto& path_score : ranker_results) {
    // We use FilePath::FromUTF8Unsafe to decode the filepath string. As per its
    // documentation, this is a safe use of the function because
    // ZeroStateFileProvider is only used on ChromeOS, for which
    // filepaths are UTF8.
    const auto& path = base::FilePath::FromUTF8Unsafe(path_score.first);
    if (base::PathExists(path))
      results.emplace_back(path, path_score.second);
  }
  return results;
}

}  // namespace

ZeroStateFileProvider::ZeroStateFileProvider(Profile* profile)
    : profile_(profile), file_tasks_observer_(this), weak_factory_(this) {
  DCHECK(profile_);
  task_runner_ = base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::TaskPriority::BEST_EFFORT, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  // TODO(crbug.com/959679): Add a metric for if this succeeds or fails.
  if (auto* notifier =
          file_manager::file_tasks::FileTasksNotifier::GetForProfile(
              profile_)) {
    file_tasks_observer_.Add(notifier);

    RecurrenceRankerConfigProto config;
    config.set_min_seconds_between_saves(300u);
    config.set_condition_limit(1u);
    config.set_condition_decay(0.5f);
    config.set_target_limit(200);
    config.set_target_decay(0.9f);
    config.mutable_predictor()->mutable_default_predictor();
    files_ranker_ = std::make_unique<RecurrenceRanker>(
        "ZeroStateLocalFiles",
        profile->GetPath().AppendASCII("zero_state_local_files.pb"), config,
        chromeos::ProfileHelper::IsEphemeralUserProfile(profile));
  }
}

ZeroStateFileProvider::~ZeroStateFileProvider() = default;

void ZeroStateFileProvider::Start(const base::string16& query) {
  // TODO(crbug.com/959679): Add latency metrics.
  ClearResultsSilently();
  if (!query.empty())
    return;

  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&FilterNonexistentFiles,
                     files_ranker_->RankTopN(kMaxLocalFiles)),
      base::BindOnce(&ZeroStateFileProvider::SetSearchResults,
                     weak_factory_.GetWeakPtr()));
}

void ZeroStateFileProvider::SetSearchResults(FilePathResults results) {
  SearchProvider::Results new_results;
  for (const auto& filepath_score : results) {
    new_results.emplace_back(std::make_unique<ZeroStateFileResult>(
        filepath_score.first, filepath_score.second, profile_));
  }
  SwapResults(&new_results);
}

void ZeroStateFileProvider::OnFilesOpened(
    const std::vector<FileOpenEvent>& file_opens) {
  // TODO(crbug.com/959679): Filter out DriveFS files.
  if (!files_ranker_)
    return;
  for (const auto& file_open : file_opens)
    files_ranker_->Record(file_open.path.value());
}

}  // namespace app_list
