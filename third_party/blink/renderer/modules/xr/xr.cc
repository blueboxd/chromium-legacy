// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr.h"

#include <utility>

#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
namespace blink {

namespace {

const char kNavigatorDetachedError[] =
    "The navigator.xr object is no longer associated with a document.";

const char kPageNotVisible[] = "The page is not visible";

const char kFeaturePolicyBlocked[] =
    "Access to the feature \"xr\" is disallowed by feature policy.";

const char kActiveImmersiveSession[] =
    "There is already an active, immersive XRSession.";

const char kRequestRequiresUserActivation[] =
    "The requested session requires user activation.";

const char kSessionNotSupported[] =
    "The specified session configuration is not supported.";

const char kNoDevicesMessage[] = "No XR hardware found.";

const char kImmersiveArModeNotValid[] =
    "Failed to execute '%s' on 'XR': The provided value 'immersive-ar' is not "
    "a valid enum value of type XRSessionMode.";

// Helper method to convert session mode into Mojo options.
device::mojom::blink::XRSessionOptionsPtr convertModeToMojo(
    XRSession::SessionMode mode) {
  auto session_options = device::mojom::blink::XRSessionOptions::New();
  session_options->immersive = (mode == XRSession::kModeImmersiveVR ||
                                mode == XRSession::kModeImmersiveAR);
  session_options->environment_integration =
      mode == XRSession::kModeImmersiveAR;

  return session_options;
}

XRSession::SessionMode stringToSessionMode(const String& mode_string) {
  if (mode_string == "inline") {
    return XRSession::kModeInline;
  }
  if (mode_string == "immersive-vr") {
    return XRSession::kModeImmersiveVR;
  }
  if (mode_string == "immersive-ar") {
    return XRSession::kModeImmersiveAR;
  }

  NOTREACHED();  // Only strings in the enum are allowed by IDL.
  return XRSession::kModeInline;
}

const char* SessionModeToString(XRSession::SessionMode session_mode) {
  switch (session_mode) {
    case XRSession::SessionMode::kModeInline:
      return "inline";
    case XRSession::SessionMode::kModeImmersiveVR:
      return "immersive-vr";
    case XRSession::SessionMode::kModeImmersiveAR:
      return "immersive-ar";
  }

  NOTREACHED();
  return "";
}

// Converts the given string to an XRSessionFeature. If the string is
// unrecognized, returns nullopt. Based on the spec:
// https://immersive-web.github.io/webxr/#feature-name
base::Optional<device::mojom::XRSessionFeature> StringToXRSessionFeature(
    const String& feature_string) {
  if (feature_string == "viewer") {
    return device::mojom::XRSessionFeature::REF_SPACE_VIEWER;
  } else if (feature_string == "local") {
    return device::mojom::XRSessionFeature::REF_SPACE_LOCAL;
  } else if (feature_string == "local-floor") {
    return device::mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR;
  } else if (feature_string == "bounded-floor") {
    return device::mojom::XRSessionFeature::REF_SPACE_BOUNDED_FLOOR;
  } else if (feature_string == "unbounded") {
    return device::mojom::XRSessionFeature::REF_SPACE_UNBOUNDED;
  }

  return base::nullopt;
}

bool IsFeatureValidForMode(device::mojom::XRSessionFeature feature,
                           XRSession::SessionMode mode) {
  switch (feature) {
    case device::mojom::XRSessionFeature::REF_SPACE_VIEWER:
    case device::mojom::XRSessionFeature::REF_SPACE_LOCAL:
    case device::mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR:
      return true;
    case device::mojom::XRSessionFeature::REF_SPACE_BOUNDED_FLOOR:
    case device::mojom::XRSessionFeature::REF_SPACE_UNBOUNDED:
      return mode == XRSession::kModeImmersiveVR ||
             mode == XRSession::kModeImmersiveAR;
  }
}

template <typename Fn>
XRSessionFeatureSet ParseRequestedFeatures(
    const WTF::Vector<ScriptValue>& features,
    XRSession::SessionMode session_mode,
    Fn&& error_fn) {
  XRSessionFeatureSet result;

  // Iterate over all requested features, even if intermediate
  // elements are found to be invalid.
  for (const auto& feature : features) {
    String feature_string;
    if (feature.ToString(feature_string)) {
      auto feature_enum = StringToXRSessionFeature(feature_string);

      if (!feature_enum) {
        String error_message =
            "Unrecognized feature requested: " + feature_string;
        error_fn(std::move(error_message));
      } else if (!IsFeatureValidForMode(feature_enum.value(), session_mode)) {
        String error_message =
            "Feature '" + feature_string +
            "' is not supported for mode: " + SessionModeToString(session_mode);
        error_fn(std::move(error_message));
      } else {
        result.insert(feature_enum.value());
      }
    } else {
      error_fn("Unrecognized feature value");
    }
  }

  return result;
}

// Ensure that the immersive session request is allowed, if not
// return which security error occurred.
// https://immersive-web.github.io/webxr/#immersive-session-request-is-allowed
const char* CheckImmersiveSessionRequestAllowed(LocalFrame* frame,
                                                Document* doc) {
  // Ensure that the session was initiated by a user gesture
  if (!LocalFrame::HasTransientUserActivation(frame)) {
    return kRequestRequiresUserActivation;
  }

  // Check that the document is "trustworthy"
  // https://immersive-web.github.io/webxr/#trustworthy
  if (!doc->IsPageVisible()) {
    return kPageNotVisible;
  }

  if (!doc->IsFeatureEnabled(mojom::FeaturePolicyFeature::kWebXr,
                             ReportOptions::kReportOnFailure)) {
    return kFeaturePolicyBlocked;
  }

  // Consent occurs in the Browser process.

  return nullptr;
}

}  // namespace

// Ensure that the inline session request is allowed, if not
// return which security error occurred.
// https://immersive-web.github.io/webxr/#inline-session-request-is-allowed
const char* XR::CheckInlineSessionRequestAllowed(
    LocalFrame* frame,
    Document* doc,
    const PendingRequestSessionQuery& query) {
  // Without user activation, we must reject the session if *any* features
  // (optional or required) were present, whether or not they were recognized.
  // The only exception to this is the 'viewer' feature.
  if (!LocalFrame::HasTransientUserActivation(frame)) {
    if (query.InvalidOptionalFeatures() || query.InvalidRequiredFeatures()) {
      return kRequestRequiresUserActivation;
    }

    // If any required features (besides 'viewer') were requested, reject.
    for (auto feature : query.RequiredFeatures()) {
      if (feature != device::mojom::XRSessionFeature::REF_SPACE_VIEWER) {
        return kRequestRequiresUserActivation;
      }
    }

    // If any optional features (besides 'viewer') were requested, reject.
    for (auto feature : query.OptionalFeatures()) {
      if (feature != device::mojom::XRSessionFeature::REF_SPACE_VIEWER) {
        return kRequestRequiresUserActivation;
      }
    }
  }

  // Make sure the WebXR feature policy is enabled
  if (!doc->IsFeatureEnabled(mojom::FeaturePolicyFeature::kWebXr,
                             ReportOptions::kReportOnFailure)) {
    return kFeaturePolicyBlocked;
  }

  return nullptr;
}

XR::PendingSupportsSessionQuery::PendingSupportsSessionQuery(
    ScriptPromiseResolver* resolver,
    XRSession::SessionMode session_mode)
    : resolver_(resolver), mode_(session_mode) {}

void XR::PendingSupportsSessionQuery::Trace(blink::Visitor* visitor) {
  visitor->Trace(resolver_);
}

void XR::PendingSupportsSessionQuery::Resolve() {
  resolver_->Resolve();
}

void XR::PendingSupportsSessionQuery::RejectWithDOMException(
    DOMExceptionCode exception_code,
    const String& message,
    ExceptionState* exception_state) {
  DCHECK_NE(exception_code, DOMExceptionCode::kSecurityError);

  if (exception_state) {
    // The generated bindings will reject the returned promise for us.
    // Detaching the resolver prevents it from thinking we abandoned
    // the promise.
    exception_state->ThrowDOMException(exception_code, message);
    resolver_->Detach();
  } else {
    resolver_->Reject(
        MakeGarbageCollected<DOMException>(exception_code, message));
  }
}

void XR::PendingSupportsSessionQuery::RejectWithSecurityError(
    const String& sanitized_message,
    ExceptionState* exception_state) {
  if (exception_state) {
    // The generated V8 bindings will reject the returned promise for us.
    // Detaching the resolver prevents it from thinking we abandoned
    // the promise.
    exception_state->ThrowSecurityError(sanitized_message);
    resolver_->Detach();
  } else {
    resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSecurityError, sanitized_message));
  }
}

