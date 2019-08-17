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
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
namespace blink {

namespace {

const char kNavigatorDetachedError[] =
    "The navigator.xr object is no longer associated with a document.";

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

// When updating this list, also update XRRuntimeManager's
// AreArFeaturesEnabled() until https://crbug.com/966647 is fixed.
// TODO(https://crbug.com/966647) remove the above comment when fixed.
bool AreArRuntimeFeaturesEnabled(const FeatureContext* context) {
  return RuntimeEnabledFeatures::WebXRHitTestEnabled(context) ||
         RuntimeEnabledFeatures::WebXRPlaneDetectionEnabled(context);
}

// Map device::mojom::blink::RequestSessionError to a DOMException.
blink::DOMException* MapToDOMException(
    device::mojom::blink::RequestSessionError error) {
  // TODO(http://crbug.com/961960): Report appropriate exception when the user
  // denies XR session request on consent dialog
  // TODO(https://crbug.com/872316): Improve the error messaging to indicate
  // the reason for a request failure.
  switch (error) {
    case device::mojom::blink::RequestSessionError::ORIGIN_NOT_SECURE:
    case device::mojom::blink::RequestSessionError::
        IMMERSIVE_SESSION_REQUEST_FROM_OFF_FOCUS_PAGE:
    case device::mojom::blink::RequestSessionError::EXISTING_IMMERSIVE_SESSION:
    case device::mojom::blink::RequestSessionError::INVALID_CLIENT:
    case device::mojom::blink::RequestSessionError::NO_RUNTIME_FOUND:
    case device::mojom::blink::RequestSessionError::UNKNOWN_RUNTIME_ERROR:
    case device::mojom::blink::RequestSessionError::USER_DENIED_CONSENT:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, kSessionNotSupported);
  }

  NOTREACHED();
  return nullptr;
}

// Converts the given string to an XRSessionFeature. If the string is
// unrecognized, returns nullopt. Based on the spec:
// https://immersive-web.github.io/webxr/#feature-name
base::Optional<XRSessionFeature> StringToXRSessionFeature(
    WTF::StringView feature_string) {
  if (feature_string == "viewer") {
    return XRSessionFeature::kViewer;
  } else if (feature_string == "local") {
    return XRSessionFeature::kLocal;
  } else if (feature_string == "local-floor") {
    return XRSessionFeature::kLocalFloor;
  } else if (feature_string == "bounded-floor") {
    return XRSessionFeature::kBoundedFloor;
  } else if (feature_string == "unbounded") {
    return XRSessionFeature::kUnbounded;
  }

  return base::nullopt;
}

}  // namespace

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

void XR::PendingSupportsSessionQuery::Reject(DOMException* exception) {
  resolver_->Reject(exception);
}

void XR::PendingSupportsSessionQuery::Reject(v8::Local<v8::Value> value) {
  resolver_->Reject(value);
}

XRSession::SessionMode XR::PendingSupportsSessionQuery::mode() const {
  return mode_;
}

XR::PendingRequestSessionQuery::PendingRequestSessionQuery(
    int64_t ukm_source_id,
    ScriptPromiseResolver* resolver,
    XRSession::SessionMode session_mode,
    XRSessionFeatureSet required_features,
    XRSessionFeatureSet optional_features)
    : resolver_(resolver),
      mode_(session_mode),
      required_features_(std::move(required_features)),
      optional_features_(std::move(optional_features)),
      ukm_source_id_(ukm_source_id) {}

void XR::PendingRequestSessionQuery::Resolve(XRSession* session) {
  ReportRequestSessionResult(SessionRequestStatus::kSuccess);
  resolver_->Resolve(session);
}

void XR::PendingRequestSessionQuery::Reject(DOMException* exception) {
  ReportRequestSessionResult(SessionRequestStatus::kOtherError);
  resolver_->Reject(exception);
}

void XR::PendingRequestSessionQuery::Reject(v8::Local<v8::Value> value) {
  ReportRequestSessionResult(SessionRequestStatus::kOtherError);
  resolver_->Reject(value);
}

XRSession::SessionMode XR::PendingRequestSessionQuery::mode() const {
  return mode_;
}

XRSessionFeatureSet const& XR::PendingRequestSessionQuery::RequiredFeatures()
    const {
  return required_features_;
}

XRSessionFeatureSet const& XR::PendingRequestSessionQuery::OptionalFeatures()
    const {
  return optional_features_;
}

ScriptState* XR::PendingRequestSessionQuery::GetScriptState() const {
  return resolver_->GetScriptState();
}

void XR::PendingRequestSessionQuery::Trace(blink::Visitor* visitor) {
  visitor->Trace(resolver_);
}

