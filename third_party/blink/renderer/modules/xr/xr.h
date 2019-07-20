// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_H_

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/page/focus_changed_observer.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_session_init.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ScriptPromiseResolver;
class XRFrameProvider;
class XRSessionInit;

class XR final : public EventTargetWithInlineData,
                 public ContextLifecycleObserver,
                 public device::mojom::blink::VRServiceClient,
                 public FocusChangedObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(XR);

 public:
  // TODO(crbug.com/976796): Fix lint errors.
  static XR* Create(LocalFrame& frame, int64_t source_id) {
    return MakeGarbageCollected<XR>(frame, source_id);
  }

  // TODO(crbug.com/976796): Fix lint errors.
  XR(LocalFrame& frame, int64_t ukm_source_id);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(devicechange, kDevicechange)

  ScriptPromise supportsSession(ScriptState*, const String&);
  ScriptPromise requestSession(ScriptState*, const String&, XRSessionInit*);

  XRFrameProvider* frameProvider();

  bool CanRequestNonImmersiveFrameData() const;
  void GetNonImmersiveFrameData(
      device::mojom::blink::XRFrameDataRequestOptionsPtr,
      device::mojom::blink::XRFrameDataProvider::GetFrameDataCallback);

  const device::mojom::blink::XREnvironmentIntegrationProviderAssociatedPtr&
  xrEnvironmentProviderPtr();

  // VRServiceClient overrides.
  void OnDeviceChanged() override;

  // EventTarget overrides.
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  // ContextLifecycleObserver overrides.
  void ContextDestroyed(ExecutionContext*) override;
  void Trace(blink::Visitor*) override;

  // FocusChangedObserver overrides.
  void FocusedFrameChanged() override;
  bool IsFrameFocused();

  int64_t GetSourceId() const { return ukm_source_id_; }

  using EnvironmentProviderErrorCallback = base::OnceCallback<void()>;
  void AddEnvironmentProviderErrorHandler(
      EnvironmentProviderErrorCallback callback);

  void ExitPresent();

  base::TimeTicks NavigationStart() const { return navigation_start_; }

 private:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class SessionRequestStatus : int {
    // `requestSession` succeeded.
    kSuccess = 0,
    // `requestSession` failed with other (unknown) error.
    kOtherError = 1,
    kMaxValue = kOtherError,
  };

  // Encapsulates blink-side `XR::requestSession()` call. It is a wrapper around
  // ScriptPromiseResolver that allows us to add additional logic as certain
  // things related to promise's life cycle happen.
  class PendingRequestSessionQuery final
      : public GarbageCollected<PendingRequestSessionQuery> {
   public:
    PendingRequestSessionQuery(int64_t ukm_source_id,
                               ScriptPromiseResolver* resolver,
                               XRSession::SessionMode mode,
                               XRSessionInit*);
    virtual ~PendingRequestSessionQuery() = default;

    // Resolves underlying promise with passed in XR session.
    void Resolve(XRSession* session);
    // Rejects underlying promise with passed in DOM exception.
    void Reject(DOMException* exception);
    // Rejects underlying promise with passed in v8 value. Used to raise
    // TypeError which is not a DOM exception.
    void Reject(v8::Local<v8::Value> value);

    XRSession::SessionMode mode() const;
    const XRSessionInit* SessionInit() const;

    // Returns underlying resolver's script state.
    ScriptState* GetScriptState() const;

    virtual void Trace(blink::Visitor*);

   private:
    void ReportRequestSessionResult(SessionRequestStatus status);

    Member<ScriptPromiseResolver> resolver_;
    const XRSession::SessionMode mode_;
    Member<XRSessionInit> session_init_;

    const int64_t ukm_source_id_;

    DISALLOW_COPY_AND_ASSIGN(PendingRequestSessionQuery);
  };

  // Encapsulates blink-side `XR::supportsSession()` call.  It is a wrapper
  // around ScriptPromiseResolver that allows us to add additional logic as
  // certain things related to promise's life cycle happen.
  class PendingSupportsSessionQuery final
      : public GarbageCollected<PendingSupportsSessionQuery> {
   public:
    PendingSupportsSessionQuery(ScriptPromiseResolver*, XRSession::SessionMode);
    virtual ~PendingSupportsSessionQuery() = default;

    // Resolves underlying promise.
    void Resolve();
    // Rejects underlying promise with passed in DOM exception.
    void Reject(DOMException* exception);
    // Rejects underlying promise with passed in v8 value. Used to raise
    // TypeError which is not a DOM exception.
    void Reject(v8::Local<v8::Value> value);

    XRSession::SessionMode mode() const;

    virtual void Trace(blink::Visitor*);

   private:
    Member<ScriptPromiseResolver> resolver_;
    const XRSession::SessionMode mode_;

    DISALLOW_COPY_AND_ASSIGN(PendingSupportsSessionQuery);
  };

  void OnRequestDeviceReturned(device::mojom::blink::XRDevicePtr device);
  void DispatchPendingSessionCalls();

  void DispatchRequestSession(PendingRequestSessionQuery*);
  void OnRequestSessionReturned(
      PendingRequestSessionQuery*,
      device::mojom::blink::RequestSessionResultPtr result);

  void DispatchSupportsSession(PendingSupportsSessionQuery*);
  void OnSupportsSessionReturned(PendingSupportsSessionQuery*,
                                 bool supports_session);

  void EnsureDevice();
  void ReportImmersiveSupported(bool supported);

  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) override;

  XRSession* CreateSession(
      XRSession::SessionMode mode,
      XRSession::EnvironmentBlendMode blend_mode,
      device::mojom::blink::XRSessionClientRequest client_request,
      device::mojom::blink::VRDisplayInfoPtr display_info,
      bool uses_input_eventing,
      bool sensorless_session = false);
  XRSession* CreateSensorlessInlineSession();

  void Dispose();

  void OnDeviceDisconnect();
  void OnEnvironmentProviderDisconnect();
  void OnMagicWindowProviderDisconnect();

  // Reports that session request has returned.
  void ReportRequestSessionResult(XRSession::SessionMode session_mode,
                                  SessionRequestStatus status);

  bool pending_device_ = false;

  // Indicates whether use of requestDevice has already been logged.
  bool did_log_supports_immersive_ = false;

  // Indicates whether we've already logged a request for an immersive session.
  bool did_log_request_immersive_session_ = false;

  const int64_t ukm_source_id_;

  // Track calls that were made prior to the internal device successfully being
  // queried. Can be removed once the service has been updated to allow the
  // respective calls to be made directly.
  HeapVector<Member<PendingSupportsSessionQuery>> pending_mode_queries_;
  HeapVector<Member<PendingRequestSessionQuery>> pending_session_requests_;

  HeapHashSet<Member<PendingSupportsSessionQuery>> outstanding_support_queries_;
  HeapHashSet<Member<PendingRequestSessionQuery>> outstanding_request_queries_;

  Vector<EnvironmentProviderErrorCallback>
      environment_provider_error_callbacks_;

  Member<XRFrameProvider> frame_provider_;
  HeapHashSet<WeakMember<XRSession>> sessions_;
  device::mojom::blink::VRServicePtr service_;
  device::mojom::blink::XRDevicePtr device_;
  device::mojom::blink::XRFrameDataProviderPtr magic_window_provider_;
  device::mojom::blink::XREnvironmentIntegrationProviderAssociatedPtr
      environment_provider_;
  mojo::Binding<device::mojom::blink::VRServiceClient> binding_;

  // Time at which navigation started. Used as the base for relative timestamps,
  // such as for Gamepad objects.
  base::TimeTicks navigation_start_;

  FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle
      feature_handle_for_scheduler_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_H_