void XR::PendingSupportsSessionQuery::RejectWithTypeError(
    const String& message,
    ExceptionState* exception_state) {
  if (exception_state) {
    // The generated bindings will reject the returned promise for us.
    // Detaching the resolver prevents it from thinking we abandoned
    // the promise.
    exception_state->ThrowTypeError(message);
    resolver_->Detach();
  } else {
    resolver_->Reject(V8ThrowException::CreateTypeError(
        resolver_->GetScriptState()->GetIsolate(), message));
  }
}

XRSession::SessionMode XR::PendingSupportsSessionQuery::mode() const {
  return mode_;
}

XR::PendingRequestSessionQuery::PendingRequestSessionQuery(
    int64_t ukm_source_id,
    ScriptPromiseResolver* resolver,
    XRSession::SessionMode session_mode,
    RequestedXRSessionFeatureSet required_features,
    RequestedXRSessionFeatureSet optional_features)
    : resolver_(resolver),
      mode_(session_mode),
      required_features_(std::move(required_features)),
      optional_features_(std::move(optional_features)),
      ukm_source_id_(ukm_source_id) {}

void XR::PendingRequestSessionQuery::Resolve(XRSession* session) {
  resolver_->Resolve(session);
  ReportRequestSessionResult(SessionRequestStatus::kSuccess);
}

