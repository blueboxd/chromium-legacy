// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"

#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

DawnObjectBase::DawnObjectBase(
    scoped_refptr<DawnControlClientHolder> dawn_control_client)
    : dawn_control_client_(std::move(dawn_control_client)) {}

const scoped_refptr<DawnControlClientHolder>&
DawnObjectBase::GetDawnControlClient() const {
  return dawn_control_client_;
}

bool DawnObjectBase::IsDawnControlClientDestroyed() const {
  return dawn_control_client_->IsDestroyed();
}

gpu::webgpu::WebGPUInterface* DawnObjectBase::GetInterface() const {
  return dawn_control_client_->GetInterface();
}

const DawnProcTable& DawnObjectBase::GetProcs() const {
  return dawn_control_client_->GetProcs();
}

DawnDeviceClientSerializerHolder::DawnDeviceClientSerializerHolder(
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    uint64_t device_client_id)
    : dawn_control_client_(std::move(dawn_control_client)),
      device_client_id_(device_client_id) {}

DawnDeviceClientSerializerHolder::~DawnDeviceClientSerializerHolder() {
  if (dawn_control_client_->IsDestroyed()) {
    return;
  }
  dawn_control_client_->GetInterface()->RemoveDevice(device_client_id_);
}

DawnObjectImpl::DawnObjectImpl(GPUDevice* device)
    : DawnObjectBase(device->GetDawnControlClient()),
      device_(device),
      device_client_serializer_holder_(
          device->GetDeviceClientSerializerHolder()) {}

DawnObjectImpl::~DawnObjectImpl() = default;

void DawnObjectImpl::Trace(Visitor* visitor) {
  visitor->Trace(device_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
