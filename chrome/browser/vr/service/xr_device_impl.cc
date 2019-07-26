// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/service/xr_device_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/common/trace_event_common.h"
#include "build/build_config.h"
#include "chrome/browser/vr/metrics/session_metrics_helper.h"
#include "chrome/browser/vr/mode.h"
#include "chrome/browser/vr/service/browser_xr_runtime.h"
#include "chrome/browser/vr/service/xr_runtime_manager.h"
#include "chrome/common/chrome_switches.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "device/vr/buildflags/buildflags.h"

#if defined(OS_WIN)
#include "chrome/browser/vr/service/xr_session_request_consent_manager.h"
#elif defined(OS_ANDROID)
#include "chrome/browser/vr/service/gvr_consent_helper.h"
#if BUILDFLAG(ENABLE_ARCORE)
#include "chrome/browser/vr/service/arcore_consent_prompt_interface.h"
#endif
#endif

namespace vr {

namespace {

// TODO(mthiesse): When we unship WebVR 1.1, set this to false.
static constexpr bool kAllowHTTPWebVRWithFlag = true;

bool IsSecureContext(content::RenderFrameHost* host) {
  if (!host)
    return false;
  while (host != nullptr) {
    if (!content::IsOriginSecure(host->GetLastCommittedURL()))
      return false;
    host = host->GetParent();
  }
  return true;
}

device::mojom::XRRuntimeSessionOptionsPtr GetRuntimeOptions(
    device::mojom::XRSessionOptions* options) {
  device::mojom::XRRuntimeSessionOptionsPtr runtime_options =
      device::mojom::XRRuntimeSessionOptions::New();
  runtime_options->immersive = options->immersive;
  runtime_options->environment_integration = options->environment_integration;
  runtime_options->is_legacy_webvr = options->is_legacy_webvr;
  return runtime_options;
}

}  // namespace

// static
bool XRDeviceImpl::IsXrDeviceConsentPromptDisabledForTesting() {
  static bool is_xr_device_consent_prompt_disabled_for_testing =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableXrDeviceConsentPromptForTesting);
  return is_xr_device_consent_prompt_disabled_for_testing;
}

XRDeviceImpl::XRDeviceImpl(content::RenderFrameHost* render_frame_host,
                           device::mojom::XRDeviceRequest request,
                           scoped_refptr<XRRuntimeManager> runtime_manager)
    :  // TODO(https://crbug.com/846392): render_frame_host can be null because
       // of a test, not because a XRDeviceImpl can be created without it.
      in_focused_frame_(
          render_frame_host ? render_frame_host->GetView()->HasFocus() : false),
      runtime_manager_(std::move(runtime_manager)),
      render_frame_host_(render_frame_host),
      binding_(this) {
  binding_.Bind(std::move(request));
  magic_window_controllers_.set_connection_error_handler(base::BindRepeating(
      &XRDeviceImpl::OnInlineSessionDisconnected,
      base::Unretained(this)));  // Unretained is OK since the collection is
                                 // owned by XRDeviceImpl.
}

void XRDeviceImpl::OnInlineSessionCreated(
    device::mojom::XRDeviceId session_runtime_id,
    device::mojom::XRDevice::RequestSessionCallback callback,
    device::mojom::XRSessionPtr session,
    device::mojom::XRSessionControllerPtr controller) {
  if (!session) {
    std::move(callback).Run(
        device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::UNKNOWN_RUNTIME_ERROR));
    return;
  }

  // Start giving out magic window data if we are focused.
  controller->SetFrameDataRestricted(!in_focused_frame_);

  auto id = magic_window_controllers_.AddPtr(std::move(controller));

  // Note: We might be recording an inline session that was created by WebVR.
  GetSessionMetricsHelper()->RecordInlineSessionStart(id);

  OnSessionCreated(session_runtime_id, std::move(callback), std::move(session));
}

void XRDeviceImpl::OnInlineSessionDisconnected(size_t session_id) {
  // Notify metrics helper that inline session was stopped.
  auto* metrics_helper = GetSessionMetricsHelper();
  metrics_helper->RecordInlineSessionStop(session_id);
}

