// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/federated_learning/floc_blocklist_service.h"

#include <utility>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "components/federated_learning/floc_constants.h"
#include "components/federated_learning/proto/blocklist.pb.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace federated_learning {

namespace {

class CopyingFileInputStream : public google::protobuf::io::CopyingInputStream {
 public:
  explicit CopyingFileInputStream(base::File file) : file_(std::move(file)) {}

  CopyingFileInputStream(const CopyingFileInputStream&) = delete;
  CopyingFileInputStream& operator=(const CopyingFileInputStream&) = delete;

  ~CopyingFileInputStream() override = default;

  // google::protobuf::io::CopyingInputStream:
  int Read(void* buffer, int size) override {
    return file_.ReadAtCurrentPosNoBestEffort(static_cast<char*>(buffer), size);
  }

 private:
  base::File file_;
};

FlocBlocklistService::LoadedBlocklist LoadBlockListOnBackgroundThread(
    const base::FilePath& file_path) {
  base::File blocklist_file(file_path,
                            base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!blocklist_file.IsValid())
    return FlocBlocklistService::LoadedBlocklist();

  CopyingFileInputStream copying_stream(std::move(blocklist_file));
  google::protobuf::io::CopyingInputStreamAdaptor zero_copy_stream_adaptor(
      &copying_stream);

  proto::Blocklist blocklist_proto;
  if (!blocklist_proto.ParseFromZeroCopyStream(&zero_copy_stream_adaptor))
    return FlocBlocklistService::LoadedBlocklist();

  std::unordered_set<uint64_t> blocklist;
  for (uint64_t entry : blocklist_proto.entries())
    blocklist.insert(entry);

  return blocklist;
}

}  // namespace

FlocBlocklistService::FlocBlocklistService()
    : background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

FlocBlocklistService::~FlocBlocklistService() = default;

void FlocBlocklistService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FlocBlocklistService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FlocBlocklistService::OnBlocklistFileReady(
    const base::FilePath& file_path) {
  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(&LoadBlockListOnBackgroundThread, file_path),
      base::BindOnce(&FlocBlocklistService::OnBlocklistLoadResult,
                     AsWeakPtr()));
}

void FlocBlocklistService::SetBackgroundTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner) {
  background_task_runner_ = background_task_runner;
}

bool FlocBlocklistService::BlocklistLoaded() const {
  return loaded_blocklist_.has_value();
}

bool FlocBlocklistService::ShouldBlockFloc(uint64_t floc_id) const {
  // If the blocklist hasn't been loaded or if there was a load failure, we
  // block all flocs.
  if (!loaded_blocklist_)
    return true;

  return loaded_blocklist_->find(floc_id) != loaded_blocklist_->end();
}

void FlocBlocklistService::OnBlocklistLoadResult(LoadedBlocklist blocklist) {
  loaded_blocklist_ = std::move(blocklist);

  if (loaded_blocklist_) {
    for (auto& observer : observers_)
      observer.OnBlocklistLoaded();
  }
}

}  // namespace federated_learning