void XR::PendingRequestSessionQuery::RejectWithDOMException(
    DOMExceptionCode exception_code,
    const String& message,
    ExceptionState* exception_state) {
  DCHECK_NE(exception_code, DOMExceptionCode::kSecurityError);

  if (exception_state) {
    exception_state->ThrowDOMException(exception_code, message);
    resolver_->Detach();
  } else {
    resolver_->Reject(
        MakeGarbageCollected<DOMException>(exception_code, message));
  }

  ReportRequestSessionResult(SessionRequestStatus::kOtherError);
}

void XR::PendingRequestSessionQuery::RejectWithSecurityError(
    const String& sanitized_message,
    ExceptionState* exception_state) {
  if (exception_state) {
    exception_state->ThrowSecurityError(sanitized_message);
    resolver_->Detach();
  } else {
    resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSecurityError, sanitized_message));
  }

  ReportRequestSessionResult(SessionRequestStatus::kOtherError);
}

void XR::PendingRequestSessionQuery::RejectWithTypeError(
    const String& message,
    ExceptionState* exception_state) {
  if (exception_state) {
    exception_state->ThrowTypeError(message);
    resolver_->Detach();
  } else {
    resolver_->Reject(V8ThrowException::CreateTypeError(
        GetScriptState()->GetIsolate(), message));
  }

  ReportRequestSessionResult(SessionRequestStatus::kOtherError);
}

void XR::PendingRequestSessionQuery::ReportRequestSessionResult(
    SessionRequestStatus status) {
  LocalFrame* frame = resolver_->GetFrame();
  Document* doc = frame ? frame->GetDocument() : nullptr;
  if (!doc)
    return;

  ukm::builders::XR_WebXR_SessionRequest(ukm_source_id_)
      .SetMode(static_cast<int64_t>(mode_))
      .SetStatus(static_cast<int64_t>(status))
      .Record(doc->UkmRecorder());
}

XRSession::SessionMode XR::PendingRequestSessionQuery::mode() const {
  return mode_;
}

const XRSessionFeatureSet& XR::PendingRequestSessionQuery::RequiredFeatures()
    const {
  return required_features_.valid_features;
}

const XRSessionFeatureSet& XR::PendingRequestSessionQuery::OptionalFeatures()
    const {
  return optional_features_.valid_features;
}

bool XR::PendingRequestSessionQuery::InvalidRequiredFeatures() const {
  return required_features_.invalid_features;
}

bool XR::PendingRequestSessionQuery::InvalidOptionalFeatures() const {
  return optional_features_.invalid_features;
}

