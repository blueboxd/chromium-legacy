// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app_shim/app_shim_controller.h"

#import <Cocoa/Cocoa.h>
#include <mach/message.h>

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/hash/md5.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/launch_services_util.h"
#include "base/mac/mach_logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app_shim/app_shim_delegate.h"
#include "chrome/browser/ui/cocoa/browser_window_command_handler.h"
#include "chrome/browser/ui/cocoa/chrome_command_dispatcher_delegate.h"
#include "chrome/browser/ui/cocoa/main_menu_builder.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/mac/app_mode_common.h"
#include "components/remote_cocoa/app_shim/application_bridge.h"
#include "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#include "components/remote_cocoa/common/application.mojom.h"
#include "content/public/browser/remote_cocoa.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
// The maximum amount of time to wait for Chrome's AppShimHostManager to be
// ready.
constexpr base::TimeDelta kPollTimeoutSeconds =
    base::TimeDelta::FromSeconds(60);

// The period in between attempts to check of Chrome's AppShimHostManager is
// ready.
constexpr base::TimeDelta kPollPeriodMsec =
    base::TimeDelta::FromMilliseconds(100);
}  // namespace

AppShimController::Params::Params() = default;
AppShimController::Params::Params(const Params& other) = default;
AppShimController::Params::~Params() = default;

AppShimController::AppShimController(const Params& params)
    : params_(params),
      shim_binding_(this),
      host_request_(mojo::MakeRequest(&host_)),
      delegate_([[AppShimDelegate alloc] init]),
      launch_app_done_(false),
      attention_request_id_(0) {
  // Since AppShimController is created before the main message loop starts,
  // NSApp will not be set, so use sharedApplication.
  NSApplication* sharedApplication = [NSApplication sharedApplication];
  [sharedApplication setDelegate:delegate_];

  // Ensure Chrome is launched.
  FindOrLaunchChrome();

  // Start polling to see if Chrome is ready to connect.
  PollForChromeReady(kPollTimeoutSeconds);
}

AppShimController::~AppShimController() {
  // Un-set the delegate since NSApplication does not retain it.
  NSApplication* sharedApplication = [NSApplication sharedApplication];
  [sharedApplication setDelegate:nil];
}

void AppShimController::FindOrLaunchChrome() {
  DCHECK(!chrome_running_app_);

  // If this shim was launched by Chrome, open that specified process.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          app_mode::kLaunchedByChromeProcessId)) {
    std::string chrome_pid_string =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            app_mode::kLaunchedByChromeProcessId);
    int chrome_pid;
    if (!base::StringToInt(chrome_pid_string, &chrome_pid))
      LOG(FATAL) << "Invalid PID: " << chrome_pid_string;

    chrome_running_app_.reset([NSRunningApplication
        runningApplicationWithProcessIdentifier:chrome_pid]);
    if (!chrome_running_app_)
      LOG(FATAL) << "Failed open process with PID: " << chrome_pid;
  }

  // Find an already running instance of Chrome to open, if one exists.
  NSArray* running_chrome_instances = [NSRunningApplication
      runningApplicationsWithBundleIdentifier:[base::mac::OuterBundle()
                                                  bundleIdentifier]];
  if ([running_chrome_instances count] > 0) {
    chrome_running_app_.reset([running_chrome_instances objectAtIndex:0],
                              base::scoped_policy::RETAIN);
    return;
  }

  // In tests, launching Chrome does nothing.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          app_mode::kLaunchedForTest)) {
    return;
  }

  // Launch Chrome.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kSilentLaunch);
  command_line.AppendSwitchPath(switches::kProfileDirectory,
                                params_.profile_dir);
  chrome_running_app_.reset(base::mac::OpenApplicationWithPath(
      base::mac::OuterBundlePath(), command_line, NSWorkspaceLaunchDefault));
  if (!chrome_running_app_)
    LOG(FATAL) << "Failed launch Chrome.";
}