void XR::PendingRequestSessionQuery::ReportRequestSessionResult(
    SessionRequestStatus status) {
  Document* doc =
      resolver_->GetFrame() ? resolver_->GetFrame()->GetDocument() : nullptr;
  if (!doc)
    return;

  ukm::builders::XR_WebXR_SessionRequest(ukm_source_id_)
      .SetMode(static_cast<int64_t>(mode_))
      .SetStatus(static_cast<int64_t>(status))
      .Record(doc->UkmRecorder());
}

XR::XR(LocalFrame& frame, int64_t ukm_source_id)
    : ContextLifecycleObserver(frame.GetDocument()),
      FocusChangedObserver(frame.GetPage()),
      ukm_source_id_(ukm_source_id),
      binding_(this),
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
                                  const String& mode) {
  LocalFrame* frame = GetFrame();
  if (!frame || !frame->GetDocument()) {
    // Reject if the frame is inaccessible.
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           kNavigatorDetachedError));
  }

  Document* doc = frame->GetDocument();
  XRSession::SessionMode session_mode = stringToSessionMode(mode);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  PendingSupportsSessionQuery* query =
      MakeGarbageCollected<PendingSupportsSessionQuery>(resolver, session_mode);

  if (session_mode == XRSession::kModeImmersiveAR &&
      !AreArRuntimeFeaturesEnabled(doc)) {
    query->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(),
        String::Format(kImmersiveArModeNotValid, __func__)));
    return promise;
  }

  if (!doc->IsFeatureEnabled(mojom::FeaturePolicyFeature::kWebVr,
                             ReportOptions::kReportOnFailure)) {
    // Only allow the call to be made if the appropriate feature policy is in
    // place.
    query->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSecurityError, kFeaturePolicyBlocked));
    return promise;
  }

  if (session_mode == XRSession::kModeInline) {
    // `inline` sessions are always supported if not blocked by feature policy.
    query->Resolve();
  } else {
    if (!service_) {
      // If we don't have a service at the time we reach this call it indicates
      // that there's no WebXR hardware. Reject as not supported.
      query->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, kSessionNotSupported));
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

ScriptPromise XR::requestSession(ScriptState* script_state,
                                 const String& mode,
                                 XRSessionInit* session_init) {
  // TODO(https://crbug.com/968622): Make sure we don't forget to call
  // metrics-related methods when the promise gets resolved/rejected.

  LocalFrame* frame = GetFrame();
  Document* doc = frame ? frame->GetDocument() : nullptr;
  if (!doc) {
    // Reject if the frame or doc is inaccessible.

    // Do *not* record an UKM event in this case (we won't be able to access the
    // Document to get UkmRecorder anyway).

    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           kNavigatorDetachedError));
  }

  XRSession::SessionMode session_mode = stringToSessionMode(mode);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // Parse required feature strings
  bool has_invalid_required_feature = false;
  XRSessionFeatureSet required_features;
  if (session_init && session_init->hasRequiredFeatures()) {
    for (const auto& feature : session_init->requiredFeatures()) {
      // Add all features so that we can track the presence of an unknown
      // required feature later, to reject it at the appropriate time.
      auto feature_enum = StringToXRSessionFeature(feature);
      if (!feature_enum.has_value()) {
        GetExecutionContext()->AddConsoleMessage(ConsoleMessage::Create(
            mojom::ConsoleMessageSource::kJavaScript,
            mojom::ConsoleMessageLevel::kError,
            "Unrecognized required feature requested: " + feature));
        has_invalid_required_feature = true;
      } else {
        required_features.insert(feature_enum.value());
      }
    }
  }

  // Parse optional feature strings
  XRSessionFeatureSet optional_features;
  if (session_init && session_init->hasOptionalFeatures()) {
    for (const auto& feature : session_init->optionalFeatures()) {
      auto feature_enum = StringToXRSessionFeature(feature);

      // Unrecognized optional features can be silently ignored, as they won't
      // be supported.
      if (!feature_enum.has_value()) {
        GetExecutionContext()->AddConsoleMessage(ConsoleMessage::Create(
            mojom::ConsoleMessageSource::kJavaScript,
            mojom::ConsoleMessageLevel::kWarning,
            "Unrecognized optional feature requested: " + feature));
      } else {
        optional_features.insert(feature_enum.value());
      }
    }
  }

  PendingRequestSessionQuery* query =
      MakeGarbageCollected<PendingRequestSessionQuery>(
          GetSourceId(), resolver, session_mode, std::move(required_features),
          std::move(optional_features));

  if (session_mode == XRSession::kModeImmersiveAR &&
      !AreArRuntimeFeaturesEnabled(doc)) {
    query->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(),
        String::Format(kImmersiveArModeNotValid, __func__)));

    return promise;
  }

  bool is_immersive = session_mode == XRSession::kModeImmersiveVR ||
                      session_mode == XRSession::kModeImmersiveAR;

  if (is_immersive && !did_log_request_immersive_session_) {
    ukm::builders::XR_WebXR(GetSourceId())
        .SetDidRequestPresentation(1)
        .Record(doc->UkmRecorder());
    did_log_request_immersive_session_ = true;
  }

  if (!doc->IsFeatureEnabled(mojom::FeaturePolicyFeature::kWebVr,
                             ReportOptions::kReportOnFailure)) {
    // Only allow the call to be made if the appropriate feature policy is in
    // place.
    query->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSecurityError, kFeaturePolicyBlocked));
    return promise;
  }

  // Only one immersive session can be active at a time.
  if (is_immersive && frameProvider()->immersive_session()) {
    query->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kActiveImmersiveSession));
    return promise;
  }

  // All immersive sessions require a user gesture.
  bool has_user_activation = LocalFrame::HasTransientUserActivation(frame);
  if (is_immersive && !has_user_activation) {
    query->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSecurityError, kRequestRequiresUserActivation));
    return promise;
  }

  // TODO(https://crbug.com/962991): The error handling here is not spec
  // compliant. The spec says to reject based on no device before rejecting
  // based on user gesture.
  // If we no longer have a valid service connection reject the request, unless
  // it was for an inline mode.  In which case, we'll end up creating the
  // session in OnRequestSessionReturned.
  if (!service_ && session_mode != XRSession::kModeInline) {
    query->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError, kNoDevicesMessage));
    return promise;
  }

  if (!service_) {
    // If we don't have a service by the time we reach this call, there is no XR
    // hardware. Create a sensorless session.
    if (query->mode() == XRSession::kModeInline) {
      XRSession* session = CreateSensorlessInlineSession();
      query->Resolve(session);
      return promise;
    }

    // TODO(https://crbug.com/962991): The spec says to reject with null.
    // Clarify/fix the spec. In other places where we have no device or no
    // service we return kNotSupportedError.
    query->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError, kSessionNotSupported));
    return promise;
  }

  // Reject session if any of the required features were invalid.
  if (has_invalid_required_feature) {
    query->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError, kSessionNotSupported));
    return promise;
  }

  device::mojom::blink::XRSessionOptionsPtr session_options =
      convertModeToMojo(query->mode());

  outstanding_request_queries_.insert(query);
  service_->RequestSession(
      std::move(session_options),
      WTF::Bind(&XR::OnRequestSessionReturned, WrapWeakPersistent(this),
                WrapPersistent(query)));

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
    query->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotSupportedError, kSessionNotSupported));
  }
}

