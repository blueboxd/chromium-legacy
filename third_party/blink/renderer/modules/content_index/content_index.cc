// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_index/content_index.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

ContentIndex::ContentIndex(ServiceWorkerRegistration* registration,
                           scoped_refptr<base::SequencedTaskRunner> task_runner)
    : registration_(registration), task_runner_(std::move(task_runner)) {
  DCHECK(registration_);
}

ContentIndex::~ContentIndex() = default;

ScriptPromise ContentIndex::add(ScriptState* script_state,
                                const ContentDescription* description) {
  if (!registration_->active()) {
    return ScriptPromise::Reject(
        script_state,
        V8ThrowException::CreateTypeError(script_state->GetIsolate(),
                                          "No active registration available on "
                                          "the ServiceWorkerRegistration."));
  }

  // TODO(crbug.com/973844): Fetch the icon and add a struct trait to convert
  // from `ContentDescription` to `mojom::blink::ContentDescriptionPtr`.
  return ScriptPromise::RejectWithDOMException(
      script_state, MakeGarbageCollected<DOMException>(
                        DOMExceptionCode::kNotSupportedError,
                        "ContentIndex::add is not yet implemented"));
}

ScriptPromise ContentIndex::deleteDescription(ScriptState* script_state,
                                              const String& id) {
  if (!registration_->active()) {
    return ScriptPromise::Reject(
        script_state,
        V8ThrowException::CreateTypeError(script_state->GetIsolate(),
                                          "No active registration available on "
                                          "the ServiceWorkerRegistration."));
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  GetService()->Delete(
      registration_->RegistrationId(), id,
      WTF::Bind(&ContentIndex::DidDeleteDescription, WrapPersistent(this),
                WrapPersistent(resolver)));

  return promise;
}

void ContentIndex::DidDeleteDescription(ScriptPromiseResolver* resolver,
                                        mojom::blink::ContentIndexError error) {
  ScriptState* script_state = resolver->GetScriptState();
  ScriptState::Scope scope(script_state);

  resolver->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNotSupportedError,
      "ContentIndex::delete is not yet implemented"));
}

ScriptPromise ContentIndex::getDescriptions(ScriptState* script_state) {
  if (!registration_->active()) {
    return ScriptPromise::Reject(
        script_state,
        V8ThrowException::CreateTypeError(script_state->GetIsolate(),
                                          "No active registration available on "
                                          "the ServiceWorkerRegistration."));
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  GetService()->GetDescriptions(
      registration_->RegistrationId(),
      WTF::Bind(&ContentIndex::DidGetDescriptions, WrapPersistent(this),
                WrapPersistent(resolver)));

  return promise;
}

void ContentIndex::DidGetDescriptions(
    ScriptPromiseResolver* resolver,
    mojom::blink::ContentIndexError error,
    Vector<mojom::blink::ContentDescriptionPtr> descriptions) {
  ScriptState* script_state = resolver->GetScriptState();
  ScriptState::Scope scope(script_state);

  resolver->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNotSupportedError,
      "ContentIndex::getDescriptions is not yet implemented"));
}

void ContentIndex::Trace(blink::Visitor* visitor) {
  visitor->Trace(registration_);
  ScriptWrappable::Trace(visitor);
}

mojom::blink::ContentIndexService* ContentIndex::GetService() {
  if (!content_index_service_) {
    registration_->GetExecutionContext()->GetInterfaceProvider()->GetInterface(
        mojo::MakeRequest(&content_index_service_, task_runner_));
  }
  return content_index_service_.get();
}

}  // namespace blink
