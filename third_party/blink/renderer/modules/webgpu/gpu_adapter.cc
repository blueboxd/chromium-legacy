// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_extensions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_request_adapter_options.h"

namespace blink {

namespace {
WGPUDeviceProperties AsDawnType(const GPUDeviceDescriptor* descriptor) {
  DCHECK_NE(nullptr, descriptor);

  WGPUDeviceProperties requested_device_properties = {};
  requested_device_properties.textureCompressionBC =
      descriptor->extensions()->textureCompressionBC();

  return requested_device_properties;
}
}  // anonymous namespace

// static
GPUAdapter* GPUAdapter::Create(
    const String& name,
    uint32_t adapter_service_id,
    const WGPUDeviceProperties& properties,
    scoped_refptr<DawnControlClientHolder> dawn_control_client) {
  return MakeGarbageCollected<GPUAdapter>(name, adapter_service_id, properties,
                                          std::move(dawn_control_client));
}

GPUAdapter::GPUAdapter(
    const String& name,
    uint32_t adapter_service_id,
    const WGPUDeviceProperties& properties,
    scoped_refptr<DawnControlClientHolder> dawn_control_client)
    : DawnObjectBase(dawn_control_client),
      name_(name),
      adapter_service_id_(adapter_service_id),
      adapter_properties_(properties) {}

const String& GPUAdapter::name() const {
  return name_;
}

ScriptValue GPUAdapter::extensions(ScriptState* script_state) const {
  V8ObjectBuilder object_builder(script_state);
  object_builder.AddBoolean("textureCompressionBC",
                            adapter_properties_.textureCompressionBC);
  return object_builder.GetScriptValue();
}

ScriptPromise GPUAdapter::requestDevice(ScriptState* script_state,
                                        const GPUDeviceDescriptor* descriptor) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  WGPUDeviceProperties requested_device_properties = AsDawnType(descriptor);
  GetInterface()->RequestDevice(adapter_service_id_,
                                &requested_device_properties);

  // TODO(jiawei.shao@intel.com): create GPUDevice in the callback of
  // GetInterface()->RequestDevice().
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  GPUDevice* device = GPUDevice::Create(
      execution_context, GetDawnControlClient(), this, descriptor);

  resolver->Resolve(device);
  return promise;
}

}  // namespace blink
