// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/null_execution_context.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/security_context_init.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/dom_timer.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/platform/scheduler/public/dummy_schedulers.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

namespace blink {

NullExecutionContext::NullExecutionContext(
    OriginTrialContext* origin_trial_context)
    : ExecutionContext(v8::Isolate::GetCurrent()),
      security_context_(
          SecurityContextInit(
              nullptr /* origin */,
              origin_trial_context,
              MakeGarbageCollected<Agent>(v8::Isolate::GetCurrent(),
                                          base::UnguessableToken::Null())),
          SecurityContext::kLocal),
      scheduler_(scheduler::CreateDummyFrameScheduler()) {
  if (origin_trial_context)
    origin_trial_context->BindExecutionContext(this);
}

NullExecutionContext::~NullExecutionContext() {}

void NullExecutionContext::SetUpSecurityContextForTesting() {
  auto* policy = MakeGarbageCollected<ContentSecurityPolicy>();
  GetSecurityContext().SetSecurityOriginForTesting(
      SecurityOrigin::Create(url_));
  policy->BindToDelegate(GetContentSecurityPolicyDelegate());
  GetSecurityContext().SetContentSecurityPolicy(policy);
}

FrameOrWorkerScheduler* NullExecutionContext::GetScheduler() {
  return scheduler_.get();
}

scoped_refptr<base::SingleThreadTaskRunner> NullExecutionContext::GetTaskRunner(
    TaskType) {
  return Thread::Current()->GetTaskRunner();
}

BrowserInterfaceBrokerProxy& NullExecutionContext::GetBrowserInterfaceBroker() {
  return GetEmptyBrowserInterfaceBroker();
}

void NullExecutionContext::Trace(Visitor* visitor) {
  visitor->Trace(security_context_);
  ExecutionContext::Trace(visitor);
}

}  // namespace blink