ScriptState* XR::PendingRequestSessionQuery::GetScriptState() const {
  return resolver_->GetScriptState();
}

void XR::PendingRequestSessionQuery::Trace(blink::Visitor* visitor) {
  visitor->Trace(resolver_);
}

device::mojom::blink::XRSessionOptionsPtr XR::XRSessionOptionsFromQuery(
    const PendingRequestSessionQuery& query) {
  device::mojom::blink::XRSessionOptionsPtr session_options =
      convertModeToMojo(query.mode());

  CopyToVector(query.RequiredFeatures(), session_options->required_features);
  CopyToVector(query.OptionalFeatures(), session_options->optional_features);

  return session_options;
}

XR::XR(LocalFrame& frame, int64_t ukm_source_id)
    : ContextLifecycleObserver(frame.GetDocument()),
      FocusChangedObserver(frame.GetPage()),
      ukm_source_id_(ukm_source_id),
      navigation_start_(
          frame.Loader().GetDocumentLoader()->GetTiming().NavigationStart()),
      feature_handle_for_scheduler_(frame.GetFrameScheduler()->RegisterFeature(
          SchedulingPolicy::Feature::kWebXR,
          {SchedulingPolicy::RecordMetricsForBackForwardCache()})) {
  // See https://bit.ly/2S0zRAS for task types.
  DCHECK(frame.IsAttached());
  frame.GetInterfaceProvider().GetInterface(mojo::MakeRequest(
      &service_, frame.GetTaskRunner(TaskType::kMiscPlatformAPI)));
  service_.set_connection_error_handler(
      WTF::Bind(&XR::Dispose, WrapWeakPersistent(this)));
}

void XR::FocusedFrameChanged() {
  // Tell all sessions that focus changed.
  for (const auto& session : sessions_) {
    session->OnFocusChanged();
  }

  if (frame_provider_)
    frame_provider_->OnFocusChanged();
}

bool XR::IsFrameFocused() {
  return FocusChangedObserver::IsFrameFocused(GetFrame());
}

ExecutionContext* XR::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

const AtomicString& XR::InterfaceName() const {
  return event_target_names::kXR;
}

XRFrameProvider* XR::frameProvider() {
  if (!frame_provider_) {
    frame_provider_ = MakeGarbageCollected<XRFrameProvider>(this);
  }

  return frame_provider_;
}

bool XR::CanRequestNonImmersiveFrameData() const {
  return !!magic_window_provider_;
}

void XR::GetNonImmersiveFrameData(
    device::mojom::blink::XRFrameDataRequestOptionsPtr options,
    device::mojom::blink::XRFrameDataProvider::GetFrameDataCallback callback) {
  DCHECK(CanRequestNonImmersiveFrameData());
  magic_window_provider_->GetFrameData(std::move(options), std::move(callback));
}

const device::mojom::blink::XREnvironmentIntegrationProviderAssociatedPtr&
XR::xrEnvironmentProviderPtr() {
  return environment_provider_;
}

void XR::AddEnvironmentProviderErrorHandler(
    EnvironmentProviderErrorCallback callback) {
  environment_provider_error_callbacks_.push_back(std::move(callback));
}

void XR::ExitPresent() {
  DCHECK(service_);
  service_->ExitPresent();
}