SessionMetricsHelper* XRDeviceImpl::GetSessionMetricsHelper() {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  SessionMetricsHelper* metrics_helper =
      SessionMetricsHelper::FromWebContents(web_contents);
  if (!metrics_helper) {
    // This will only happen if we are not already in VR; set start params
    // accordingly.
    metrics_helper =
        SessionMetricsHelper::CreateForWebContents(web_contents, Mode::kNoVr);
  }

  return metrics_helper;
}

void XRDeviceImpl::OnSessionCreated(
    device::mojom::XRDeviceId session_runtime_id,
    device::mojom::XRDevice::RequestSessionCallback callback,
    device::mojom::XRSessionPtr session) {
  if (!session) {
    std::move(callback).Run(
        device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::UNKNOWN_RUNTIME_ERROR));
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("XR.RuntimeUsed", session_runtime_id);

  device::mojom::XRSessionClientPtr client;
  session->client_request = mojo::MakeRequest(&client);

  session_clients_.AddPtr(std::move(client));

  std::move(callback).Run(
      device::mojom::RequestSessionResult::NewSession(std::move(session)));
}

XRDeviceImpl::~XRDeviceImpl() {
  runtime_manager_->OnRendererDeviceRemoved(this);
}

void XRDeviceImpl::RequestSession(
    device::mojom::XRSessionOptionsPtr options,
    device::mojom::XRDevice::RequestSessionCallback callback) {
  DCHECK(options);

  // Check that the request satisifies secure context requirements.
  if (!IsSecureContextRequirementSatisfied()) {
    std::move(callback).Run(
        device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::ORIGIN_NOT_SECURE));
    return;
  }

  // Check that the request is coming from a focused page if required.
  if (!in_focused_frame_ && options->immersive) {
    std::move(callback).Run(
        device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::
                IMMERSIVE_SESSION_REQUEST_FROM_OFF_FOCUS_PAGE));
    return;
  }

  if (runtime_manager_->IsOtherDevicePresenting(this)) {
    // Can't create sessions while an immersive session exists.
    std::move(callback).Run(
        device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::EXISTING_IMMERSIVE_SESSION));
    return;
  }

  // Inline sessions do not need permissions. WebVR did not require
  // permissions.
  // TODO(crbug.com/968221): Address privacy requirements for inline sessions
  if (!options->immersive || options->is_legacy_webvr) {
    DoRequestSession(std::move(options), std::move(callback));
    return;
  }

  // TODO(crbug.com/968233): Unify the below consent flow.
#if defined(OS_ANDROID)

  if (options->environment_integration) {
#if BUILDFLAG(ENABLE_ARCORE)
    if (!render_frame_host_) {
      // Reject promise.
      std::move(callback).Run(
          device::mojom::RequestSessionResult::NewFailureReason(
              device::mojom::RequestSessionError::INVALID_CLIENT));
    } else {
      if (IsXrDeviceConsentPromptDisabledForTesting()) {
        DoRequestSession(std::move(options), std::move(callback));
      } else {
        ArCoreConsentPromptInterface::GetInstance()->ShowConsentPrompt(
            render_frame_host_->GetProcess()->GetID(),
            render_frame_host_->GetRoutingID(),
            base::BindOnce(&XRDeviceImpl::OnConsentResult,
                           weak_ptr_factory_.GetWeakPtr(), std::move(options),
                           std::move(callback)));
      }
    }
#endif
    return;
  } else {
    // GVR.
    if (!render_frame_host_) {
      // Reject promise.
      std::move(callback).Run(
          device::mojom::RequestSessionResult::NewFailureReason(
              device::mojom::RequestSessionError::INVALID_CLIENT));
    } else {
      if (IsXrDeviceConsentPromptDisabledForTesting()) {
        DoRequestSession(std::move(options), std::move(callback));
      } else {
        GvrConsentHelper::GetInstance()->PromptUserAndGetConsent(
            render_frame_host_->GetProcess()->GetID(),
            render_frame_host_->GetRoutingID(),
            base::BindOnce(&XRDeviceImpl::OnConsentResult,
                           weak_ptr_factory_.GetWeakPtr(), std::move(options),
                           std::move(callback)));
      }
    }
    return;
  }

