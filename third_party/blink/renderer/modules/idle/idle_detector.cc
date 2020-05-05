// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/idle/idle_detector.h"

#include <utility>

#include "base/time/time.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_idle_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

namespace {

using mojom::blink::IdleManagerError;

const char kFeaturePolicyBlocked[] =
    "Access to the feature \"idle-detection\" is disallowed by feature policy.";

constexpr base::TimeDelta kDefaultThreshold = base::TimeDelta::FromSeconds(60);
constexpr base::TimeDelta kMinimumThreshold = base::TimeDelta::FromSeconds(60);

}  // namespace

IdleDetector* IdleDetector::Create(ScriptState* script_state,
                                   const IdleOptions* options,
                                   ExceptionState& exception_state) {
  base::TimeDelta threshold =
      options->hasThreshold()
          ? base::TimeDelta::FromMilliseconds(options->threshold())
          : kDefaultThreshold;

  if (threshold < kMinimumThreshold) {
    exception_state.ThrowTypeError("Minimum threshold is 60 seconds.");
    return nullptr;
  }

  auto* detector = MakeGarbageCollected<IdleDetector>(
      ExecutionContext::From(script_state), threshold);
  return detector;
}

// static
IdleDetector* IdleDetector::Create(ScriptState* script_state,
                                   ExceptionState& exception_state) {
  return Create(script_state, IdleOptions::Create(), exception_state);
}

IdleDetector::IdleDetector(ExecutionContext* context, base::TimeDelta threshold)
    : ExecutionContextClient(context),
      threshold_(threshold),
      receiver_(this, context),
      idle_service_(context) {}

IdleDetector::~IdleDetector() = default;

const AtomicString& IdleDetector::InterfaceName() const {
  return event_target_names::kIdleDetector;
}

ExecutionContext* IdleDetector::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

bool IdleDetector::HasPendingActivity() const {
  // This object should be considered active as long as there are registered
  // event listeners.
  return GetExecutionContext() && HasEventListeners();
}

ScriptPromise IdleDetector::start(ScriptState* script_state,
                                  ExceptionState& exception_state) {
  // Validate options.
  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context->IsContextThread());

  if (!context->IsFeatureEnabled(
          mojom::blink::FeaturePolicyFeature::kIdleDetection,
          ReportOptions::kReportOnFailure)) {
    exception_state.ThrowSecurityError(kFeaturePolicyBlocked);
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  StartMonitoring(resolver);
  return promise;
}

void IdleDetector::stop() {
  receiver_.reset();
}

void IdleDetector::StartMonitoring(ScriptPromiseResolver* resolver) {
  if (receiver_.is_bound()) {
    resolver->Resolve();
    return;
  }

  // See https://bit.ly/2S0zRAS for task types.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);

  if (!idle_service_.is_bound()) {
    GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
        idle_service_.BindNewPipeAndPassReceiver(task_runner));
  }

  mojo::PendingRemote<mojom::blink::IdleMonitor> idle_monitor_remote;
  receiver_.Bind(idle_monitor_remote.InitWithNewPipeAndPassReceiver(),
                 task_runner);

  idle_service_->AddMonitor(
      threshold_, std::move(idle_monitor_remote),
      WTF::Bind(&IdleDetector::OnAddMonitor, WrapWeakPersistent(this),
                WrapPersistent(resolver)));
  return;
}

void IdleDetector::OnAddMonitor(ScriptPromiseResolver* resolver,
                                IdleManagerError error,
                                mojom::blink::IdleStatePtr state) {
  switch (error) {
    case IdleManagerError::kPermissionDisabled:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "Notification permission disabled"));
      return;
    case IdleManagerError::kSuccess:
      DCHECK(state);
      resolver->Resolve();
      Update(std::move(state));
      return;
  }
}

blink::IdleState* IdleDetector::state() const {
  return state_;
}

void IdleDetector::Update(mojom::blink::IdleStatePtr state) {
  DCHECK(receiver_.is_bound());
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed())
    return;

  if (state_ && state.get()->Equals(state_->state()))
    return;

  state_ = MakeGarbageCollected<blink::IdleState>(std::move(state));

  DispatchEvent(*Event::Create(event_type_names::kChange));
}

void IdleDetector::Trace(Visitor* visitor) {
  visitor->Trace(state_);
  visitor->Trace(receiver_);
  visitor->Trace(idle_service_);
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  ActiveScriptWrappable::Trace(visitor);
}

}  // namespace blink