void AppShimController::PollForChromeReady(
    const base::TimeDelta& time_until_timeout) {
  // If the Chrome application we planned to connect to is not running, quit.
  if ([chrome_running_app_ isTerminated])
    LOG(FATAL) << "Running chrome instance terminated before connecting.";

  mojo::PlatformChannelEndpoint endpoint = GetBrowserEndpoint();
  if (endpoint.is_valid()) {
    InitBootstrapPipe(std::move(endpoint));
    return;
  }

  // Otherwise, try again after a brief delay.
  if (time_until_timeout < kPollPeriodMsec)
    LOG(FATAL) << "Timed out waiting for running chrome instance to be ready.";
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AppShimController::PollForChromeReady,
                     base::Unretained(this),
                     time_until_timeout - kPollPeriodMsec),
      kPollPeriodMsec);
}

mojo::PlatformChannelEndpoint AppShimController::GetBrowserEndpoint() {
  NSString* browser_bundle_id =
      base::mac::ObjCCast<NSString>([[NSBundle mainBundle]
          objectForInfoDictionaryKey:app_mode::kBrowserBundleIDKey]);
  CHECK(browser_bundle_id);

  std::string name_fragment = base::StringPrintf(
      "%s.%s.%s", base::SysNSStringToUTF8(browser_bundle_id).c_str(),
      app_mode::kAppShimBootstrapNameFragment,
      base::MD5String(params_.user_data_dir.value()).c_str());
  return ConnectToBrowser(name_fragment);
}

// static
mojo::PlatformChannelEndpoint AppShimController::ConnectToBrowser(
    const mojo::NamedPlatformChannel::ServerName& server_name) {
  // Normally NamedPlatformChannel is used for point-to-point peer
  // communication. For apps shims, the same server is used to establish
  // connections between multiple shim clients and the server. To do this,
  // the shim creates a local PlatformChannel and sends the local (send)
  // endpoint to the server in a raw Mach message. The server uses that to
  // establish an IsolatedConnection, which the client does as well with the
  // remote (receive) end.
  mojo::PlatformChannelEndpoint server_endpoint =
      mojo::NamedPlatformChannel::ConnectToServer(server_name);
  // The browser may still be in the process of launching, so the endpoint
  // may not yet be available.
  if (!server_endpoint.is_valid())
    return mojo::PlatformChannelEndpoint();

  mojo::PlatformChannel channel;
  mach_msg_base_t message{};
  message.header.msgh_id = app_mode::kBootstrapMsgId;
  message.header.msgh_bits =
      MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND, MACH_MSG_TYPE_MOVE_SEND);
  message.header.msgh_size = sizeof(message);
  message.header.msgh_local_port =
      channel.TakeLocalEndpoint().TakePlatformHandle().ReleaseMachSendRight();
  message.header.msgh_remote_port =
      server_endpoint.TakePlatformHandle().ReleaseMachSendRight();
  kern_return_t kr = mach_msg_send(&message.header);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "mach_msg_send";
    return mojo::PlatformChannelEndpoint();
  }
  return channel.TakeRemoteEndpoint();
}

void AppShimController::InitBootstrapPipe(
    mojo::PlatformChannelEndpoint endpoint) {
  SetUpMenu();

  // Chrome will relaunch shims when relaunching apps.
  [NSApp disableRelaunchOnLogin];
  CHECK(!params_.user_data_dir.empty());

  CreateChannelAndSendLaunchApp(std::move(endpoint));
}

void AppShimController::CreateChannelAndSendLaunchApp(
    mojo::PlatformChannelEndpoint endpoint) {
  mojo::ScopedMessagePipeHandle message_pipe =
      bootstrap_mojo_connection_.Connect(std::move(endpoint));
  CHECK(message_pipe.is_valid());
  host_bootstrap_ = chrome::mojom::AppShimHostBootstrapPtr(
      chrome::mojom::AppShimHostBootstrapPtrInfo(std::move(message_pipe), 0));
  host_bootstrap_.set_connection_error_with_reason_handler(base::BindOnce(
      &AppShimController::BootstrapChannelError, base::Unretained(this)));

  bool launched_by_chrome = base::CommandLine::ForCurrentProcess()->HasSwitch(
      app_mode::kLaunchedByChromeProcessId);
  apps::AppShimLaunchType launch_type =
      launched_by_chrome ? apps::APP_SHIM_LAUNCH_REGISTER_ONLY
                         : apps::APP_SHIM_LAUNCH_NORMAL;

  [delegate_ setController:this];

  std::vector<base::FilePath> files;
  [delegate_ getFilesToOpenAtStartup:&files];

  host_bootstrap_->LaunchApp(std::move(host_request_), params_.profile_dir,
                             params_.app_mode_id, launch_type, files,
                             base::BindOnce(&AppShimController::LaunchAppDone,
                                            base::Unretained(this)));
}

