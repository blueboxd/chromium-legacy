// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/web_engine_devtools_controller.h"

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/interface_ptr_set.h>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/common/content_switches.h"
#include "fuchsia/engine/switches.h"
#include "net/base/net_errors.h"
#include "net/base/port_util.h"
#include "net/socket/tcp_server_socket.h"

namespace {

using OnDevToolsPortChanged = base::OnceCallback<void(uint16_t)>;

class DevToolsSocketFactory : public content::DevToolsSocketFactory {
 public:
  DevToolsSocketFactory(OnDevToolsPortChanged on_devtools_port,
                        net::IPEndPoint ip_end_point)
      : on_devtools_port_(std::move(on_devtools_port)),
        ip_end_point_(std::move(ip_end_point)) {}
  ~DevToolsSocketFactory() override = default;

  // content::DevToolsSocketFactory implementation.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    const int kTcpListenBackLog = 5;
    auto socket =
        std::make_unique<net::TCPServerSocket>(nullptr, net::NetLogSource());
    int error = socket->Listen(ip_end_point_, kTcpListenBackLog);
    if (error != net::OK) {
      LOG(WARNING) << "Failed to start the HTTP debugger service. "
                   << net::ErrorToString(error);
      std::move(on_devtools_port_).Run(0);
      return nullptr;
    }

    net::IPEndPoint end_point;
    socket->GetLocalAddress(&end_point);
    std::move(on_devtools_port_).Run(end_point.port());
    return socket;
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* out_name) override {
    return nullptr;
  }

 private:
  OnDevToolsPortChanged on_devtools_port_;
  net::IPEndPoint ip_end_point_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsSocketFactory);
};

void StartRemoteDebuggingServer(OnDevToolsPortChanged on_devtools_port,
                                net::IPEndPoint ip_end_point) {
  const base::FilePath kDisableActivePortOutputDirectory{};
  const base::FilePath kDisableDebugOutput{};

  content::DevToolsAgentHost::StartRemoteDebuggingServer(
      std::make_unique<DevToolsSocketFactory>(std::move(on_devtools_port),
                                              ip_end_point),
      kDisableActivePortOutputDirectory, kDisableDebugOutput);
}

class NoopController : public WebEngineDevToolsController {
 public:
  NoopController() = default;
  ~NoopController() override = default;

  // WebEngineDevToolsController implementation:
  void OnContextCreated() override {}
  void OnContextDestroyed() override {}
  bool OnFrameCreated(content::WebContents* contents,
                      bool user_debugging) override {
    return !user_debugging;
  }
  void OnFrameLoaded(content::WebContents* contents) override {}
  void OnFrameDestroyed(content::WebContents* contents) override {}
  content::DevToolsAgentHost::List RemoteDebuggingTargets() override {
    return {};
  }
  void GetDevToolsPort(base::OnceCallback<void(uint16_t)> callback) override {
    std::move(callback).Run(0);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NoopController);
};

// "User-mode" makes DevTools accessible to remote devices for Frames specified
// by the Context owner. The controller, which starts DevTools when the first
// Frame is created, and shuts it down when no debuggable Frames remain.
class UserModeController : public WebEngineDevToolsController {
 public:
  explicit UserModeController(uint16_t server_port)
      : ip_endpoint_(net::IPAddress::IPv6AllZeros(), server_port) {}

  ~UserModeController() override { DCHECK(!is_remote_debugging_started_); }

  // WebEngineDevToolsController implementation:
  void OnContextCreated() override {}
  void OnContextDestroyed() override {
    if (is_remote_debugging_started_)
      content::DevToolsAgentHost::StopRemoteDebuggingServer();
  }
  bool OnFrameCreated(content::WebContents* contents,
                      bool user_debugging) override {
    if (user_debugging) {
      debuggable_contents_.insert(contents);
      UpdateRemoteDebuggingServer();
    }
    return true;
  }
  void OnFrameLoaded(content::WebContents* contents) override {}
  void OnFrameDestroyed(content::WebContents* contents) override {
    debuggable_contents_.erase(contents);
    UpdateRemoteDebuggingServer();
  }
  content::DevToolsAgentHost::List RemoteDebuggingTargets() override {
    DCHECK(is_remote_debugging_started_);

    content::DevToolsAgentHost::List enabled_hosts;
    for (content::WebContents* contents : debuggable_contents_) {
      enabled_hosts.push_back(
          content::DevToolsAgentHost::GetOrCreateFor(contents));
    }
    return enabled_hosts;
  }
  void GetDevToolsPort(base::OnceCallback<void(uint16_t)> callback) override {
    get_port_callbacks_.emplace_back(std::move(callback));
    MaybeNotifyGetPortCallbacks();
  }

 private:
  // Starts or stops the remote debugging server, if necessary
  void UpdateRemoteDebuggingServer() {
    bool need_remote_debugging = !debuggable_contents_.empty();
    if (need_remote_debugging == is_remote_debugging_started_)
      return;
    is_remote_debugging_started_ = need_remote_debugging;

    if (need_remote_debugging) {
      StartRemoteDebuggingServer(
          base::BindOnce(&UserModeController::OnDevToolsPortChanged,
                         base::Unretained(this)),
          ip_endpoint_);
    } else {
      content::DevToolsAgentHost::StopRemoteDebuggingServer();
      devtools_port_.reset();
    }
  }

