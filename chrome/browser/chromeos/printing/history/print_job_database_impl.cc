// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/history/print_job_database_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/chromeos/printing/history/print_job_info.pb.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace chromeos {

namespace {

using EntryVector =
    leveldb_proto::ProtoDatabase<printing::proto::PrintJobInfo>::KeyEntryVector;

const base::FilePath::CharType kPrintJobDatabaseName[] =
    FILE_PATH_LITERAL("PrintJobDatabase");

const int kMaxInitializeAttempts = 3;

}  // namespace

PrintJobDatabaseImpl::PrintJobDatabaseImpl(
    leveldb_proto::ProtoDatabaseProvider* database_provider,
    base::FilePath profile_path) {
  auto print_job_database_path = profile_path.Append(kPrintJobDatabaseName);

  scoped_refptr<base::SequencedTaskRunner> database_task_runner =
      base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock(),
                                       base::TaskPriority::BEST_EFFORT});

  database_ = database_provider->GetDB<printing::proto::PrintJobInfo>(
      leveldb_proto::ProtoDbType::PRINT_JOB_DATABASE, print_job_database_path,
      database_task_runner);
}

PrintJobDatabaseImpl::~PrintJobDatabaseImpl() {}

void PrintJobDatabaseImpl::Initialize(InitializeCallback callback) {
  database_->Init(base::BindOnce(&PrintJobDatabaseImpl::OnInitialized,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 std::move(callback)));
}

bool PrintJobDatabaseImpl::IsInitialized() {
  return is_initialized_;
}

void PrintJobDatabaseImpl::SavePrintJob(
    const printing::proto::PrintJobInfo& print_job_info,
    SavePrintJobCallback callback) {
  if (!is_initialized_) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }
  auto entries_to_save = std::make_unique<EntryVector>();
  entries_to_save->push_back(
      std::make_pair(print_job_info.id(), print_job_info));
  database_->UpdateEntries(
      /*entries_to_save=*/std::move(entries_to_save),
      /*keys_to_remove=*/std::make_unique<std::vector<std::string>>(),
      base::BindOnce(&PrintJobDatabaseImpl::OnPrintJobSaved,
                     weak_ptr_factory_.GetWeakPtr(), print_job_info,
                     std::move(callback)));
}

void PrintJobDatabaseImpl::DeletePrintJob(const std::string& id,
                                          DeletePrintJobCallback callback) {
  if (!is_initialized_) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }
  auto keys_to_remove = std::make_unique<std::vector<std::string>>();
  keys_to_remove->push_back(id);
  database_->UpdateEntries(
      /*entries_to_save=*/std::make_unique<EntryVector>(),
      /*keys_to_remove=*/std::move(keys_to_remove),
      base::BindOnce(&PrintJobDatabaseImpl::OnPrintJobDeleted,
                     weak_ptr_factory_.GetWeakPtr(), id, std::move(callback)));
}

void PrintJobDatabaseImpl::GetPrintJobs(GetPrintJobsCallback callback) {
  auto entries = std::make_unique<std::vector<printing::proto::PrintJobInfo>>();
  for (const auto& pair : cache_)
    entries->emplace_back(pair.second);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true, std::move(entries)));
}

void PrintJobDatabaseImpl::OnInitialized(
    InitializeCallback callback,
    leveldb_proto::Enums::InitStatus status) {
  switch (status) {
    case leveldb_proto::Enums::InitStatus::kError:
      if (initialize_attempts_ < kMaxInitializeAttempts) {
        initialize_attempts_++;
        database_->Init(base::BindOnce(&PrintJobDatabaseImpl::OnInitialized,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       std::move(callback)));
      } else {
        FinishInitialization(std::move(callback), false);
      }
      break;
    case leveldb_proto::Enums::InitStatus::kOK:
      database_->LoadKeysAndEntries(
          base::BindOnce(&PrintJobDatabaseImpl::OnKeysAndEntriesLoaded,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
      break;
    case leveldb_proto::Enums::InitStatus::kInvalidOperation:
    case leveldb_proto::Enums::InitStatus::kNotInitialized:
    case leveldb_proto::Enums::InitStatus::kCorrupt:
      NOTREACHED();
      break;
  }
}

void PrintJobDatabaseImpl::OnKeysAndEntriesLoaded(
    InitializeCallback callback,
    bool success,
    std::unique_ptr<std::map<std::string, printing::proto::PrintJobInfo>>
        entries) {
  is_initialized_ = success;
  if (success)
    cache_.insert(entries->begin(), entries->end());
  FinishInitialization(std::move(callback), success);
}

void PrintJobDatabaseImpl::FinishInitialization(InitializeCallback callback,
                                                bool success) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), success));
}

void PrintJobDatabaseImpl::OnPrintJobSaved(
    const printing::proto::PrintJobInfo& print_job_info,
    SavePrintJobCallback callback,
    bool success) {
  if (success)
    cache_[print_job_info.id()] = print_job_info;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), success));
}

void PrintJobDatabaseImpl::OnPrintJobDeleted(const std::string& id,
                                             DeletePrintJobCallback callback,
                                             bool success) {
  if (success)
    cache_.erase(id);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), success));
}

void PrintJobDatabaseImpl::GetPrintJobsFromProtoDatabase(
    GetPrintJobsCallback callback) {
  if (!is_initialized_) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false, nullptr));
    return;
  }
  database_->LoadEntries(
      base::BindOnce(&PrintJobDatabaseImpl::OnPrintJobsRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PrintJobDatabaseImpl::OnPrintJobsRetrieved(
    GetPrintJobsCallback callback,
    bool success,
    std::unique_ptr<std::vector<printing::proto::PrintJobInfo>> entries) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), success, std::move(entries)));
}

}  // namespace chromeos
