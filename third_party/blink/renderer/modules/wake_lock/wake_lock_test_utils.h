// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_TEST_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_TEST_UTILS_H_

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/device/public/mojom/wake_lock.mojom-blink.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/wake_lock/wake_lock.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/modules/wake_lock/wake_lock_type.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace blink {

// Mock WakeLock implementation that tracks whether it's bound or acquired, and
// provides a few helper methods to synchronously wait for RequestWakeLock()
// and CancelWakeLock() to be called.
class MockWakeLock : public device::mojom::blink::WakeLock {
 public:
  MockWakeLock() : binding_(this) {}
  ~MockWakeLock() override = default;

  bool is_acquired() const { return is_acquired_; }

  void Bind(device::mojom::blink::WakeLockRequest request) {
    DCHECK(!binding_.is_bound());
    binding_.Bind(std::move(request));
    binding_.set_connection_error_handler(
        WTF::Bind(&MockWakeLock::OnConnectionError, WTF::Unretained(this)));
  }

  // Forcefully terminate a binding to test connection errors.
  void Unbind() { OnConnectionError(); }

  // Synchronously wait for RequestWakeLock() to be called.
  void WaitForRequest() {
    DCHECK(!request_wake_lock_callback_);
    base::RunLoop run_loop;
    request_wake_lock_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // Synchronously wait for CancelWakeLock() to be called.
  void WaitForCancelation() {
    DCHECK(!cancel_wake_lock_callback_);
    base::RunLoop run_loop;
    cancel_wake_lock_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void OnConnectionError() {
    if (binding_.is_bound())
      binding_.Unbind();
    CancelWakeLock();
  }

  // device::mojom::blink::WakeLock implementation
  void RequestWakeLock() override {
    is_acquired_ = true;
    if (request_wake_lock_callback_)
      std::move(request_wake_lock_callback_).Run();
  }
  void CancelWakeLock() override {
    is_acquired_ = false;
    if (cancel_wake_lock_callback_)
      std::move(cancel_wake_lock_callback_).Run();
  }
  void AddClient(device::mojom::blink::WakeLockRequest) override {}
  void ChangeType(device::mojom::blink::WakeLockType,
                  ChangeTypeCallback) override {}
  void HasWakeLockForTests(HasWakeLockForTestsCallback) override {}

  bool is_acquired_ = false;

  base::OnceClosure request_wake_lock_callback_;
  base::OnceClosure cancel_wake_lock_callback_;

  mojo::Binding<device::mojom::blink::WakeLock> binding_;
};

// Mock WakeLockService implementation that creates a MockWakeLock in its
// GetWakeLock() implementation.
class MockWakeLockService : public mojom::blink::WakeLockService {
 public:
  MockWakeLockService() = default;
  ~MockWakeLockService() override = default;

  void BindRequest(mojo::ScopedMessagePipeHandle handle) {
    bindings_.AddBinding(
        this, mojom::blink::WakeLockServiceRequest(std::move(handle)));
  }

  MockWakeLock& get_wake_lock(WakeLockType type) {
    size_t pos = static_cast<size_t>(type);
    return mock_wake_lock_[pos];
  }

 private:
  WakeLockType ToBlinkWakeLockType(device::mojom::blink::WakeLockType type) {
    switch (type) {
      case device::mojom::blink::WakeLockType::kPreventDisplaySleep:
        return WakeLockType::kScreen;
      case device::mojom::blink::WakeLockType::kPreventAppSuspension:
        return WakeLockType::kSystem;
      default:
        NOTREACHED();
        return WakeLockType::kMaxValue;
    }
  }

  // mojom::blink::WakeLockService implementation
  void GetWakeLock(device::mojom::blink::WakeLockType type,
                   device::mojom::blink::WakeLockReason reason,
                   const String& description,
                   device::mojom::blink::WakeLockRequest request) override {
    size_t pos = static_cast<size_t>(ToBlinkWakeLockType(type));
    mock_wake_lock_[pos].Bind(std::move(request));
  }

  MockWakeLock
      mock_wake_lock_[static_cast<size_t>(WakeLockType::kMaxValue) + 1];
  mojo::BindingSet<mojom::blink::WakeLockService> bindings_;
};

// Overrides requests for WakeLockService with MockWakeLockService instances.
//
// Usage:
// TEST(Foo, Bar) {
//   MockWakeLockService mock_service;
//   WakeLockTestingContext context(&mock_service);
//   mojom::blink::WakeLockServicePtr service;
//   context.GetDocument()->GetInterfaceProvider()->GetInterface(
//       mojo::MakeRequest(&service));
//   service->GetWakeLock(...);  // Will call mock_service.GetWakeLock().
// }
class WakeLockTestingContext final {
  STACK_ALLOCATED();

 public:
  WakeLockTestingContext(MockWakeLockService* mock_wake_lock_service) {
    service_manager::InterfaceProvider::TestApi test_api(
        GetDocument()->GetInterfaceProvider());
    test_api.SetBinderForName(
        mojom::blink::WakeLockService::Name_,
        WTF::BindRepeating(&MockWakeLockService::BindRequest,
                           WTF::Unretained(mock_wake_lock_service)));
  }

  Document* GetDocument() { return &testing_scope_.GetDocument(); }
  LocalFrame* Frame() { return &testing_scope_.GetFrame(); }
  ScriptState* GetScriptState() { return testing_scope_.GetScriptState(); }

  // Synchronously waits for |promise| to be rejected.
  void WaitForPromiseRejection(ScriptPromise promise) {
    base::RunLoop run_loop;
    promise.Then(v8::Local<v8::Function>(),
                 ClosureRunnerFunction::CreateFunction(GetScriptState(),
                                                       run_loop.QuitClosure()));
    // Execute pending microtasks, otherwise it can take a few seconds for the
    // promise to resolve.
    v8::MicrotasksScope::PerformCheckpoint(GetScriptState()->GetIsolate());
    run_loop.Run();
  }

 private:
  // Helper class for WaitForPromiseRejection(). It provides a function that is
  // invoked when a ScriptPromise is rejected that invokes |callback|.
  class ClosureRunnerFunction final : public ScriptFunction {
   public:
    static v8::Local<v8::Function> CreateFunction(
        ScriptState* script_state,
        base::RepeatingClosure callback) {
      auto* function = MakeGarbageCollected<ClosureRunnerFunction>(
          script_state, std::move(callback));
      return function->BindToV8Function();
    }

    ClosureRunnerFunction(ScriptState* script_state,
                          base::RepeatingClosure callback)
        : ScriptFunction(script_state), callback_(std::move(callback)) {}

   private:
    ScriptValue Call(ScriptValue) override {
      if (callback_)
        std::move(callback_).Run();
      return ScriptValue();
    }

    base::RepeatingClosure callback_;
  };

  V8TestingScope testing_scope_;
};

// Shorthand for getting a PromiseState out of a ScriptPromise.
inline v8::Promise::PromiseState GetScriptPromiseState(
    const ScriptPromise& promise) {
  return promise.V8Value().As<v8::Promise>()->State();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WAKE_LOCK_WAKE_LOCK_TEST_UTILS_H_
