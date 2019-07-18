// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/context_provider_impl.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/async/default.h>
#include <lib/fdio/directory.h>
#include <lib/fdio/fd.h>
#include <lib/fdio/io.h>
#include <lib/zx/job.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <zircon/processargs.h>

#include <utility>
#include <vector>

#include "base/base_paths_fuchsia.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/scoped_file.h"
#include "base/fuchsia/default_job.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "content/public/common/content_switches.h"
#include "fuchsia/engine/common.h"
#include "net/http/http_util.h"
#include "services/service_manager/sandbox/fuchsia/sandbox_policy_fuchsia.h"

namespace {

// Returns the underlying channel if |directory| is a client endpoint for a
// |fuchsia::io::Directory| protocol. Otherwise, returns an empty channel.
zx::channel ValidateDirectoryAndTakeChannel(
    fidl::InterfaceHandle<fuchsia::io::Directory> directory_handle) {
  fidl::SynchronousInterfacePtr<fuchsia::io::Directory> directory =
      directory_handle.BindSync();
  zx_status_t status = ZX_ERR_INTERNAL;
  std::vector<uint8_t> entries;

  directory->ReadDirents(0, &status, &entries);
  if (status == ZX_OK) {
    return directory.Unbind().TakeChannel();
  }

  // Not a directory.
  return zx::channel();
}

// Verifies that Vulkan loader service is provided by the specified service
// directory.
bool CheckVulkanSupport(
    const fidl::InterfaceHandle<::fuchsia::io::Directory>& directory_handle,
    bool* vulkan_supported) {
  zx::channel dir_channel(fdio_service_clone(directory_handle.channel().get()));
  if (!dir_channel)
    return false;

  base::ScopedFD dir_fd;
  zx_status_t status = fdio_fd_create(dir_channel.release(),
                                      base::ScopedFD::Receiver(dir_fd).get());
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "fdio_fd_create()";
    return false;
  }

  struct stat statbuf;
  int result =
      fstatat(dir_fd.get(), "fuchsia.vulkan.loader.Loader", &statbuf, 0);
  *vulkan_supported = result == 0;

  return true;
}

}  // namespace

ContextProviderImpl::ContextProviderImpl() = default;

ContextProviderImpl::~ContextProviderImpl() = default;

void ContextProviderImpl::Create(
    fuchsia::web::CreateContextParams params,
    fidl::InterfaceRequest<fuchsia::web::Context> context_request) {
  if (!context_request.is_valid()) {
    DLOG(ERROR) << "Invalid |context_request|.";
    return;
  }
  if (!params.has_service_directory()) {
    DLOG(ERROR)
        << "Missing argument |service_directory| in CreateContextParams.";
    context_request.Close(ZX_ERR_INVALID_ARGS);
    return;
  }

  fidl::InterfaceHandle<::fuchsia::io::Directory> service_directory =
      std::move(*params.mutable_service_directory());

  // Enable Vulkan if the Vulkan loader service is present in the service
  // directory.
  bool vulkan_supported = false;
  if (!CheckVulkanSupport(service_directory, &vulkan_supported)) {
    // TODO(crbug.com/934539): Add type epitaph.
    DLOG(WARNING) << "Invalid |service_directory| in CreateContextParams.";
    return;
  }

  base::LaunchOptions launch_options;
  service_manager::SandboxPolicyFuchsia sandbox_policy;
  sandbox_policy.Initialize(service_manager::SANDBOX_TYPE_WEB_CONTEXT);
  sandbox_policy.SetServiceDirectory(std::move(service_directory));
  sandbox_policy.UpdateLaunchOptionsForSandbox(&launch_options);

  // Transfer the ContextRequest handle to a well-known location in the child
  // process' handle table.
  launch_options.handles_to_transfer.push_back(
      {kContextRequestHandleId, context_request.channel().get()});

  // Bind |data_directory| to /data directory, if provided.
  if (params.has_data_directory()) {
    zx::channel data_directory_channel = ValidateDirectoryAndTakeChannel(
        std::move(*params.mutable_data_directory()));
    if (data_directory_channel.get() == ZX_HANDLE_INVALID) {
      DLOG(ERROR)
          << "Invalid argument |data_directory| in CreateContextParams.";
      context_request.Close(ZX_ERR_INVALID_ARGS);
      return;
    }

    base::FilePath data_path;
    if (!base::PathService::Get(base::DIR_APP_DATA, &data_path)) {
      DLOG(ERROR) << "Failed to get data directory service path.";
      return;
    }
    launch_options.paths_to_transfer.push_back(
        base::PathToTransfer{data_path, data_directory_channel.release()});
  }

  // Isolate the child Context processes by containing them within their own
  // respective jobs.
  zx::job job;
  zx_status_t status = zx::job::create(*base::GetDefaultJob(), 0, &job);
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "zx_job_create";
    return;
  }
  launch_options.job_handle = job.get();

  base::CommandLine launch_command = *base::CommandLine::ForCurrentProcess();
  std::vector<zx::channel> devtools_listener_channels;

  if (params.has_remote_debugging_port()) {
    launch_command.AppendSwitchNative(
        switches::kRemoteDebuggingPort,
        base::NumberToString(params.remote_debugging_port()));
  } else if (devtools_listeners_.size() != 0) {
    // Connect DevTools listeners to the new Context process.
    std::vector<std::string> handles_ids;
    for (auto& devtools_listener : devtools_listeners_.ptrs()) {
      fidl::InterfaceHandle<fuchsia::web::DevToolsPerContextListener>
          client_listener;
      devtools_listener.get()->get()->OnContextDevToolsAvailable(
          client_listener.NewRequest());
      devtools_listener_channels.emplace_back(client_listener.TakeChannel());
      handles_ids.push_back(
          base::NumberToString(base::LaunchOptions::AddHandleToTransfer(
              &launch_options.handles_to_transfer,
              devtools_listener_channels.back().get())));
    }
    launch_command.AppendSwitchNative(kRemoteDebuggerHandles,
                                      base::JoinString(handles_ids, ","));
  }

