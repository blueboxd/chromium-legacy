// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/test/fake_leveldb_service.h"

namespace content {
namespace test {

FakeLevelDBService::OpenRequest::OpenRequest() = default;
FakeLevelDBService::OpenRequest::OpenRequest(OpenRequest&&) = default;
FakeLevelDBService::OpenRequest::OpenRequest(
    bool in_memory,
    std::string dbname,
    std::string memenv_tracking_name,
    mojo::PendingAssociatedReceiver<leveldb::mojom::LevelDBDatabase> receiver,
    OpenCallback callback)
    : in_memory(in_memory),
      dbname(std::move(dbname)),
      memenv_tracking_name(std::move(memenv_tracking_name)),
      receiver(std::move(receiver)),
      callback(std::move(callback)) {}
FakeLevelDBService::OpenRequest::~OpenRequest() = default;

FakeLevelDBService::FakeLevelDBService() = default;
FakeLevelDBService::~FakeLevelDBService() = default;

void FakeLevelDBService::Open(
    const base::FilePath& directory,
    const std::string& dbname,
    const base::Optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    mojo::PendingAssociatedReceiver<leveldb::mojom::LevelDBDatabase> receiver,
    OpenCallback callback) {
  open_requests_.emplace_back(false, dbname, "", std::move(receiver),
                              std::move(callback));
  if (on_open_callback_)
    std::move(on_open_callback_).Run();
}

void FakeLevelDBService::OpenWithOptions(
    const leveldb_env::Options& options,
    const base::FilePath& directory,
    const std::string& dbname,
    const base::Optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    mojo::PendingAssociatedReceiver<leveldb::mojom::LevelDBDatabase> receiver,
    OpenCallback callback) {
  open_requests_.emplace_back(false, dbname, "", std::move(receiver),
                              std::move(callback));
  if (on_open_callback_)
    std::move(on_open_callback_).Run();
}

void FakeLevelDBService::OpenInMemory(
    const base::Optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    const std::string& tracking_name,
    mojo::PendingAssociatedReceiver<leveldb::mojom::LevelDBDatabase> receiver,
    OpenCallback callback) {
  open_requests_.emplace_back(true, "", tracking_name, std::move(receiver),
                              std::move(callback));
  if (on_open_callback_)
    std::move(on_open_callback_).Run();
}

void FakeLevelDBService::Destroy(const base::FilePath& directory,
                                 const std::string& dbname,
                                 DestroyCallback callback) {
  destroy_requests_.push_back({dbname});
  std::move(callback).Run(leveldb::mojom::DatabaseError::OK);
}

void FakeLevelDBService::Bind(
    mojo::PendingReceiver<leveldb::mojom::LevelDBService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void FakeLevelDBService::FlushBindingsForTesting() {
  receivers_.FlushForTesting();
}

}  // namespace test
}  // namespace content
