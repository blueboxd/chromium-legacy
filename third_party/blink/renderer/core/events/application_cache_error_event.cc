// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/application_cache_error_event.h"

#include "third_party/blink/public/mojom/appcache/appcache.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_application_cache_error_event_init.h"
#include "third_party/blink/renderer/core/event_type_names.h"

namespace blink {

static const String& ErrorReasonToString(mojom::AppCacheErrorReason reason) {
  DEFINE_STATIC_LOCAL(String, error_manifest, ("manifest"));
  DEFINE_STATIC_LOCAL(String, error_signature, ("signature"));
  DEFINE_STATIC_LOCAL(String, error_resource, ("resource"));
  DEFINE_STATIC_LOCAL(String, error_changed, ("changed"));
  DEFINE_STATIC_LOCAL(String, error_abort, ("abort"));
  DEFINE_STATIC_LOCAL(String, error_quota, ("quota"));
  DEFINE_STATIC_LOCAL(String, error_policy, ("policy"));
  DEFINE_STATIC_LOCAL(String, error_unknown, ("unknown"));

  switch (reason) {
    case mojom::AppCacheErrorReason::APPCACHE_MANIFEST_ERROR:
      return error_manifest;
    case mojom::AppCacheErrorReason::APPCACHE_SIGNATURE_ERROR:
      return error_signature;
    case mojom::AppCacheErrorReason::APPCACHE_RESOURCE_ERROR:
      return error_resource;
    case mojom::AppCacheErrorReason::APPCACHE_CHANGED_ERROR:
      return error_changed;
    case mojom::AppCacheErrorReason::APPCACHE_ABORT_ERROR:
      return error_abort;
    case mojom::AppCacheErrorReason::APPCACHE_QUOTA_ERROR:
      return error_quota;
    case mojom::AppCacheErrorReason::APPCACHE_POLICY_ERROR:
      return error_policy;
    case mojom::AppCacheErrorReason::APPCACHE_UNKNOWN_ERROR:
      return error_unknown;
  }
  NOTREACHED();
  return g_empty_string;
}

ApplicationCacheErrorEvent::ApplicationCacheErrorEvent(
    mojom::AppCacheErrorReason reason,
    const String& url,
    uint16_t status,
    const String& message)
    : Event(event_type_names::kError, Bubbles::kNo, Cancelable::kNo),
      reason_(ErrorReasonToString(reason)),
      url_(url),
      status_(status),
      message_(message) {}

ApplicationCacheErrorEvent::ApplicationCacheErrorEvent(
    const AtomicString& event_type,
    const ApplicationCacheErrorEventInit* initializer)
    : Event(event_type, initializer), status_(0) {
  if (initializer->hasReason())
    reason_ = initializer->reason();
  if (initializer->hasUrl())
    url_ = initializer->url();
  if (initializer->hasStatus())
    status_ = initializer->status();
  if (initializer->hasMessage())
    message_ = initializer->message();
}

ApplicationCacheErrorEvent::~ApplicationCacheErrorEvent() = default;

void ApplicationCacheErrorEvent::Trace(Visitor* visitor) {
  Event::Trace(visitor);
}

}  // namespace blink
