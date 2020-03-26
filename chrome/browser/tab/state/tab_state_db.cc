// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/state/tab_state_db.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace {

const char kTabStateDBFolder[] = "tab_state_db";
leveldb::ReadOptions CreateReadOptions() {
  leveldb::ReadOptions opts;
  opts.fill_cache = false;
  return opts;
}

bool DatabasePrefixFilter(const std::string& key_prefix,
                          const std::string& key) {
  return base::StartsWith(key, key_prefix, base::CompareCase::SENSITIVE);
}

}  // namespace

TabStateDB::TabStateDB(
    leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
    const base::FilePath& profile_directory,
    base::OnceClosure closure)
    : database_status_(leveldb_proto::Enums::InitStatus::kNotInitialized),
      storage_database_(
          proto_database_provider->GetDB<tab_state_db::TabStateContentProto>(
              leveldb_proto::ProtoDbType::TAB_STATE_DATABASE,
              profile_directory.AppendASCII(kTabStateDBFolder),
              base::CreateSequencedTaskRunner(
                  {base::ThreadPool(), base::MayBlock(),
                   base::TaskPriority::USER_VISIBLE}))) {
  storage_database_->Init(base::BindOnce(&TabStateDB::OnDatabaseInitialized,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         std::move(closure)));
}

TabStateDB::~TabStateDB() = default;

bool TabStateDB::IsInitialized() const {
  return database_status_ == leveldb_proto::Enums::InitStatus::kOK;
}

void TabStateDB::LoadContent(const std::string& key, LoadCallback callback) {
  storage_database_->LoadEntriesWithFilter(
      base::BindRepeating(&DatabasePrefixFilter, key), CreateReadOptions(),
      /* target_prefix */ "",
      base::BindOnce(&TabStateDB::OnLoadContent, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void TabStateDB::InsertContent(const std::string& key,
                               const std::vector<uint8_t>& value,
                               OperationCallback callback) {
  auto contents_to_save = std::make_unique<ContentEntry>();
  tab_state_db::TabStateContentProto proto;
  proto.set_key(key);
  proto.set_content_data(value.data(), value.size());
  contents_to_save->emplace_back(proto.key(), std::move(proto));
  storage_database_->UpdateEntries(
      std::move(contents_to_save), std::make_unique<std::vector<std::string>>(),
      base::BindOnce(&TabStateDB::OnOperationCommitted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void TabStateDB::DeleteContent(const std::string& key,
                               OperationCallback callback) {
  storage_database_->UpdateEntriesWithRemoveFilter(
      std::make_unique<ContentEntry>(),
      std::move(base::BindRepeating(&DatabasePrefixFilter, std::move(key))),
      base::BindOnce(&TabStateDB::OnOperationCommitted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void TabStateDB::DeleteAllContent(OperationCallback callback) {
  storage_database_->Destroy(std::move(callback));
}

TabStateDB::TabStateDB(
    std::unique_ptr<leveldb_proto::ProtoDatabase<
        tab_state_db::TabStateContentProto>> storage_database,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::OnceClosure closure)
    : database_status_(leveldb_proto::Enums::InitStatus::kNotInitialized),
      storage_database_(std::move(storage_database)) {
  storage_database_->Init(base::BindOnce(&TabStateDB::OnDatabaseInitialized,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         std::move(closure)));
}

void TabStateDB::OnDatabaseInitialized(
    base::OnceClosure closure,
    leveldb_proto::Enums::InitStatus status) {
  DCHECK_EQ(database_status_,
            leveldb_proto::Enums::InitStatus::kNotInitialized);
  database_status_ = status;
  std::move(closure).Run();
}

void TabStateDB::OnLoadContent(
    LoadCallback callback,
    bool success,
    std::unique_ptr<std::vector<tab_state_db::TabStateContentProto>> content) {
  std::vector<KeyAndValue> results;
  if (success) {
    for (const auto& proto : *content) {
      DCHECK(proto.has_key());
      DCHECK(proto.has_content_data());
      results.emplace_back(proto.key(),
                           std::vector<uint8_t>(proto.content_data().begin(),
                                                proto.content_data().end()));
    }
  }
  std::move(callback).Run(success, std::move(results));
}

void TabStateDB::OnOperationCommitted(OperationCallback callback,
                                      bool success) {
  std::move(callback).Run(success);
}