  void OnDevToolsPortChanged(uint16_t port) {
    devtools_port_ = port;
    MaybeNotifyGetPortCallbacks();
  }

  void MaybeNotifyGetPortCallbacks() {
    if (!devtools_port_.has_value())
      return;
    for (auto& callback : get_port_callbacks_)
      std::move(callback).Run(devtools_port_.value());
    get_port_callbacks_.clear();
  }

  const net::IPEndPoint ip_endpoint_;

  // True if the remote debugging server is started.
  bool is_remote_debugging_started_ = false;

  // Currently active DevTools port. Set to 0 on service startup error.
  base::Optional<uint16_t> devtools_port_;

  // Set of Frames' content::WebContents which are remotely debuggable.
  base::flat_set<content::WebContents*> debuggable_contents_;

  std::vector<base::OnceCallback<void(uint16_t)>> get_port_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(UserModeController);
};

// "Debug-mode" is used for on-device testing, and makes all Frames available
// for debugging by clients on the same device. DevTools is only started when
// the first Frame finishes loading its main document, so that the
// DevToolsPerContextListeners can start interacting with it immediately.
// Derives from NoopController to inherit its handling of requests for
// user-mode debugging, and for the DevTools port, by clients.
class DebugModeController : public WebEngineDevToolsController {
 public:
  explicit DebugModeController(
      std::vector<fuchsia::web::DevToolsPerContextListenerPtr> listeners) {
    for (auto& listener : listeners) {
      devtools_listeners_.AddInterfacePtr(std::move(listener));
    }
  }

  ~DebugModeController() override = default;

  // DevToolsController implementation:
  void OnContextCreated() override {
    StartRemoteDebuggingServer(
        base::BindOnce(&DebugModeController::OnDevToolsPortChanged,
                       base::Unretained(this)),
        net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0));
  }
  void OnContextDestroyed() override {
    content::DevToolsAgentHost::StopRemoteDebuggingServer();
  }
  bool OnFrameCreated(content::WebContents* contents,
                      bool user_debugging) override {
    return !user_debugging;
  }
  void OnFrameLoaded(content::WebContents* contents) override {
    frame_loaded_ = true;
    MaybeSendRemoteDebuggingCallbacks();
  }
  void OnFrameDestroyed(content::WebContents* contents) override {}
  content::DevToolsAgentHost::List RemoteDebuggingTargets() override {
    return content::DevToolsAgentHost::GetOrCreateAll();
  }
  void GetDevToolsPort(base::OnceCallback<void(uint16_t)> callback) override {
    std::move(callback).Run(0);
  }

 private:
  void OnDevToolsPortChanged(uint16_t port) {
    devtools_port_ = port;
    MaybeSendRemoteDebuggingCallbacks();
  }

  void MaybeSendRemoteDebuggingCallbacks() {
    if (!frame_loaded_ || !devtools_port_)
      return;

    // If |devtools_port_| is valid then notify all listeners, otherwise
    // disconnect them.
    if (devtools_port_.value() == 0) {
      devtools_listeners_.CloseAll();
    } else {
      for (const auto& listener : devtools_listeners_.ptrs()) {
        listener->get()->OnHttpPortOpen(devtools_port_.value());
      }
    }
  }

  // Currently active DevTools port. Set to 0 on service startup error.
  base::Optional<uint16_t> devtools_port_;

  bool frame_loaded_ = false;

  fidl::InterfacePtrSet<fuchsia::web::DevToolsPerContextListener>
      devtools_listeners_;

  DISALLOW_COPY_AND_ASSIGN(DebugModeController);
};

}  //  namespace

// static
std::unique_ptr<WebEngineDevToolsController>
WebEngineDevToolsController::CreateFromCommandLine(
    const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kRemoteDebuggingPort)) {
    // Set up DevTools to listen on all network routes on the command-line
    // provided port.
    base::StringPiece command_line_port_value =
        command_line.GetSwitchValueASCII(switches::kRemoteDebuggingPort);
    int parsed_port = 0;

    // The command-line option can only be provided by the ContextProvider
    // process, it should not fail to parse to an int.
    DCHECK(base::StringToInt(command_line_port_value, &parsed_port));

    if (parsed_port != 0 &&
        (!net::IsPortValid(parsed_port) || net::IsWellKnownPort(parsed_port))) {
      LOG(WARNING) << "Invalid HTTP debugger service port number "
                   << command_line_port_value;
      return nullptr;
    }

    return std::make_unique<UserModeController>(parsed_port);
  }

  if (command_line.HasSwitch(switches::kRemoteDebuggerHandles)) {
    // Initialize the Debug devtools listeners.
    std::string handle_ids_str =
        command_line.GetSwitchValueASCII(switches::kRemoteDebuggerHandles);

    // Extract individual handle IDs from the comma-separated list.
    base::StringTokenizer tokenizer(handle_ids_str, ",");
    std::vector<fuchsia::web::DevToolsPerContextListenerPtr> listeners;
    while (tokenizer.GetNext()) {
      uint32_t handle_id = 0;
      if (!base::StringToUint(tokenizer.token(), &handle_id))
        continue;
      fuchsia::web::DevToolsPerContextListenerPtr listener;
      listener.Bind(zx::channel(zx_take_startup_handle(handle_id)));
      listeners.emplace_back(std::move(listener));
    }

    return std::make_unique<DebugModeController>(std::move(listeners));
  }

  return std::make_unique<NoopController>();
}
