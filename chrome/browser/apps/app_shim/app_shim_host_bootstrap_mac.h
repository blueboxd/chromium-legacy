// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HOST_BOOTSTRAP_MAC_H_
#define CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HOST_BOOTSTRAP_MAC_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"
#include "chrome/common/mac/app_shim.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/isolated_connection.h"

class AppShimHostBootstrap : public chrome::mojom::AppShimHostBootstrap {
 public:
  // The interface through which the AppShimHostBootstrap registers itself
  // with the ExtensionAppShimHandler.
  class Client {
   public:
    // Invoked by the AppShimHostBootstrap when a shim process has connected to
    // the browser process. This will connect to (creating, if needed) an
    // AppShimHost. |bootstrap| must have OnConnectedToHost or
    // OnFailedToConnectToHost called on it to inform the shim of the result.
    virtual void OnShimProcessConnected(
        std::unique_ptr<AppShimHostBootstrap> bootstrap) = 0;
  };

  // Set the client interface that all objects of this class will use.
  static void SetClient(Client* client);

  // Creates a new server-side mojo channel at |endpoint|, which contains a
  // a Mach port for a channel created by an MachBootstrapAcceptor, and
  // begins listening for messages on it. The PID of the sender of |endpoint|
  // is stored in |peer_pid|.
  static void CreateForChannelAndPeerID(mojo::PlatformChannelEndpoint endpoint,
                                        base::ProcessId peer_pid);

  ~AppShimHostBootstrap() override;

  // Called in response to connecting (or failing to connect to) an
  // AppShimHost.
  void OnConnectedToHost(chrome::mojom::AppShimRequest app_shim_request);
  void OnFailedToConnectToHost(apps::AppShimLaunchResult result);

  chrome::mojom::AppShimHostRequest GetLaunchAppShimHostRequest();
  base::ProcessId GetAppShimPid() const { return pid_; }
  const std::string& GetAppId() const { return app_id_; }
  const base::FilePath& GetProfilePath() const { return profile_path_; }

  // Indicates the type of launch (by Chrome or from the app).
  apps::AppShimLaunchType GetLaunchType() const { return launch_type_; }

  // If non-empty, holds an array of file paths given as arguments, or dragged
  // onto the app bundle or dock icon.
  const std::vector<base::FilePath>& GetLaunchFiles() const { return files_; }

 protected:
  explicit AppShimHostBootstrap(base::ProcessId peer_pid);
  void ServeChannel(mojo::PlatformChannelEndpoint endpoint);
  void ChannelError(uint32_t custom_reason, const std::string& description);

  // chrome::mojom::AppShimHostBootstrap.
  void LaunchApp(chrome::mojom::AppShimHostRequest app_shim_host_request,
                 const base::FilePath& profile_dir,
                 const std::string& app_id,
                 apps::AppShimLaunchType launch_type,
                 const std::vector<base::FilePath>& files,
                 LaunchAppCallback callback) override;

  mojo::IsolatedConnection bootstrap_mojo_connection_;
  mojo::Binding<chrome::mojom::AppShimHostBootstrap> host_bootstrap_binding_;

  // The arguments from the LaunchApp call, and whether or not it has happened
  // yet.
  bool has_received_launch_app_ = false;
  base::ProcessId pid_ = 0;
  chrome::mojom::AppShimHostRequest app_shim_host_request_;
  base::FilePath profile_path_;
  std::string app_id_;
  apps::AppShimLaunchType launch_type_;
  std::vector<base::FilePath> files_;
  LaunchAppCallback launch_app_callback_;

  THREAD_CHECKER(thread_checker_);
  DISALLOW_COPY_AND_ASSIGN(AppShimHostBootstrap);
};

#endif  // CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HOST_BOOTSTRAP_MAC_H_
