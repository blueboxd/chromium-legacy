// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SMBFS_SMBFS_HOST_H_
#define CHROMEOS_COMPONENTS_SMBFS_SMBFS_HOST_H_

#include <memory>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "chromeos/components/smbfs/mojom/smbfs.mojom.h"
#include "chromeos/disks/mount_point.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace smbfs {

// SmbFsHost is a connection to a running instance of smbfs. It exposes methods
// provided by smbfs over Mojo (eg. server-side copy), and provides access to
// the host from smbfs using the Delegate interface. Destroying SmbFsHost will
// unmount and clean up the smbfs instance.
class COMPONENT_EXPORT(SMBFS) SmbFsHost {
 public:
  class Delegate {
   public:
    virtual ~Delegate();

    // Notification that the smbfs process is no longer connected via Mojo.
    virtual void OnDisconnected() = 0;
  };

  SmbFsHost(std::unique_ptr<chromeos::disks::MountPoint> mount_point,
            Delegate* delegate,
            mojo::Remote<mojom::SmbFs> smbfs_remote,
            mojo::PendingReceiver<mojom::SmbFsDelegate> delegate_receiver);
  ~SmbFsHost();

  // Returns the path where SmbFS is mounted.
  const base::FilePath& mount_path() const {
    return mount_point_->mount_path();
  }

 private:
  // Mojo disconnection handler.
  void OnDisconnect();

  const std::unique_ptr<chromeos::disks::MountPoint> mount_point_;
  Delegate* const delegate_;

  mojo::Remote<mojom::SmbFs> smbfs_;
  std::unique_ptr<mojom::SmbFsDelegate> delegate_impl_;

  DISALLOW_COPY_AND_ASSIGN(SmbFsHost);
};

}  // namespace smbfs

#endif  // CHROMEOS_COMPONENTS_SMBFS_SMBFS_HOST_H_