ScriptPromise XR::supportsSession(ScriptState* script_state,
                                  const String& mode,
                                  ExceptionState& exception_state) {
  LocalFrame* frame = GetFrame();
  Document* doc = frame ? frame->GetDocument() : nullptr;
  if (!doc) {
    // Reject if the frame or document is inaccessible.
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kNavigatorDetachedError);
    return ScriptPromise();  // Will be rejected by generated bindings
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  XRSession::SessionMode session_mode = stringToSessionMode(mode);
  PendingSupportsSessionQuery* query =
      MakeGarbageCollected<PendingSupportsSessionQuery>(resolver, session_mode);

  if (session_mode == XRSession::kModeImmersiveAR &&
      !RuntimeEnabledFeatures::WebXRARModuleEnabled(doc)) {
    query->RejectWithTypeError(
        String::Format(kImmersiveArModeNotValid, "supportsSession"),
        &exception_state);
    return promise;
  }

  if (!doc->IsFeatureEnabled(mojom::FeaturePolicyFeature::kWebXr,
                             ReportOptions::kReportOnFailure)) {
    // Only allow the call to be made if the appropriate feature policy is in
    // place.
    query->RejectWithSecurityError(kFeaturePolicyBlocked, &exception_state);
    return promise;
  }

  if (session_mode == XRSession::kModeInline) {
    // `inline` sessions are always supported if not blocked by feature policy.
    query->Resolve();
  } else {
    if (!service_) {
      // If we don't have a service at the time we reach this call it indicates
      // that there's no WebXR hardware. Reject as not supported.
      query->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                    kSessionNotSupported, &exception_state);
      return promise;
    }

    device::mojom::blink::XRSessionOptionsPtr session_options =
        convertModeToMojo(query->mode());

    outstanding_support_queries_.insert(query);
    service_->SupportsSession(
        std::move(session_options),
        WTF::Bind(&XR::OnSupportsSessionReturned, WrapPersistent(this),
                  WrapPersistent(query)));
  }

  return promise;
}

void XR::RequestImmersiveSession(LocalFrame* frame,
                                 Document* doc,
                                 PendingRequestSessionQuery* query,
                                 ExceptionState* exception_state) {
  // Log an immersive session request if we haven't already
  if (!did_log_request_immersive_session_) {
    ukm::builders::XR_WebXR(GetSourceId())
        .SetDidRequestPresentation(1)
        .Record(doc->UkmRecorder());
    did_log_request_immersive_session_ = true;
  }

  // If its an immersive AR session, make sure that feature is enabled
  if (query->mode() == XRSession::kModeImmersiveAR &&
      !RuntimeEnabledFeatures::WebXRARModuleEnabled(doc)) {
    query->RejectWithTypeError(
        String::Format(kImmersiveArModeNotValid, "requestSession"),
        exception_state);
    return;
  }

  // Make sure the request is allowed
  auto* immersive_session_request_error =
      CheckImmersiveSessionRequestAllowed(frame, doc);
  if (immersive_session_request_error) {
    query->RejectWithSecurityError(immersive_session_request_error,
                                   exception_state);
    return;
  }

  // Ensure there are no other immersive sessions currently pending or active
  if (has_outstanding_immersive_request_ ||
      frameProvider()->immersive_session()) {
    query->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                  kActiveImmersiveSession, exception_state);
    return;
  }

  // If we don't have a service by the time we reach this call, there is no XR
  // hardware.
  if (!service_) {
    query->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                  kNoDevicesMessage, exception_state);
    return;
  }

  // Reject session if any of the required features were invalid.
  if (query->InvalidRequiredFeatures()) {
    query->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                  kSessionNotSupported, exception_state);
    return;
  }

  // Reworded from spec 'pending immersive session'
  has_outstanding_immersive_request_ = true;

  // Submit the request to VrServiceImpl in the Browser process
  outstanding_request_queries_.insert(query);
  auto session_options = XRSessionOptionsFromQuery(*query);
  service_->RequestSession(
      std::move(session_options),
      WTF::Bind(&XR::OnRequestSessionReturned, WrapWeakPersistent(this),
                WrapPersistent(query)));
}

void XR::RequestInlineSession(LocalFrame* frame,
                              Document* doc,
                              PendingRequestSessionQuery* query,
                              ExceptionState* exception_state) {
  // Make sure the inline session request was allowed
  auto* inline_session_request_error =
      CheckInlineSessionRequestAllowed(frame, doc, *query);
  if (inline_session_request_error) {
    query->RejectWithSecurityError(inline_session_request_error,
                                   exception_state);
    return;
  }

  if (!service_) {
    // If we don't have a service by the time we reach this call, there is no XR
    // hardware. Create a sensorless session if possible.
    if (CanCreateSensorlessInlineSession(query)) {
      XRSession* session = CreateSensorlessInlineSession();
      query->Resolve(session);
    } else {
      query->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                    kSessionNotSupported, exception_state);
    }
    return;
  }

  // Reject session if any of the required features were invalid.
  if (query->InvalidRequiredFeatures()) {
    query->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                  kSessionNotSupported, exception_state);
    return;
  }

  // Submit the request to VrServiceImpl in the Browser process
  outstanding_request_queries_.insert(query);
  auto session_options = XRSessionOptionsFromQuery(*query);
  service_->RequestSession(
      std::move(session_options),
      WTF::Bind(&XR::OnRequestSessionReturned, WrapWeakPersistent(this),
                WrapPersistent(query)));
}

