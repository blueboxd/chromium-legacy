// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/web_engine_remote_debugging.h"

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/common/content_switches.h"
#include "fuchsia/engine/browser/web_engine_browser_context.h"
#include "fuchsia/engine/common.h"
#include "net/base/net_errors.h"
#include "net/base/port_util.h"
#include "net/socket/tcp_server_socket.h"

namespace {

class WebEngineDevToolsSocketFactory : public content::DevToolsSocketFactory {
 public:
  WebEngineDevToolsSocketFactory(
      WebEngineRemoteDebugging::OnListenCallback on_listen_callback,
      uint16_t port,
      bool for_fidl_debug_api)
      : on_listen_callback_(std::move(on_listen_callback)),
        port_(port),
        for_fidl_debug_api_(for_fidl_debug_api) {}
  ~WebEngineDevToolsSocketFactory() override = default;

  // content::DevToolsSocketFactory implementation.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    const int kTcpListenBackLog = 5;
    std::unique_ptr<net::ServerSocket> socket(
        new net::TCPServerSocket(nullptr, net::NetLogSource()));
    // When set from the fuchsia.web.Debug API, remote debugging only listens on
    // IPv4 localhost, not IPv6.
    net::IPEndPoint ip_end_point(for_fidl_debug_api_
                                     ? net::IPAddress::IPv4Localhost()
                                     : net::IPAddress::IPv6AllZeros(),
                                 port_);
    int error = socket->Listen(ip_end_point, kTcpListenBackLog);
    if (error != net::OK) {
      LOG(WARNING) << "Failed to start the HTTP debugger service. "
                   << net::ErrorToString(error);
      std::move(on_listen_callback_).Run(0);
      return nullptr;
    }

    net::IPEndPoint end_point;
    socket->GetLocalAddress(&end_point);
    std::move(on_listen_callback_).Run(end_point.port());
    return socket;
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* out_name) override {
    return nullptr;
  }

 private:
  WebEngineRemoteDebugging::OnListenCallback on_listen_callback_;
  const uint16_t port_;
  const bool for_fidl_debug_api_;

  DISALLOW_COPY_AND_ASSIGN(WebEngineDevToolsSocketFactory);
};

}  //  namespace

WebEngineRemoteDebugging::WebEngineRemoteDebugging() {
  const base::FilePath kDisableActivePortOutputDirectory;
  const base::FilePath kDisableDebugOutput;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kRemoteDebuggingPort)) {
    // Set up DevTools to listen on all network routes on the command-line
    // provided port.
    base::StringPiece command_line_port_value =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kRemoteDebuggingPort);
    int parsed_port = 0;

    // The command-line option can only be provided by the ContextProvider
    // process, it should not fail to parse to an int.
    DCHECK(base::StringToInt(command_line_port_value, &parsed_port));
    if (parsed_port != 0 &&
        (!net::IsPortValid(parsed_port) || net::IsWellKnownPort(parsed_port))) {
      LOG(WARNING) << "Invalid HTTP debugger service port number "
                   << command_line_port_value;
      OnRemoteDebuggingServiceStatusChanged(0);
      return;
    }

    content::DevToolsAgentHost::StartRemoteDebuggingServer(
        std::make_unique<WebEngineDevToolsSocketFactory>(
            base::BindOnce(&WebEngineRemoteDebugging::
                               OnRemoteDebuggingServiceStatusChanged,
                           base::Unretained(this)),
            parsed_port, false),
        kDisableActivePortOutputDirectory, kDisableDebugOutput);
    remote_debugging_enabled_ = true;
  } else if (base::CommandLine::ForCurrentProcess()->HasSwitch(
                 kRemoteDebuggerHandles)) {
    // Set up DevTools to listen on localhost on an ephemeral port when
    // enabled from the fuchsia.web.Debug API.
    content::DevToolsAgentHost::StartRemoteDebuggingServer(
        std::make_unique<WebEngineDevToolsSocketFactory>(
            base::BindOnce(&WebEngineRemoteDebugging::
                               OnRemoteDebuggingServiceStatusChanged,
                           base::Unretained(this)),
            0, true),
        kDisableActivePortOutputDirectory, kDisableDebugOutput);
    remote_debugging_enabled_ = true;

    // Initialize the Debug devtools listeners.
    debug_devtools_listeners_.emplace();
    std::string handle_ids_str =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            kRemoteDebuggerHandles);

    // Extract individual handle IDs from the comma-separated list.
    base::StringTokenizer tokenizer(handle_ids_str, ",");
    while (tokenizer.GetNext()) {
      uint32_t handle_id = 0;
      if (!base::StringToUint(tokenizer.token(), &handle_id))
        continue;
      fidl::InterfacePtr<fuchsia::web::DevToolsPerContextListener> listener;
      listener.Bind(zx::channel(zx_take_startup_handle(handle_id)));
      debug_devtools_listeners_->AddInterfacePtr(std::move(listener));
    }
  } else {
    OnRemoteDebuggingServiceStatusChanged(0);
  }
}

void WebEngineRemoteDebugging::OnDebugDevToolsPortReady() {
  // The DevTools service should have been opened before a Frame navigated.
  DCHECK(devtools_port_received_);

  if (debug_devtools_listeners_notified_)
    return;
  debug_devtools_listeners_notified_ = true;
  if (!debug_devtools_listeners_)
    return;

  if (devtools_port_ == 0) {
    // The DevTools service failed to open. Close the listeners.
    debug_devtools_listeners_->CloseAll();
  } else {
    for (auto& listener : debug_devtools_listeners_->ptrs()) {
      listener->get()->OnHttpPortOpen(devtools_port_);
    }
  }
}

void WebEngineRemoteDebugging::GetRemoteDebuggingPort(
    GetRemoteDebuggingPortCallback callback) {
  pending_callbacks_.push_back(std::move(callback));
  MaybeSendRemoteDebuggingCallbacks();
}

WebEngineRemoteDebugging::~WebEngineRemoteDebugging() {
  if (remote_debugging_enabled_)
    content::DevToolsAgentHost::StopRemoteDebuggingServer();
}

void WebEngineRemoteDebugging::OnRemoteDebuggingServiceStatusChanged(
    uint16_t port) {
  DCHECK(!devtools_port_received_);
  devtools_port_ = port;
  devtools_port_received_ = true;
  MaybeSendRemoteDebuggingCallbacks();
}

void WebEngineRemoteDebugging::MaybeSendRemoteDebuggingCallbacks() {
  if (!devtools_port_received_)
    return;

  std::vector<GetRemoteDebuggingPortCallback> pending_callbacks =
      std::move(pending_callbacks_);

  // Signal the pending callbacks and clear the vector.
  std::for_each(
      pending_callbacks.begin(), pending_callbacks.end(),
      [this](GetRemoteDebuggingPortCallback& callback) {
        fuchsia::web::Context_GetRemoteDebuggingPort_Result result;
        // Do not send the port in case the service was opened for debugging
        // and not via CreateContextParams argument.
        if (debug_devtools_listeners_ || devtools_port_ == 0) {
          result.set_err(
              fuchsia::web::ContextError::REMOTE_DEBUGGING_PORT_NOT_OPENED);
        } else {
          fuchsia::web::Context_GetRemoteDebuggingPort_Response response;
          response.port = devtools_port_;
          result.set_response(std::move(response));
        }
        callback(std::move(result));
      });
  pending_callbacks.clear();
}
