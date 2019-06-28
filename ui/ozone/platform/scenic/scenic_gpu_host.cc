// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_gpu_host.h"

#include <inttypes.h>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/ozone/platform/scenic/scenic_window.h"
#include "ui/ozone/platform/scenic/scenic_window_manager.h"
#include "ui/ozone/public/interfaces/scenic_gpu_host.mojom.h"
#include "ui/ozone/public/interfaces/scenic_gpu_service.mojom.h"

namespace {

using BinderCallback =
    base::RepeatingCallback<void(const std::string&,
                                 mojo::ScopedMessagePipeHandle)>;

template <typename Interface>
void BindInterface(mojo::InterfaceRequest<Interface> request,
                   const BinderCallback& binder_callback) {
  binder_callback.Run(Interface::Name_, request.PassMessagePipe());
}

}  // namespace

namespace ui {

ScenicGpuHost::ScenicGpuHost(ScenicWindowManager* scenic_window_manager)
    : scenic_window_manager_(scenic_window_manager),
      host_binding_(this),
      gpu_binding_(this),
      ui_thread_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_ptr_factory_(this) {
  DETACH_FROM_THREAD(io_thread_checker_);
}

ScenicGpuHost::~ScenicGpuHost() {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
}

mojom::ScenicGpuHostPtr ScenicGpuHost::CreateHostProcessSelfBinding() {
  DCHECK(!host_binding_.is_bound());
  mojom::ScenicGpuHostPtr gpu_host;
  host_binding_.Bind(mojo::MakeRequest(&gpu_host));
  return gpu_host;
}

void ScenicGpuHost::AttachSurfaceToWindow(
    int32_t window_id,
    mojo::ScopedHandle export_token_mojo) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  ScenicWindow* scenic_window = scenic_window_manager_->GetWindow(window_id);
  if (!scenic_window)
    return;
  fuchsia::ui::gfx::ExportToken export_token;
  export_token.value = zx::eventpair(
      mojo::UnwrapPlatformHandle(std::move(export_token_mojo)).TakeHandle());
  scenic_window->AttachSurface(std::move(export_token));
}

void ScenicGpuHost::OnGpuProcessLaunched(
    int host_id,
    scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
    scoped_refptr<base::SingleThreadTaskRunner> send_runner,
    base::RepeatingCallback<void(IPC::Message*)> send_callback) {
  NOTREACHED();
}

void ScenicGpuHost::OnChannelDestroyed(int host_id) {}

void ScenicGpuHost::OnGpuServiceLaunched(
    int host_id,
    scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_runner,
    GpuHostBindInterfaceCallback binder,
    GpuHostTerminateCallback terminate_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

  mojom::ScenicGpuServicePtr scenic_gpu_service;
  BindInterface(mojo::MakeRequest(&scenic_gpu_service), binder);
  ui_thread_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ScenicGpuHost::OnGpuServiceLaunchedOnUI,
                                weak_ptr_factory_.GetWeakPtr(),
                                scenic_gpu_service.PassInterface()));
}

void ScenicGpuHost::OnGpuServiceLaunchedOnUI(
    mojo::InterfacePtrInfo<mojom::ScenicGpuService> gpu_service_ptr_info) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);

  mojom::ScenicGpuHostPtr gpu_host;
  gpu_binding_.Close();
  gpu_binding_.Bind(mojo::MakeRequest(&gpu_host));

  gpu_service_.Bind(std::move(gpu_service_ptr_info));
  gpu_service_->Initialize(std::move(gpu_host));
}

void ScenicGpuHost::OnMessageReceived(const IPC::Message& message) {
  NOTREACHED();
}

}  // namespace ui
