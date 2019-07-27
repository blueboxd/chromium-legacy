// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_SCENIC_GPU_SERVICE_H_
#define UI_OZONE_PLATFORM_SCENIC_SCENIC_GPU_SERVICE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "ui/ozone/public/mojom/scenic_gpu_host.mojom.h"
#include "ui/ozone/public/mojom/scenic_gpu_service.mojom.h"

namespace ui {

// GPU process service that enables presentation to Scenic.
//
// This object exposes a mojo service to the browser process from the GPU
// process. The browser binds it to enable exchange of Scenic resources.
// In particular, we must exchange export tokens for each view surface,
// so that surfaces can present to Scenic views managed by the browser.
class ScenicGpuService : public mojom::ScenicGpuService {
 public:
  ScenicGpuService(mojom::ScenicGpuHostRequest gpu_host_request);
  ~ScenicGpuService() override;

  base::RepeatingCallback<void(mojom::ScenicGpuServiceRequest)>
  GetBinderCallback();

  // mojom::ScenicGpuService:
  void Initialize(mojom::ScenicGpuHostPtr gpu_host) override;

 private:
  void AddBinding(mojom::ScenicGpuServiceRequest request);

  mojom::ScenicGpuHostRequest gpu_host_request_;

  mojo::BindingSet<mojom::ScenicGpuService> binding_set_;

  base::WeakPtrFactory<ScenicGpuService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ScenicGpuService);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_SCENIC_GPU_SERVICE_H_