#if defined(WEB_ENGINE_ENABLE_VULKAN)
  // Enable Vulkan when the Vulkan loader service is included in the service
  // directory.
  // TODO(https://crbug.com/962617): Enable Vulkan by default and remove this
  // hack.
  if (vulkan_supported) {
    launch_command.AppendSwitchASCII(
        "--enable-features", "DefaultEnableOopRasterization,UseSkiaRenderer");
    launch_command.AppendSwitch("--use-vulkan");
    launch_command.AppendSwitchASCII("--use-gl", "stub");
  }
#endif  // WEB_ENGINE_ENABLE_VULKAN

  // Validate embedder-supplied product, and optional version, and pass it to
  // the Context to include in the UserAgent.
  if (params.has_user_agent_product()) {
    if (!net::HttpUtil::IsToken(params.user_agent_product())) {
      DLOG(ERROR) << "Invalid embedder product.";
      context_request.Close(ZX_ERR_INVALID_ARGS);
      return;
    }
    std::string product_tag(params.user_agent_product());
    if (params.has_user_agent_version()) {
      if (!net::HttpUtil::IsToken(params.user_agent_version())) {
        DLOG(ERROR) << "Invalid embedder version.";
        context_request.Close(ZX_ERR_INVALID_ARGS);
        return;
      }
      product_tag += "/" + params.user_agent_version();
    }
    launch_command.AppendSwitchNative(kUserAgentProductAndVersion,
                                      std::move(product_tag));
  } else if (params.has_user_agent_version()) {
    DLOG(ERROR) << "Embedder version without product.";
    context_request.Close(ZX_ERR_INVALID_ARGS);
    return;
  }

  if (launch_for_test_)
    launch_for_test_.Run(launch_command, launch_options);
  else
    base::LaunchProcess(launch_command, launch_options);

  // |context_request| and any DevTools channels were transferred (not copied)
  // to the Context process.
  ignore_result(context_request.TakeChannel().release());
  for (auto& channel : devtools_listener_channels)
    ignore_result(channel.release());
}

void ContextProviderImpl::SetLaunchCallbackForTest(
    LaunchCallbackForTest launch) {
  launch_for_test_ = std::move(launch);
}

void ContextProviderImpl::EnableDevTools(
    fidl::InterfaceHandle<fuchsia::web::DevToolsListener> listener,
    EnableDevToolsCallback callback) {
  devtools_listeners_.AddInterfacePtr(listener.Bind());
  callback();
}
