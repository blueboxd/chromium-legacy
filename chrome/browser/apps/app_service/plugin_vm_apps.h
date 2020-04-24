// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PLUGIN_VM_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PLUGIN_VM_APPS_H_

#include <memory>
#include <string>

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/services/app_service/public/cpp/publisher_base.h"
#include "chrome/services/app_service/public/mojom/app_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class Profile;

namespace apps {

// An app publisher (in the App Service sense) of Plugin VM apps.
//
// See chrome/services/app_service/README.md.
class PluginVmApps : public apps::PublisherBase {
 public:
  PluginVmApps(const mojo::Remote<apps::mojom::AppService>& app_service,
               Profile* profile);
  ~PluginVmApps() override;

  PluginVmApps(const PluginVmApps&) = delete;
  PluginVmApps& operator=(const PluginVmApps&) = delete;

 private:
  // apps::PublisherBase overrides.
  void Connect(mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
               apps::mojom::ConnectOptionsPtr opts) override;
  void LoadIcon(const std::string& app_id,
                apps::mojom::IconKeyPtr icon_key,
                apps::mojom::IconCompression icon_compression,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                LoadIconCallback callback) override;
  void Launch(const std::string& app_id,
              int32_t event_flags,
              apps::mojom::LaunchSource launch_source,
              int64_t display_id) override;
  void GetMenuModel(const std::string& app_id,
                    apps::mojom::MenuType menu_type,
                    int64_t display_id,
                    GetMenuModelCallback callback) override;

  void OnPluginVmAllowedChanged(bool is_allowed);

  mojo::RemoteSet<apps::mojom::Subscriber> subscribers_;

  Profile* const profile_;

  // Whether the Plugin VM app is allowed by policy.
  bool is_allowed_;

  std::unique_ptr<plugin_vm::PluginVmPolicySubscription> policy_subscription_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PLUGIN_VM_APPS_H_