ScriptPromise XR::requestSession(ScriptState* script_state,
                                 const String& mode,
                                 XRSessionInit* session_init,
                                 ExceptionState& exception_state) {
  // TODO(https://crbug.com/968622): Make sure we don't forget to call
  // metrics-related methods when the promise gets resolved/rejected.
  LocalFrame* frame = GetFrame();
  Document* doc = frame ? frame->GetDocument() : nullptr;
  if (!doc) {
    // Reject if the frame or doc is inaccessible.

    // Do *not* record an UKM event in this case (we won't be able to access the
    // Document to get UkmRecorder anyway).
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kNavigatorDetachedError);
    return ScriptPromise();  // Will be rejected by generated bindings
  }

  XRSession::SessionMode session_mode = stringToSessionMode(mode);

  // Parse required feature strings
  RequestedXRSessionFeatureSet required_features;
  if (session_init && session_init->hasRequiredFeatures()) {
    required_features.valid_features = ParseRequestedFeatures(
        session_init->requiredFeatures(), session_mode,
        [&](const String& error) {
          GetExecutionContext()->AddConsoleMessage(ConsoleMessage::Create(
              mojom::ConsoleMessageSource::kJavaScript,
              mojom::ConsoleMessageLevel::kError, error));
          required_features.invalid_features = true;
        });
  }

  // Parse optional feature strings
  RequestedXRSessionFeatureSet optional_features;
  if (session_init && session_init->hasOptionalFeatures()) {
    optional_features.valid_features = ParseRequestedFeatures(
        session_init->optionalFeatures(), session_mode,
        [&](const String& error) {
          GetExecutionContext()->AddConsoleMessage(ConsoleMessage::Create(
              mojom::ConsoleMessageSource::kJavaScript,
              mojom::ConsoleMessageLevel::kWarning, error));
          optional_features.invalid_features = true;
        });
  }

  // Certain session modes imply default features.
  // Add those default features as optional features now.
  switch (session_mode) {
    case XRSession::kModeImmersiveVR:
    case XRSession::kModeImmersiveAR:
      optional_features.valid_features.insert(
          device::mojom::XRSessionFeature::REF_SPACE_LOCAL);
      FALLTHROUGH;
    case XRSession::kModeInline:
      optional_features.valid_features.insert(
          device::mojom::XRSessionFeature::REF_SPACE_VIEWER);
      break;
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  PendingRequestSessionQuery* query =
      MakeGarbageCollected<PendingRequestSessionQuery>(
          GetSourceId(), resolver, session_mode, std::move(required_features),
          std::move(optional_features));

  switch (session_mode) {
    case XRSession::kModeImmersiveVR:
    case XRSession::kModeImmersiveAR:
      RequestImmersiveSession(frame, doc, query, &exception_state);
      break;
    case XRSession::kModeInline:
      RequestInlineSession(frame, doc, query, &exception_state);
      break;
  }

  return promise;
}

// This will be called when the XR hardware or capabilities have potentially
// changed. For example, if a new physical device was connected to the system,
// it might be able to support immersive sessions, where it couldn't before.
void XR::OnDeviceChanged() {
  DispatchEvent(*blink::Event::Create(event_type_names::kDevicechange));
}

void XR::OnSupportsSessionReturned(PendingSupportsSessionQuery* query,
                                   bool supports_session) {
  // The session query has returned and we're about to resolve or reject the
  // promise, so remove it from our outstanding list.
  DCHECK(outstanding_support_queries_.Contains(query));
  outstanding_support_queries_.erase(query);

  if (supports_session) {
    query->Resolve();
  } else {
    query->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                  kSessionNotSupported, nullptr);
  }
}

