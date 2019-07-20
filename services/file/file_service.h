// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_FILE_FILE_SERVICE_H_
#define SERVICES_FILE_FILE_SERVICE_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "components/services/filesystem/lock_table.h"
#include "components/services/leveldb/public/mojom/leveldb.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/file/public/mojom/file_system.mojom.h"
#include "services/service_manager/public/cpp/binder_map.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace file {

class FileService : public service_manager::Service {
 public:
  explicit FileService(
      mojo::PendingReceiver<service_manager::mojom::Service> receiver);
  ~FileService() override;

  service_manager::BinderMapWithContext<const service_manager::Identity&>&
  GetBinderMapForTesting() {
    return binders_;
  }

 private:
  // service_manager::Service:
  void OnStart() override;
  void OnConnect(const service_manager::ConnectSourceInfo& source,
                 const std::string& interface_name,
                 mojo::ScopedMessagePipeHandle receiver_pipe) override;

  void BindFileSystemReceiver(
      const service_manager::Identity& remote_identity,
      mojo::PendingReceiver<mojom::FileSystem> receiver);
  void BindLevelDBServiceReceiver(
      const service_manager::Identity& remote_identity,
      mojo::PendingReceiver<leveldb::mojom::LevelDBService> receiver);

  service_manager::ServiceBinding service_binding_;

  scoped_refptr<base::SequencedTaskRunner> file_service_runner_;
  scoped_refptr<base::SequencedTaskRunner> leveldb_service_runner_;

  // We create these two objects so we can delete them on the correct task
  // runners.
  class FileSystemObjects;
  std::unique_ptr<FileSystemObjects> file_system_objects_;

  class LevelDBServiceObjects;
  std::unique_ptr<LevelDBServiceObjects> leveldb_objects_;

  service_manager::BinderMapWithContext<const service_manager::Identity&>
      binders_;

  DISALLOW_COPY_AND_ASSIGN(FileService);
};

}  // namespace file

#endif  // SERVICES_FILE_FILE_SERVICE_H_
