// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DLCSERVICE_DLCSERVICE_CLIENT_H_
#define CHROMEOS_DBUS_DLCSERVICE_DLCSERVICE_CLIENT_H_

#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "chromeos/dbus/dlcservice/dlcservice.pb.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/dlcservice/dbus-constants.h"

namespace chromeos {

// This class is a singleton and should be accessed using |Get()|.
// DlcserviceClient is used to communicate with the dlcservice daemon which
// manages DLC (Downloadable Content) modules. DlcserviceClient will allow for
// CrOS features to be installed and uninstalled at runtime of the system. If
// more details about dlcservice are required, please consult
// https://chromium.git.corp.google.com/chromiumos/platform2/+/HEAD/dlcservice
class COMPONENT_EXPORT(DLCSERVICE_CLIENT) DlcserviceClient {
 public:
  // The callback used for |Install()|, if the error is something other than
  // |dlcservice::kErrorNone| the call has failed. For large DLC(s) to install,
  // there may be delay between the time of this call and the callback being
  // invoked.
  using InstallCallback = base::OnceCallback<void(
      const std::string& err,
      const dlcservice::DlcModuleList& dlc_module_list)>;

  // The callback used for |Install()|, if the caller wants to listen in on the
  // progress of their download/install. If the caller only cares for whether
  // the install is complete or not, the caller can pass in |RepeatingCallback|
  // that is a no-op.
  using ProgressCallback = base::RepeatingCallback<void(double progress)>;

  // The callback used for |Uninstall()|, if the error is something other than
  // |dlcservice::kErrorNone| the call has failed.
  using UninstallCallback = base::OnceCallback<void(const std::string& err)>;

  // The callback used for |Purge()|, if the error is something other than
  // |dlcservice::kErrorNone| the call has failed.
  using PurgeCallback = base::OnceCallback<void(const std::string& err)>;

  // The callback used for |GetInstalled()|, if the error is something other
  // than |dlcservice::kErrorNone| the call has failed. It is a very rare case
  // for |GetInstalled()| call to fail.
  using GetInstalledCallback = base::OnceCallback<void(
      const std::string& err,
      const dlcservice::DlcModuleList& dlc_module_list)>;

  // The callback used for |GetExistingDlcs()|, if the error is something other
  // than |dlcservice::kErrorNone| the call has failed. It is a very rare case
  // for |GetExistingDlcs()| call to fail.
  using GetExistingDlcsCallback = base::OnceCallback<void(
      const std::string& err,
      const dlcservice::DlcsWithContent& dlcs_with_content)>;

  // The callback to use for |Install()|, if the caller wants to ignore the
  // progress updates.
  static const ProgressCallback IgnoreProgress;

  // Installs all DLC(s) passed in while reporting progress through the progress
  // callback and only calls install callback on install success/failure.
  virtual void Install(const dlcservice::DlcModuleList& dlc_module_list,
                       InstallCallback callback,
                       ProgressCallback progress_callback) = 0;

  // Uninstalls a single DLC and calls the callback with indication of
  // success/failure. Uninstalling disables the DLC but does not remove the DLC
  // from disk. After each uninstallation, a refcount to the DLC is decremented.
  // Once the refcount reaches 0, the DLC will remain in cache. However, if
  // the DLC is not installed within a window of time after reaching a
  // refcount of 0, the DLC will be purged automatically.
  virtual void Uninstall(const std::string& dlc_id,
                         UninstallCallback callback) = 0;

  // Purges a single DLC and calls the callback with indication of
  // success/failure. Purging removes the DLC entirely from disk, regardless if
  // the DLC has been uninstalled or if there is a nonzero installed refcount.
  virtual void Purge(const std::string& dlc_id,
                     PurgeCallback purge_callback) = 0;

  // DEPRECATING: Please do not use this method call.
  // Provides the DLC(s) information such as id and root mount point.
  virtual void GetInstalled(GetInstalledCallback callback) = 0;

  // Provides the DLC(s) information such as:
  // id, name, description, used_bytes_on_disk. (reference
  // |dlcservice::DlcsWithContent| proto for complete details)
  virtual void GetExistingDlcs(GetExistingDlcsCallback callback) = 0;

  // During testing, can be used to mimic signals received back from dlcservice.
  virtual void OnInstallStatusForTest(dbus::Signal* signal) = 0;

  // Creates and initializes the global instance. |bus| must not be nullptr.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be nullptr if not initialized.
  static DlcserviceClient* Get();

 protected:
  friend class DlcserviceClientTest;

  // Initialize/Shutdown should be used instead.
  DlcserviceClient();
  virtual ~DlcserviceClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(DlcserviceClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DLCSERVICE_DLCSERVICE_CLIENT_H_