#elif defined(OS_WIN)

  DCHECK(!options->environment_integration);
  if (IsXrDeviceConsentPromptDisabledForTesting()) {
    DoRequestSession(std::move(options), std::move(callback));
  } else {
    XRSessionRequestConsentManager::Instance()->ShowDialogAndGetConsent(
        GetWebContents(),
        base::BindOnce(&XRDeviceImpl::OnConsentResult,
                       weak_ptr_factory_.GetWeakPtr(), std::move(options),
                       std::move(callback)));
  }
  return;

#else

  NOTREACHED();

#endif
}

void XRDeviceImpl::OnConsentResult(
    device::mojom::XRSessionOptionsPtr options,
    device::mojom::XRDevice::RequestSessionCallback callback,
    bool is_consent_granted) {
  if (!is_consent_granted) {
    std::move(callback).Run(
        device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::USER_DENIED_CONSENT));
    return;
  }

  // Re-check for another device instance after a potential user consent.
  // TODO(crbug.com/967513): prevent such races.
  if (runtime_manager_->IsOtherDevicePresenting(this)) {
    // Can't create sessions while an immersive session exists.
    std::move(callback).Run(
        device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::EXISTING_IMMERSIVE_SESSION));
    return;
  }

  DoRequestSession(std::move(options), std::move(callback));
}

void XRDeviceImpl::DoRequestSession(
    device::mojom::XRSessionOptionsPtr options,
    device::mojom::XRDevice::RequestSessionCallback callback) {
  // Get the runtime we'll be creating a session with.
  BrowserXRRuntime* runtime =
      runtime_manager_->GetRuntimeForOptions(options.get());
  if (!runtime) {
    std::move(callback).Run(
        device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::NO_RUNTIME_FOUND));
    return;
  }

  device::mojom::XRDeviceId session_runtime_id = runtime->GetId();
  TRACE_EVENT_INSTANT1("xr", "GetRuntimeForOptions", TRACE_EVENT_SCOPE_THREAD,
                       "id", session_runtime_id);

  auto runtime_options = GetRuntimeOptions(options.get());

#if defined(OS_ANDROID) && BUILDFLAG(ENABLE_ARCORE)
  if (session_runtime_id == device::mojom::XRDeviceId::ARCORE_DEVICE_ID) {
    if (!render_frame_host_) {
      std::move(callback).Run(
          device::mojom::RequestSessionResult::NewFailureReason(
              device::mojom::RequestSessionError::INVALID_CLIENT));
      return;
    }
    runtime_options->render_process_id =
        render_frame_host_->GetProcess()->GetID();
    runtime_options->render_frame_id = render_frame_host_->GetRoutingID();
  }
#endif

  if (runtime_options->immersive) {
    GetSessionMetricsHelper()->ReportRequestPresent(*runtime_options);

    base::OnceCallback<void(device::mojom::XRSessionPtr)> immersive_callback =
        base::BindOnce(&XRDeviceImpl::OnSessionCreated,
                       weak_ptr_factory_.GetWeakPtr(), session_runtime_id,
                       std::move(callback));
    runtime->RequestSession(this, std::move(runtime_options),
                            std::move(immersive_callback));
  } else {
    base::OnceCallback<void(device::mojom::XRSessionPtr,
                            device::mojom::XRSessionControllerPtr)>
        non_immersive_callback =
            base::BindOnce(&XRDeviceImpl::OnInlineSessionCreated,
                           weak_ptr_factory_.GetWeakPtr(), session_runtime_id,
                           std::move(callback));
    runtime->GetRuntime()->RequestSession(std::move(runtime_options),
                                          std::move(non_immersive_callback));
  }
}

