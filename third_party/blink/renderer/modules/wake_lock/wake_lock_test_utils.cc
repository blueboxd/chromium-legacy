// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/wake_lock/wake_lock_test_utils.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"

namespace blink {

using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionStatus;

// MockPermissionService

MockPermissionService::MockPermissionService() = default;
MockPermissionService::~MockPermissionService() = default;

void MockPermissionService::BindRequest(mojo::ScopedMessagePipeHandle handle) {
  DCHECK(!binding_.is_bound());
  binding_.Bind(mojom::blink::PermissionServiceRequest(std::move(handle)));
  binding_.set_connection_error_handler(WTF::Bind(
      &MockPermissionService::OnConnectionError, WTF::Unretained(this)));
}

void MockPermissionService::SetPermissionResponse(WakeLockType type,
                                                  PermissionStatus status) {
  DCHECK(status == PermissionStatus::GRANTED ||
         status == PermissionStatus::DENIED);
  permission_responses_[static_cast<size_t>(type)] = status;
}

void MockPermissionService::OnConnectionError() {
  binding_.Unbind();
}

bool MockPermissionService::GetWakeLockTypeFromDescriptor(
    const PermissionDescriptorPtr& descriptor,
    WakeLockType* output) {
  if (!descriptor->extension || !descriptor->extension->is_wake_lock())
    return false;
  switch (descriptor->extension->get_wake_lock()->type) {
    case mojom::blink::WakeLockType::kScreen:
      *output = WakeLockType::kScreen;
      return true;
    case mojom::blink::WakeLockType::kSystem:
      *output = WakeLockType::kSystem;
      return true;
    default:
      return false;
  }
}

void MockPermissionService::WaitForPermissionRequest(WakeLockType type) {
  size_t pos = static_cast<size_t>(type);
  DCHECK(!request_permission_callbacks_[pos]);
  base::RunLoop run_loop;
  request_permission_callbacks_[pos] = run_loop.QuitClosure();
  run_loop.Run();
}

void MockPermissionService::HasPermission(PermissionDescriptorPtr permission,
                                          HasPermissionCallback callback) {
  WakeLockType type;
  if (!GetWakeLockTypeFromDescriptor(permission, &type)) {
    std::move(callback).Run(PermissionStatus::DENIED);
    return;
  }
  size_t pos = static_cast<size_t>(type);
  DCHECK(permission_responses_[pos].has_value());
  std::move(callback).Run(
      permission_responses_[pos].value_or(PermissionStatus::DENIED));
}

void MockPermissionService::RequestPermission(
    PermissionDescriptorPtr permission,
    bool user_gesture,
    RequestPermissionCallback callback) {
  WakeLockType type;
  if (!GetWakeLockTypeFromDescriptor(permission, &type)) {
    std::move(callback).Run(PermissionStatus::DENIED);
    return;
  }

  size_t pos = static_cast<size_t>(type);
  DCHECK(permission_responses_[pos].has_value());
  if (request_permission_callbacks_[pos])
    std::move(request_permission_callbacks_[pos]).Run();
  std::move(callback).Run(
      permission_responses_[pos].value_or(PermissionStatus::DENIED));
}

void MockPermissionService::RequestPermissions(
    Vector<PermissionDescriptorPtr> permissions,
    bool user_gesture,
    mojom::blink::PermissionService::RequestPermissionsCallback) {
  NOTREACHED();
}

void MockPermissionService::RevokePermission(PermissionDescriptorPtr permission,
                                             RevokePermissionCallback) {
  NOTREACHED();
}

void MockPermissionService::AddPermissionObserver(
    PermissionDescriptorPtr permission,
    PermissionStatus last_known_status,
    mojom::blink::PermissionObserverPtr) {
  NOTREACHED();
}

// ScriptPromiseUtils

// static
v8::Promise::PromiseState ScriptPromiseUtils::GetPromiseState(
    const ScriptPromise& promise) {
  return promise.V8Value().As<v8::Promise>()->State();
}

// static
String ScriptPromiseUtils::GetPromiseResolutionAsString(
    const ScriptPromise& promise) {
  auto v8_promise = promise.V8Value().As<v8::Promise>();
  if (v8_promise->State() == v8::Promise::kPending) {
    return g_empty_string;
  }
  ScriptValue promise_result(promise.GetScriptValue().GetScriptState(),
                             v8_promise->Result());
  String value;
  if (!promise_result.ToString(value)) {
    return g_empty_string;
  }
  return value;
}

// static
DOMException* ScriptPromiseUtils::GetPromiseResolutionAsDOMException(
    const ScriptPromise& promise) {
  return V8DOMException::ToImplWithTypeCheck(
      promise.GetIsolate(), promise.V8Value().As<v8::Promise>()->Result());
}

}  // namespace blink
