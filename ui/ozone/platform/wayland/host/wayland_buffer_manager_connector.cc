// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_connector.h"

#include "base/bind.h"
#include "base/task_runner_util.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"

namespace ui {

namespace {

// TODO(msisov): In the future when GpuProcessHost is moved to vizhost, remove
// this utility code.
using BinderCallback = ui::GpuPlatformSupportHost::GpuHostBindInterfaceCallback;

void BindInterfaceInGpuProcess(const std::string& interface_name,
                               mojo::ScopedMessagePipeHandle interface_pipe,
                               const BinderCallback& binder_callback) {
  return binder_callback.Run(interface_name, std::move(interface_pipe));
}

template <typename Interface>
void BindInterfaceInGpuProcess(mojo::InterfaceRequest<Interface> request,
                               const BinderCallback& binder_callback) {
  BindInterfaceInGpuProcess(
      Interface::Name_, std::move(request.PassMessagePipe()), binder_callback);
}

}  // namespace

WaylandBufferManagerConnector::WaylandBufferManagerConnector(
    WaylandBufferManagerHost* buffer_manager)
    : buffer_manager_(buffer_manager) {}

WaylandBufferManagerConnector::~WaylandBufferManagerConnector() = default;

void WaylandBufferManagerConnector::OnGpuProcessLaunched(
    int host_id,
    scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
    scoped_refptr<base::SingleThreadTaskRunner> send_runner,
    base::RepeatingCallback<void(IPC::Message*)> send_callback) {}

void WaylandBufferManagerConnector::OnChannelDestroyed(int host_id) {
  buffer_manager_->OnChannelDestroyed();
}

void WaylandBufferManagerConnector::OnMessageReceived(
    const IPC::Message& message) {
  NOTREACHED() << "This class should only be used with mojo transport but here "
                  "we're wrongly getting invoked to handle IPC communication.";
}

void WaylandBufferManagerConnector::OnGpuServiceLaunched(
    int host_id,
    scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_runner,
    GpuHostBindInterfaceCallback binder,
    GpuHostTerminateCallback terminate_callback) {
  terminate_callback_ = std::move(terminate_callback);
  binder_ = std::move(binder);

  io_runner_ = io_runner;
  auto on_terminate_gpu_cb =
      base::BindOnce(&WaylandBufferManagerConnector::OnTerminateGpuProcess,
                     base::Unretained(this));
  buffer_manager_->SetTerminateGpuCallback(std::move(on_terminate_gpu_cb));

  base::PostTaskAndReplyWithResult(
      ui_runner.get(), FROM_HERE,
      base::BindOnce(&WaylandBufferManagerHost::BindInterface,
                     base::Unretained(buffer_manager_)),
      base::BindOnce(
          &WaylandBufferManagerConnector::OnBufferManagerHostPtrBinded,
          base::Unretained(this)));
}

void WaylandBufferManagerConnector::OnBufferManagerHostPtrBinded(
    ozone::mojom::WaylandBufferManagerHostPtr buffer_manager_host_ptr) const {
  ozone::mojom::WaylandBufferManagerGpuPtr buffer_manager_gpu_ptr;
  auto request = mojo::MakeRequest(&buffer_manager_gpu_ptr);
  BindInterfaceInGpuProcess(std::move(request), binder_);
  DCHECK(buffer_manager_host_ptr);
  buffer_manager_gpu_ptr->SetWaylandBufferManagerHost(
      std::move(buffer_manager_host_ptr));

#if defined(WAYLAND_GBM)
  if (!buffer_manager_->CanCreateDmabufBasedBuffer()) {
    LOG(WARNING) << "zwp_linux_dmabuf is not available.";
    buffer_manager_gpu_ptr->ResetGbmDevice();
  }
#endif
}

void WaylandBufferManagerConnector::OnTerminateGpuProcess(std::string message) {
  io_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(terminate_callback_),
                                                 std::move(message)));
}

}  // namespace ui
