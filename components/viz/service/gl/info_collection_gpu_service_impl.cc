// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/gl/info_collection_gpu_service_impl.h"

#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "gpu/config/dx_diag_node.h"
#include "gpu/config/gpu_info_collector.h"

namespace viz {

InfoCollectionGpuServiceImpl::InfoCollectionGpuServiceImpl(
    scoped_refptr<base::SingleThreadTaskRunner> main_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_runner,
    const gpu::DevicePerfInfo& device_perf_info,
    mojo::PendingReceiver<mojom::InfoCollectionGpuService> pending_receiver)
    : main_runner_(std::move(main_runner)),
      io_runner_(std::move(io_runner)),
      device_perf_info_(device_perf_info) {
  DCHECK(!io_runner_->BelongsToCurrentThread());
  DCHECK(main_runner_->BelongsToCurrentThread());

  io_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&InfoCollectionGpuServiceImpl::BindOnIO,
                     base::Unretained(this), std::move(pending_receiver)));
}

InfoCollectionGpuServiceImpl::~InfoCollectionGpuServiceImpl() {
  // Info Collection GPU process exits atomically.
  NOTREACHED();
}

void InfoCollectionGpuServiceImpl::BindOnIO(
    mojo::PendingReceiver<mojom::InfoCollectionGpuService> pending_receiver) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  DCHECK(!receiver_.is_bound());

  receiver_.Bind(std::move(pending_receiver));
}

void InfoCollectionGpuServiceImpl::
    GetGpuSupportedRuntimeVersionAndDevicePerfInfo(
        GetGpuSupportedRuntimeVersionAndDevicePerfInfoCallback callback) {
  DCHECK(io_runner_->BelongsToCurrentThread());

  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&InfoCollectionGpuServiceImpl::
                         GetGpuSupportedRuntimeVersionAndDevicePerfInfoOnMain,
                     base::Unretained(this), std::move(callback)));
}

void InfoCollectionGpuServiceImpl::
    GetGpuSupportedRuntimeVersionAndDevicePerfInfoOnMain(
        GetGpuSupportedRuntimeVersionAndDevicePerfInfoCallback callback) {
  DCHECK(main_runner_->BelongsToCurrentThread());

  gpu::Dx12VulkanVersionInfo dx12_vulkan_version_info;
  gpu::RecordGpuSupportedRuntimeVersionHistograms(&dx12_vulkan_version_info);

  io_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), dx12_vulkan_version_info,
                                device_perf_info_));
}

void InfoCollectionGpuServiceImpl::RequestDxDiagNodeInfo(
    RequestDxDiagNodeInfoCallback callback) {
  DCHECK(io_runner_->BelongsToCurrentThread());

  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&InfoCollectionGpuServiceImpl::RequestDxDiagNodeInfoOnMain,
                     base::Unretained(this), std::move(callback)));
}

void InfoCollectionGpuServiceImpl::RequestDxDiagNodeInfoOnMain(
    RequestDxDiagNodeInfoCallback callback) {
  DCHECK(main_runner_->BelongsToCurrentThread());

  // We can continue on shutdown here because we're not writing any critical
  // state in this task.
  base::PostTaskAndReplyWithResult(
      base::ThreadPool::CreateCOMSTATaskRunner(
          {base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})
          .get(),
      FROM_HERE, base::BindOnce([]() {
        gpu::DxDiagNode dx_diag_node;
        gpu::GetDxDiagnostics(&dx_diag_node);
        return dx_diag_node;
      }),
      base::BindOnce(
          [](RequestDxDiagNodeInfoCallback callback,
             scoped_refptr<base::SingleThreadTaskRunner> io_runner,
             const gpu::DxDiagNode& dx_diag_node) {
            io_runner->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), dx_diag_node));
          },
          std::move(callback), io_runner_));
}

}  // namespace viz