void XR::OnRequestSessionReturned(
    PendingRequestSessionQuery* query,
    device::mojom::blink::RequestSessionResultPtr result) {
  // The session query has returned and we're about to resolve or reject the
  // promise, so remove it from our outstanding list.
  DCHECK(outstanding_request_queries_.Contains(query));
  outstanding_request_queries_.erase(query);
  if (query->mode() == XRSession::kModeImmersiveVR ||
      query->mode() == XRSession::kModeImmersiveAR) {
    DCHECK(has_outstanding_immersive_request_);
    has_outstanding_immersive_request_ = false;
  }

  device::mojom::blink::XRSessionPtr session_ptr =
      result->is_session() ? std::move(result->get_session()) : nullptr;

  // TODO(https://crbug.com/872316) Improve the error messaging to indicate why
  // a request failed.
  if (!session_ptr) {
    // |service_| does not support the requested mode. Attempt to create a
    // sensorless session.
    if (CanCreateSensorlessInlineSession(query)) {
      XRSession* session = CreateSensorlessInlineSession();
      query->Resolve(session);
      return;
    }

    // TODO(http://crbug.com/961960): Report appropriate exception when the user
    // denies XR session request on consent dialog
    // TODO(https://crbug.com/872316): Improve the error messaging to indicate
    // the reason for a request failure.
    query->RejectWithDOMException(DOMExceptionCode::kNotSupportedError,
                                  kSessionNotSupported, nullptr);
    return;
  }

  bool environment_integration = query->mode() == XRSession::kModeImmersiveAR;

  // immersive sessions must supply display info.
  DCHECK(session_ptr->display_info);
  // If the session supports environment integration, ensure the device does
  // as well.
  DCHECK(!environment_integration || session_ptr->display_info->capabilities
                                         ->can_provide_environment_integration);
  DVLOG(2) << __func__
           << ": environment_integration=" << environment_integration
           << "can_provide_environment_integration="
           << session_ptr->display_info->capabilities
                  ->can_provide_environment_integration;

  // TODO(https://crbug.com/944936): The blend mode could be "additive".
  XRSession::EnvironmentBlendMode blend_mode = XRSession::kBlendModeOpaque;
  if (environment_integration)
    blend_mode = XRSession::kBlendModeAlphaBlend;

  XRSessionFeatureSet enabled_features;
  for (const auto& feature : session_ptr->enabled_features) {
    enabled_features.insert(feature);
  }

  XRSession* session = CreateSession(
      query->mode(), blend_mode, std::move(session_ptr->client_request),
      std::move(session_ptr->display_info), session_ptr->uses_input_eventing,
      enabled_features);

  if (query->mode() == XRSession::kModeImmersiveVR ||
      query->mode() == XRSession::kModeImmersiveAR) {
    frameProvider()->BeginImmersiveSession(session, std::move(session_ptr));
    if (environment_integration) {
      // See Task Sources spreadsheet for more information:
      // https://docs.google.com/spreadsheets/d/1b-dus1Ug3A8y0lX0blkmOjJILisUASdj8x9YN_XMwYc/view
      frameProvider()->GetDataProvider()->GetEnvironmentIntegrationProvider(
          mojo::MakeRequest(&environment_provider_,
                            GetExecutionContext()->GetTaskRunner(
                                TaskType::kMiscPlatformAPI)));
      environment_provider_.set_connection_error_handler(WTF::Bind(
          &XR::OnEnvironmentProviderDisconnect, WrapWeakPersistent(this)));
    }

    if (query->mode() == XRSession::kModeImmersiveVR &&
        session->UsesInputEventing()) {
      frameProvider()->GetDataProvider()->SetInputSourceButtonListener(
          session->GetInputClickListener());
    }
  } else {
    magic_window_provider_.Bind(std::move(session_ptr->data_provider));
    magic_window_provider_.set_connection_error_handler(WTF::Bind(
        &XR::OnMagicWindowProviderDisconnect, WrapWeakPersistent(this)));
  }

  UseCounter::Count(ExecutionContext::From(query->GetScriptState()),
                    WebFeature::kWebXrSessionCreated);

  query->Resolve(session);
}

void XR::ReportImmersiveSupported(bool supported) {
  Document* doc = GetFrame() ? GetFrame()->GetDocument() : nullptr;
  if (doc && !did_log_supports_immersive_ && supported) {
    ukm::builders::XR_WebXR ukm_builder(ukm_source_id_);
    ukm_builder.SetReturnedPresentationCapableDevice(1);
    ukm_builder.Record(doc->UkmRecorder());
    did_log_supports_immersive_ = true;
  }
}