void XRDeviceImpl::SupportsSession(
    device::mojom::XRSessionOptionsPtr options,
    device::mojom::XRDevice::SupportsSessionCallback callback) {
  runtime_manager_->SupportsSession(std::move(options), std::move(callback));
}

void XRDeviceImpl::ExitPresent() {
  BrowserXRRuntime* immersive_runtime = runtime_manager_->GetImmersiveRuntime();
  if (immersive_runtime)
    immersive_runtime->ExitPresent(this);
}

void XRDeviceImpl::SetListeningForActivate(
    device::mojom::VRDisplayClientPtr client) {
  client_ = std::move(client);
  BrowserXRRuntime* immersive_runtime = runtime_manager_->GetImmersiveRuntime();
  if (immersive_runtime && client_) {
    immersive_runtime->UpdateListeningForActivate(this);
  }
}

void XRDeviceImpl::GetImmersiveVRDisplayInfo(
    device::mojom::XRDevice::GetImmersiveVRDisplayInfoCallback callback) {
  BrowserXRRuntime* immersive_runtime = runtime_manager_->GetImmersiveRuntime();
  if (!immersive_runtime) {
    std::move(callback).Run(nullptr);
    return;
  }

  immersive_runtime->InitializeAndGetDisplayInfo(render_frame_host_,
                                                 std::move(callback));
}

void XRDeviceImpl::SetInFocusedFrame(bool in_focused_frame) {
  in_focused_frame_ = in_focused_frame;
  if (ListeningForActivate()) {
    BrowserXRRuntime* immersive_runtime =
        runtime_manager_->GetImmersiveRuntime();
    if (immersive_runtime)
      immersive_runtime->UpdateListeningForActivate(this);
  }

  magic_window_controllers_.ForAllPtrs(
      [&in_focused_frame](device::mojom::XRSessionController* controller) {
        controller->SetFrameDataRestricted(!in_focused_frame);
      });
}

void XRDeviceImpl::RuntimesChanged() {
  device::mojom::VRDisplayInfoPtr display_info =
      runtime_manager_->GetCurrentVRDisplayInfo(this);
  if (display_info) {
    session_clients_.ForAllPtrs(
        [&display_info](device::mojom::XRSessionClient* client) {
          client->OnChanged(display_info.Clone());
        });
  }
}

void XRDeviceImpl::OnExitPresent() {
  session_clients_.ForAllPtrs(
      [](device::mojom::XRSessionClient* client) { client->OnExitPresent(); });
}

// TODO(http://crbug.com/845283): We should store the state here and send blur
// messages to sessions that start blurred.
void XRDeviceImpl::OnBlur() {
  session_clients_.ForAllPtrs(
      [](device::mojom::XRSessionClient* client) { client->OnBlur(); });
}

void XRDeviceImpl::OnFocus() {
  session_clients_.ForAllPtrs(
      [](device::mojom::XRSessionClient* client) { client->OnFocus(); });
}

void XRDeviceImpl::OnActivate(device::mojom::VRDisplayEventReason reason,
                              base::OnceCallback<void(bool)> on_handled) {
  if (client_) {
    client_->OnActivate(reason, std::move(on_handled));
  }
}

void XRDeviceImpl::OnDeactivate(device::mojom::VRDisplayEventReason reason) {
  if (client_) {
    client_->OnDeactivate(reason);
  }
}

content::WebContents* XRDeviceImpl::GetWebContents() {
  if (render_frame_host_ != nullptr) {
    return content::WebContents::FromRenderFrameHost(render_frame_host_);
  }

  // We should only have a null render_frame_host_ for some unittests, for which
  // we don't actually expect to get here.
  NOTREACHED();
  return nullptr;
}

bool XRDeviceImpl::IsSecureContextRequirementSatisfied() {
  // We require secure connections unless both the webvr flag and the
  // http flag are enabled.
  bool requires_secure_context =
      !kAllowHTTPWebVRWithFlag ||
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWebVR);
  if (!requires_secure_context)
    return true;
  return IsSecureContext(render_frame_host_);
}

}  // namespace vr
