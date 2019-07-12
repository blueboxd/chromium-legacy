// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_thread_proxy.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "ui/ozone/platform/drm/gpu/drm_thread_message_proxy.h"
#include "ui/ozone/platform/drm/gpu/drm_window_proxy.h"
#include "ui/ozone/platform/drm/gpu/gbm_pixmap.h"
#include "ui/ozone/platform/drm/gpu/proxy_helpers.h"

namespace ui {

namespace {

void OnBufferCreatedOnDrmThread(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    DrmThreadProxy::CreateBufferAsyncCallback callback,
    std::unique_ptr<GbmBuffer> buffer,
    scoped_refptr<DrmFramebuffer> framebuffer) {
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(std::move(callback), std::move(buffer),
                                       std::move(framebuffer)));
}

}  // namespace

DrmThreadProxy::DrmThreadProxy() = default;

DrmThreadProxy::~DrmThreadProxy() = default;

// Used only with the paramtraits implementation.
void DrmThreadProxy::BindThreadIntoMessagingProxy(
    InterThreadMessagingProxy* messaging_proxy) {
  messaging_proxy->SetDrmThread(&drm_thread_);
}

void DrmThreadProxy::StartDrmThread(base::OnceClosure binding_drainer) {
  drm_thread_.Start(std::move(binding_drainer));
}

std::unique_ptr<DrmWindowProxy> DrmThreadProxy::CreateDrmWindowProxy(
    gfx::AcceleratedWidget widget) {
  return std::make_unique<DrmWindowProxy>(widget, &drm_thread_);
}

void DrmThreadProxy::CreateBuffer(gfx::AcceleratedWidget widget,
                                  const gfx::Size& size,
                                  gfx::BufferFormat format,
                                  gfx::BufferUsage usage,
                                  uint32_t flags,
                                  std::unique_ptr<GbmBuffer>* buffer,
                                  scoped_refptr<DrmFramebuffer>* framebuffer) {
  DCHECK(drm_thread_.task_runner())
      << "no task runner! in DrmThreadProxy::CreateBuffer";
  PostSyncTask(
      drm_thread_.task_runner(),
      base::BindOnce(&DrmThread::CreateBuffer, base::Unretained(&drm_thread_),
                     widget, size, format, usage, flags, buffer, framebuffer));
}

void DrmThreadProxy::CreateBufferAsync(gfx::AcceleratedWidget widget,
                                       const gfx::Size& size,
                                       gfx::BufferFormat format,
                                       gfx::BufferUsage usage,
                                       uint32_t flags,
                                       CreateBufferAsyncCallback callback) {
  DCHECK(drm_thread_.task_runner())
      << "no task runner! in DrmThreadProxy::CreateBufferAsync";
  drm_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DrmThread::CreateBufferAsync,
                     base::Unretained(&drm_thread_), widget, size, format,
                     usage, flags,
                     base::BindOnce(OnBufferCreatedOnDrmThread,
                                    base::ThreadTaskRunnerHandle::Get(),
                                    std::move(callback))));
}

void DrmThreadProxy::CreateBufferFromHandle(
    gfx::AcceleratedWidget widget,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::NativePixmapHandle handle,
    std::unique_ptr<GbmBuffer>* buffer,
    scoped_refptr<DrmFramebuffer>* framebuffer) {
  PostSyncTask(drm_thread_.task_runner(),
               base::BindOnce(&DrmThread::CreateBufferFromHandle,
                              base::Unretained(&drm_thread_), widget, size,
                              format, std::move(handle), buffer, framebuffer));
}

void DrmThreadProxy::SetClearOverlayCacheCallback(
    base::RepeatingClosure callback) {
  DCHECK(drm_thread_.task_runner());

  drm_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DrmThread::SetClearOverlayCacheCallback,
                     base::Unretained(&drm_thread_),
                     CreateSafeRepeatingCallback(std::move(callback))));
}

void DrmThreadProxy::CheckOverlayCapabilities(
    gfx::AcceleratedWidget widget,
    const std::vector<OverlaySurfaceCandidate>& candidates,
    OverlayCapabilitiesCallback callback) {
  DCHECK(drm_thread_.task_runner());

  drm_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DrmThread::CheckOverlayCapabilities,
                     base::Unretained(&drm_thread_), widget, candidates,
                     CreateSafeOnceCallback(std::move(callback))));
}

void DrmThreadProxy::AddBindingDrmDevice(
    ozone::mojom::DrmDeviceRequest request) {
  DCHECK(drm_thread_.task_runner()) << "DrmThreadProxy::AddBindingDrmDevice "
                                       "drm_thread_ task runner missing";

  drm_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DrmThread::AddBindingDrmDevice,
                     base::Unretained(&drm_thread_), std::move(request)));
}

}  // namespace ui
