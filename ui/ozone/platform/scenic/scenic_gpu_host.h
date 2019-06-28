// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_SCENIC_GPU_HOST_H_
#define UI_OZONE_PLATFORM_SCENIC_SCENIC_GPU_HOST_H_

#include <inttypes.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/interfaces/scenic_gpu_host.mojom.h"
#include "ui/ozone/public/interfaces/scenic_gpu_service.mojom.h"

namespace ui {
class ScenicWindowManager;

// Browser process object which supports a GPU process host.
//
// Once a GPU process starts, this objects binds to it over mojo and
// enables exchange of Scenic resources with it. In particular, we must
// exchange export tokens for each view surface, so that the surface can
// present to Scenic views managed by the browser.
class ScenicGpuHost : public mojom::ScenicGpuHost,
                      public GpuPlatformSupportHost {
 public:
  ScenicGpuHost(ScenicWindowManager* scenic_window_manager);
  ~ScenicGpuHost() override;

  // Creates browser process binding. This is used to create a software output
  // on the UI thread.
  mojom::ScenicGpuHostPtr CreateHostProcessSelfBinding();

  // mojom::ScenicGpuHost:
  void AttachSurfaceToWindow(int32_t window_id,
                             mojo::ScopedHandle export_token_mojo) override;

  // GpuPlatformSupportHost:
  void OnGpuProcessLaunched(
      int host_id,
      scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
      scoped_refptr<base::SingleThreadTaskRunner> send_runner,
      base::RepeatingCallback<void(IPC::Message*)> send_callback) override;
  void OnChannelDestroyed(int host_id) override;
  void OnMessageReceived(const IPC::Message& message) override;
  void OnGpuServiceLaunched(
      int host_id,
      scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_runner,
      GpuHostBindInterfaceCallback binder,
      GpuHostTerminateCallback terminate_callback) override;

 private:
  void OnGpuServiceLaunchedOnUI(
      mojo::InterfacePtrInfo<mojom::ScenicGpuService> gpu_service_ptr_info);
  void UpdateBinding(uint32_t service_launch_count,
                     mojom::ScenicGpuHostRequest scenic_gpu_host_request);

  ScenicWindowManager* const scenic_window_manager_;
  mojo::Binding<mojom::ScenicGpuHost> host_binding_;
  mojo::Binding<mojom::ScenicGpuHost> gpu_binding_;

  mojom::ScenicGpuServicePtr gpu_service_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_thread_runner_;

  THREAD_CHECKER(ui_thread_checker_);
  THREAD_CHECKER(io_thread_checker_);

  base::WeakPtrFactory<ScenicGpuHost> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ScenicGpuHost);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_SCENIC_GPU_HOST_H_
