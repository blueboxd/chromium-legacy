// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_SHIM_APP_SHIM_CONTROLLER_H_
#define CHROME_APP_SHIM_APP_SHIM_CONTROLLER_H_

#import <AppKit/AppKit.h>

#include "base/files/file_path.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/common/mac/app_shim.mojom.h"
#include "chrome/common/mac/app_shim_param_traits.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/system/isolated_connection.h"

@class AppShimDelegate;

// The AppShimController is responsible for launching and maintaining the
// connection with the main Chrome process, and generally controls the lifetime
// of the app shim process.
class AppShimController : public chrome::mojom::AppShim {
 public:
  struct Params {
    Params();
    Params(const Params& other);
    ~Params();
    // The full path of the user data dir.
    base::FilePath user_data_dir;
    // The relative path of the profile.
    base::FilePath profile_dir;
    std::string app_mode_id;
    base::string16 app_mode_name;
  };

  explicit AppShimController(const Params& params);
  ~AppShimController() override;

  chrome::mojom::AppShimHost* host() const { return host_.get(); }

  // Called when the app is activated, e.g. by clicking on it in the dock, by
  // dropping a file on the dock icon, or by Cmd+Tabbing to it.
  // Returns whether the message was sent.
  bool SendFocusApp(apps::AppShimFocusType focus_type,
                    const std::vector<base::FilePath>& files);

 private:
  friend class TestShimClient;

  // Create a channel from the Mojo |endpoint| and send a LaunchApp message.
  void CreateChannelAndSendLaunchApp(mojo::PlatformChannelEndpoint endpoint);

  // Builds main menu bar items.
  void SetUpMenu();
  void ChannelError(uint32_t custom_reason, const std::string& description);
  void BootstrapChannelError(uint32_t custom_reason,
                             const std::string& description);
  void LaunchAppDone(apps::AppShimLaunchResult result,
                     chrome::mojom::AppShimRequest app_shim_request);

  // chrome::mojom::AppShim implementation.
  void CreateRemoteCocoaApplication(
      remote_cocoa::mojom::ApplicationAssociatedRequest request) override;
  void CreateCommandDispatcherForWidget(uint64_t widget_id) override;
  void Hide() override;
  void SetBadgeLabel(const std::string& badge_label) override;
  void UnhideWithoutActivation() override;
  void SetUserAttention(apps::AppShimAttentionType attention_type) override;

  // Terminates the app shim process.
  void Close();

  // Returns the connection to the AppShimHostManager in the browser. Returns
  // an invalid endpoint if it is not available yet.
  mojo::PlatformChannelEndpoint GetBrowserEndpoint();

  // Sets up a connection to the AppShimHostManager at the given Mach
  // endpoint name.
  static mojo::PlatformChannelEndpoint ConnectToBrowser(
      const mojo::NamedPlatformChannel::ServerName& server_name);

  // Connects to Chrome and sends a LaunchApp message.
  void InitBootstrapPipe(mojo::PlatformChannelEndpoint endpoint);

  // Find a running instance of Chrome and set |chrome_running_app_| to it. If
  // none exists, launch Chrome, and set |chrome_running_app_|.
  void FindOrLaunchChrome();

  // Check to see if Chrome's AppShimHostManager has been initialized. If it
  // has, then connect.
  void PollForChromeReady(const base::TimeDelta& time_until_timeout);

  const Params params_;
  base::scoped_nsobject<NSRunningApplication> chrome_running_app_;

  mojo::IsolatedConnection bootstrap_mojo_connection_;
  chrome::mojom::AppShimHostBootstrapPtr host_bootstrap_;

  mojo::Binding<chrome::mojom::AppShim> shim_binding_;
  chrome::mojom::AppShimHostPtr host_;
  chrome::mojom::AppShimHostRequest host_request_;

  base::scoped_nsobject<AppShimDelegate> delegate_;
  bool launch_app_done_;
  NSInteger attention_request_id_;

  DISALLOW_COPY_AND_ASSIGN(AppShimController);
};

#endif  // CHROME_APP_SHIM_APP_SHIM_CONTROLLER_H_