void AppShimController::SetUpMenu() {
  chrome::BuildMainMenu(NSApp, delegate_, params_.app_mode_name, true);
}

void AppShimController::BootstrapChannelError(uint32_t custom_reason,
                                              const std::string& description) {
  // The bootstrap channel is expected to close after sending LaunchAppDone.
  if (launch_app_done_)
    return;
  LOG(ERROR) << "Channel error custom_reason:" << custom_reason
             << " description: " << description;
  Close();
}

void AppShimController::ChannelError(uint32_t custom_reason,
                                     const std::string& description) {
  LOG(ERROR) << "Channel error custom_reason:" << custom_reason
             << " description: " << description;
  Close();
}

void AppShimController::LaunchAppDone(
    apps::AppShimLaunchResult result,
    chrome::mojom::AppShimRequest app_shim_request) {
  if (result != apps::APP_SHIM_LAUNCH_SUCCESS) {
    Close();
    return;
  }
  shim_binding_.Bind(std::move(app_shim_request),
                     ui::WindowResizeHelperMac::Get()->task_runner());
  shim_binding_.set_connection_error_with_reason_handler(
      base::BindOnce(&AppShimController::ChannelError, base::Unretained(this)));

  std::vector<base::FilePath> files;
  if ([delegate_ getFilesToOpenAtStartup:&files])
    SendFocusApp(apps::APP_SHIM_FOCUS_OPEN_FILES, files);

  launch_app_done_ = true;
  host_bootstrap_.reset();
}

void AppShimController::CreateRemoteCocoaApplication(
    remote_cocoa::mojom::ApplicationAssociatedRequest request) {
  remote_cocoa::ApplicationBridge::Get()->BindRequest(std::move(request));
  remote_cocoa::ApplicationBridge::Get()->SetContentNSViewCreateCallbacks(
      base::BindRepeating(remote_cocoa::CreateRenderWidgetHostNSView),
      base::BindRepeating(remote_cocoa::CreateWebContentsNSView));
}

void AppShimController::CreateCommandDispatcherForWidget(uint64_t widget_id) {
  if (auto* bridge =
          remote_cocoa::NativeWidgetNSWindowBridge::GetFromId(widget_id)) {
    bridge->SetCommandDispatcher(
        [[[ChromeCommandDispatcherDelegate alloc] init] autorelease],
        [[[BrowserWindowCommandHandler alloc] init] autorelease]);
  } else {
    LOG(ERROR) << "Failed to find host for command dispatcher.";
  }
}

void AppShimController::Hide() {
  [NSApp hide:nil];
}

void AppShimController::SetBadgeLabel(const std::string& badge_label) {
  NSApp.dockTile.badgeLabel = base::SysUTF8ToNSString(badge_label);
}

void AppShimController::UnhideWithoutActivation() {
  [NSApp unhideWithoutActivation];
}

void AppShimController::SetUserAttention(
    apps::AppShimAttentionType attention_type) {
  switch (attention_type) {
    case apps::APP_SHIM_ATTENTION_CANCEL:
      [NSApp cancelUserAttentionRequest:attention_request_id_];
      attention_request_id_ = 0;
      break;
    case apps::APP_SHIM_ATTENTION_CRITICAL:
      attention_request_id_ = [NSApp requestUserAttention:NSCriticalRequest];
      break;
    case apps::APP_SHIM_ATTENTION_INFORMATIONAL:
      attention_request_id_ =
          [NSApp requestUserAttention:NSInformationalRequest];
      break;
    case apps::APP_SHIM_ATTENTION_NUM_TYPES:
      NOTREACHED();
  }
}

void AppShimController::Close() {
  [NSApp terminate:nil];
}

bool AppShimController::SendFocusApp(apps::AppShimFocusType focus_type,
                                     const std::vector<base::FilePath>& files) {
  if (launch_app_done_) {
    host_->FocusApp(focus_type, files);
    return true;
  }

  return false;
}