void XR::AddedEventListener(const AtomicString& event_type,
                            RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::AddedEventListener(event_type,
                                                registered_listener);

  if (!service_)
    return;

  if (event_type == event_type_names::kDevicechange) {
    // Register for notifications if we haven't already.
    //
    // See https://bit.ly/2S0zRAS for task types.
    auto task_runner =
        GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
    if (!receiver_.is_bound())
      service_->SetClient(receiver_.BindNewPipeAndPassRemote(task_runner));
  }
}

void XR::ContextDestroyed(ExecutionContext*) {
  Dispose();
}

// A session is always created and returned.
XRSession* XR::CreateSession(
    XRSession::SessionMode mode,
    XRSession::EnvironmentBlendMode blend_mode,
    device::mojom::blink::XRSessionClientRequest client_request,
    device::mojom::blink::VRDisplayInfoPtr display_info,
    bool uses_input_eventing,
    XRSessionFeatureSet enabled_features,
    bool sensorless_session) {
  XRSession* session = MakeGarbageCollected<XRSession>(
      this, client_request ? std::move(client_request) : nullptr, mode,
      blend_mode, uses_input_eventing, sensorless_session,
      std::move(enabled_features));
  if (display_info)
    session->SetXRDisplayInfo(std::move(display_info));
  sessions_.insert(session);
  return session;
}

bool XR::CanCreateSensorlessInlineSession(
    const PendingRequestSessionQuery* query) const {
  // Sensorless can only support an inline mode
  if (query->mode() != XRSession::kModeInline)
    return false;

  // Sensorless can only be supported if the only required feature is the
  // viewer reference space.
  for (const auto& feature : query->RequiredFeatures()) {
    if (feature != device::mojom::XRSessionFeature::REF_SPACE_VIEWER) {
      return false;
    }
  }

  return true;
}

XRSession* XR::CreateSensorlessInlineSession() {
  // TODO(https://crbug.com/944936): The blend mode could be "additive".
  XRSession::EnvironmentBlendMode blend_mode = XRSession::kBlendModeOpaque;
  return CreateSession(XRSession::kModeInline, blend_mode,
                       nullptr /* client request */, nullptr /* display_info */,
                       false /* uses_input_eventing */,
                       {device::mojom::XRSessionFeature::REF_SPACE_VIEWER},
                       true /* sensorless_session */);
}

void XR::Dispose() {
  // If the document context was destroyed, shut down the client connection
  // and never call the mojo service again.
  service_.reset();
  receiver_.reset();

  // Shutdown frame provider, which manages the message pipes.
  if (frame_provider_)
    frame_provider_->Dispose();

  HeapHashSet<Member<PendingSupportsSessionQuery>> support_queries =
      outstanding_support_queries_;
  for (const auto& query : support_queries) {
    OnSupportsSessionReturned(query, false);
  }
  DCHECK(outstanding_support_queries_.IsEmpty());

  HeapHashSet<Member<PendingRequestSessionQuery>> request_queries =
      outstanding_request_queries_;
  for (const auto& query : request_queries) {
    // TODO(https://crbug.com/962991): The spec should specify
    // what is returned here.
    OnRequestSessionReturned(
        query, device::mojom::blink::RequestSessionResult::NewFailureReason(
                   device::mojom::RequestSessionError::INVALID_CLIENT));
  }
  DCHECK(outstanding_support_queries_.IsEmpty());
}

void XR::OnEnvironmentProviderDisconnect() {
  for (auto& callback : environment_provider_error_callbacks_) {
    std::move(callback).Run();
  }

  environment_provider_error_callbacks_.clear();
  environment_provider_.reset();
}

// Ends all non-immersive sessions when the magic window provider got
// disconnected.
void XR::OnMagicWindowProviderDisconnect() {
  for (auto& session : sessions_) {
    if (!session->immersive() && !session->ended()) {
      session->ForceEnd();
    }
  }
  magic_window_provider_.reset();
}

void XR::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_provider_);
  visitor->Trace(sessions_);
  visitor->Trace(outstanding_support_queries_);
  visitor->Trace(outstanding_request_queries_);
  ContextLifecycleObserver::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
