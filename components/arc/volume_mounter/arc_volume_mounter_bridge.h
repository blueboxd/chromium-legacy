// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_VOLUME_MOUNTER_ARC_VOLUME_MOUNTER_BRIDGE_H_
#define COMPONENTS_ARC_VOLUME_MOUNTER_ARC_VOLUME_MOUNTER_BRIDGE_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "components/arc/mojom/volume_mounter.mojom.h"
#include "components/arc/session/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class handles Volume mount/unmount requests from cros-disks and
// send them to Android.
class ArcVolumeMounterBridge
    : public KeyedService,
      public chromeos::disks::DiskMountManager::Observer,
      public ConnectionObserver<mojom::VolumeMounterInstance>,
      public mojom::VolumeMounterHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcVolumeMounterBridge* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcVolumeMounterBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcVolumeMounterBridge(content::BrowserContext* context,
                         ArcBridgeService* bridge_service);
  ~ArcVolumeMounterBridge() override;

  // ConnectionObserver<mojom::VolumeMounterInstance> overrides:
  void OnConnectionReady() override;

  // chromeos::disks::DiskMountManager::Observer overrides:
  void OnMountEvent(chromeos::disks::DiskMountManager::MountEvent event,
                    chromeos::MountError error_code,
                    const chromeos::disks::DiskMountManager::MountPointInfo&
                        mount_info) override;

  // mojom::VolumeMounterHost overrides:
  void RequestAllMountPoints() override;

  bool files_app_toast_shown() const { return files_app_toast_shown_; }
  void set_files_app_toast_shown(bool files_app_toast_shown) {
    files_app_toast_shown_ = files_app_toast_shown;
  }

 private:
  void SendAllMountEvents();

  void SendMountEventForMyFiles();

  bool HasAccessToRemovableMedia() const;
  void OnPrefChanged();

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  PrefService* const pref_service_;
  PrefChangeRegistrar change_registerar_;

  // A flag to remember if the ARC toast UI in Files.app has been shown in the
  // current user session.
  bool files_app_toast_shown_ = false;

  base::WeakPtrFactory<ArcVolumeMounterBridge> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcVolumeMounterBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_VOLUME_MOUNTER_ARC_VOLUME_MOUNTER_BRIDGE_H_
