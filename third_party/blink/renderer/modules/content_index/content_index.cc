// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_index/content_index.h"

#include "base/optional.h"
#include "base/time/time.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/loader/threaded_icon_loader.h"
#include "third_party/blink/renderer/modules/content_index/content_description_type_converter.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

constexpr base::TimeDelta kIconFetchTimeout = base::TimeDelta::FromSeconds(30);

// Validates |description|. If there is an error, an error message to be passed
// to a TypeError is passed. Otherwise a null string is returned.
WTF::String ValidateDescription(const ContentDescription& description,
                                ServiceWorkerRegistration* registration) {
  // TODO(crbug.com/973844): Should field sizes be capped?

  if (description.id().IsEmpty())
    return "ID cannot be empty";

  if (description.title().IsEmpty())
    return "Title cannot be empty";

  if (description.description().IsEmpty())
    return "Description cannot be empty";

  if (description.iconUrl().IsEmpty())
    return "Invalid icon URL provided";

  if (description.launchUrl().IsEmpty())
    return "Invalid launch URL provided";

  KURL icon_url =
      registration->GetExecutionContext()->CompleteURL(description.iconUrl());
  if (!icon_url.ProtocolIsInHTTPFamily())
    return "Invalid icon URL protocol";

  KURL launch_url =
      registration->GetExecutionContext()->CompleteURL(description.launchUrl());
  auto* security_origin =
      registration->GetExecutionContext()->GetSecurityOrigin();
  if (!security_origin->CanRequest(launch_url))
    return "Service Worker cannot request provided launch URL";

  if (!launch_url.GetString().StartsWith(registration->scope()))
    return "Launch URL must belong to the Service Worker's scope";

  return WTF::String();
}

}  // namespace

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

  WTF::String description_error =
      ValidateDescription(*description, registration_.Get());
  if (!description_error.IsNull()) {
    return ScriptPromise::Reject(
        script_state, V8ThrowException::CreateTypeError(
                          script_state->GetIsolate(), description_error));
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  KURL icon_url =
      registration_->GetExecutionContext()->CompleteURL(description->iconUrl());
  ResourceRequest resource_request(icon_url);
  resource_request.SetRequestContext(mojom::RequestContextType::IMAGE);
  resource_request.SetPriority(ResourceLoadPriority::kMedium);
  resource_request.SetTimeoutInterval(kIconFetchTimeout);

  auto* threaded_icon_loader = MakeGarbageCollected<ThreadedIconLoader>();
  // TODO(crbug.com/973844): Use ideal icon dimensions instead of the max.
  threaded_icon_loader->Start(
      registration_->GetExecutionContext(), resource_request,
      /* resize_dimensions= */
      WebSize(mojom::blink::ContentIndexService::kMaxIconDimension,
              mojom::blink::ContentIndexService::kMaxIconDimension),
      WTF::Bind(&ContentIndex::DidGetIcon, WrapPersistent(this),
                WrapPersistent(resolver), WrapPersistent(threaded_icon_loader),
                mojom::blink::ContentDescription::From(description)));

  return promise;
}

void ContentIndex::DidGetIcon(ScriptPromiseResolver* resolver,
                              ThreadedIconLoader* loader,
                              mojom::blink::ContentDescriptionPtr description,
                              SkBitmap icon,
                              double resize_scale) {
  ScriptState* script_state = resolver->GetScriptState();
  ScriptState::Scope scope(script_state);

  if (icon.isNull()) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), "Icon could not be loaded"));
    return;
  }

  KURL launch_url = registration_->GetExecutionContext()->CompleteURL(
      description->launch_url);

  GetService()->Add(registration_->RegistrationId(), std::move(description),
                    icon, launch_url,
                    WTF::Bind(&ContentIndex::DidAdd, WrapPersistent(this),
                              WrapPersistent(resolver)));
}

void ContentIndex::DidAdd(ScriptPromiseResolver* resolver,
                          mojom::blink::ContentIndexError error) {
  ScriptState* script_state = resolver->GetScriptState();
  ScriptState::Scope scope(script_state);

  switch (error) {
    case mojom::blink::ContentIndexError::NONE:
      resolver->Resolve();
      return;
    case mojom::blink::ContentIndexError::STORAGE_ERROR:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError,
          "Failed to add description due to I/O error."));
      return;
    case mojom::blink::ContentIndexError::INVALID_PARAMETER:
      // The renderer should have been killed.
      NOTREACHED();
      return;
  }
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

  switch (error) {
    case mojom::blink::ContentIndexError::NONE:
      resolver->Resolve();
      return;
    case mojom::blink::ContentIndexError::STORAGE_ERROR:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError,
          "Failed to delete description due to I/O error."));
      return;
    case mojom::blink::ContentIndexError::INVALID_PARAMETER:
      // The renderer should have been killed.
      NOTREACHED();
      return;
  }
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

  HeapVector<Member<ContentDescription>> blink_descriptions;
  blink_descriptions.ReserveCapacity(descriptions.size());
  for (const auto& description : descriptions)
    blink_descriptions.push_back(description.To<blink::ContentDescription*>());

  switch (error) {
    case mojom::blink::ContentIndexError::NONE:
      resolver->Resolve(std::move(blink_descriptions));
      return;
    case mojom::blink::ContentIndexError::STORAGE_ERROR:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError,
          "Failed to get descriptions due to I/O error."));
      return;
    case mojom::blink::ContentIndexError::INVALID_PARAMETER:
      // The renderer should have been killed.
      NOTREACHED();
      return;
  }
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