void XR::OnRequestSessionReturned(
    PendingRequestSessionQuery* query,
    device::mojom::blink::RequestSessionResultPtr result) {
  // The session query has returned and we're about to resolve or reject the
  // promise, so remove it from our outstanding list.
  DCHECK(outstanding_request_queries_.Contains(query));
  outstanding_request_queries_.erase(query);

  device::mojom::blink::XRSessionPtr session_ptr =
      result->is_session() ? std::move(result->get_session()) : nullptr;

  // TODO(https://crbug.com/872316) Improve the error messaging to indicate why
  // a request failed.
  if (!session_ptr) {
    // |service_| does not support the requested mode. Attempt to create a
    // sensorless session.
    if (query->mode() == XRSession::kModeInline) {
      XRSession* session = CreateSensorlessInlineSession();

      query->Resolve(session);
      return;
    }

    query->Reject(MapToDOMException(result->get_failure_reason()));
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

  // TODO(crbug.com/991605): enabled_features should be coming back on something
  // in the session request result.  Which includes populating any granted
  // implied optional features.
  XRSessionFeatureSet enabled_features = query->RequiredFeatures();
  for (const auto& feature : query->OptionalFeatures()) {
    enabled_features.insert(feature);
  }

  // Add any implied optional features per the spec.
  switch (query->mode()) {
    case XRSession::kModeImmersiveVR:
    case XRSession::kModeImmersiveAR:
      enabled_features.insert(XRSessionFeature::kLocal);
      FALLTHROUGH;
    case XRSession::kModeInline:
      enabled_features.insert(XRSessionFeature::kViewer);
      break;
    default:
      break;
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
    if (!binding_.is_bound()) {
      device::mojom::blink::VRServiceClientPtr client;
      binding_.Bind(mojo::MakeRequest(&client, task_runner), task_runner);
      service_->SetClient(std::move(client));
    }
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

XRSession* XR::CreateSensorlessInlineSession() {
  // TODO(https://crbug.com/944936): The blend mode could be "additive".
  XRSession::EnvironmentBlendMode blend_mode = XRSession::kBlendModeOpaque;
  return CreateSession(
      XRSession::kModeInline, blend_mode, nullptr /* client request */,
      nullptr /* display_info */, false /* uses_input_eventing */,
      {XRSessionFeature::kViewer}, true /* sensorless_session */);
}

void XR::Dispose() {
  // If the document context was destroyed, shut down the client connection
  // and never call the mojo service again.
  service_.reset();
  binding_.Close();

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
        query,
        device::mojom::blink::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::EXISTING_IMMERSIVE_SESSION));
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
