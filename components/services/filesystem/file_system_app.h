// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_FILESYSTEM_FILE_SYSTEM_APP_H_
#define COMPONENTS_SERVICES_FILESYSTEM_FILE_SYSTEM_APP_H_

#include "base/macros.h"
#include "components/services/filesystem/directory_impl.h"
#include "components/services/filesystem/file_system_impl.h"
#include "components/services/filesystem/lock_table.h"
#include "components/services/filesystem/public/mojom/file_system.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"

namespace filesystem {

class FileSystemApp : public service_manager::Service {
 public:
  explicit FileSystemApp(
      mojo::PendingReceiver<service_manager::mojom::Service> receiver);
  ~FileSystemApp() override;

 private:
  // Gets the system specific toplevel profile directory.
  static base::FilePath GetUserDataDir();

  // |service_manager::Service| override:
  void OnConnect(const service_manager::ConnectSourceInfo& source_info,
                 const std::string& interface_name,
                 mojo::ScopedMessagePipeHandle receiver_pipe) override;

  service_manager::ServiceBinding service_binding_;
  scoped_refptr<LockTable> lock_table_;
  mojo::UniqueReceiverSet<mojom::FileSystem> file_systems_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemApp);
};

}  // namespace filesystem

#endif  // COMPONENTS_SERVICES_FILESYSTEM_FILE_SYSTEM_APP_H_
