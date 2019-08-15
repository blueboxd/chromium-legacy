// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/service/vr_service_impl.h"

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
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "device/vr/buildflags/buildflags.h"
#include "device/vr/vr_device.h"

#if defined(OS_WIN)
#include "chrome/browser/vr/service/xr_session_request_consent_manager.h"
#elif defined(OS_ANDROID)
#include "chrome/browser/vr/service/gvr_consent_helper.h"
#if BUILDFLAG(ENABLE_ARCORE)
#include "chrome/browser/vr/service/arcore_consent_prompt_interface.h"
#endif
#endif

namespace {

// TODO(https://crbug.com/960132): When we unship WebVR 1.1, set this to false.
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

namespace vr {

VRServiceImpl::VRServiceImpl(content::RenderFrameHost* render_frame_host)
    : WebContentsObserver(
          content::WebContents::FromRenderFrameHost(render_frame_host)),
      render_frame_host_(render_frame_host),
      // TODO(https://crbug.com/846392): render_frame_host can be null because
      // of a test, not because a VRServiceImpl can be created without it.
      in_focused_frame_(render_frame_host
                            ? render_frame_host->GetView()->HasFocus()
                            : false) {
  DCHECK(render_frame_host_);

  runtime_manager_ = XRRuntimeManager::GetOrCreateInstance();
  runtime_manager_->AddService(this);

  magic_window_controllers_.set_connection_error_handler(base::BindRepeating(
      &VRServiceImpl::OnInlineSessionDisconnected,
      base::Unretained(this)));  // Unretained is OK since the collection is
                                 // owned by VRServiceImpl.
}

// Constructor for testing.
VRServiceImpl::VRServiceImpl() : render_frame_host_(nullptr) {
  runtime_manager_ = XRRuntimeManager::GetOrCreateInstance();
  runtime_manager_->AddService(this);
}

VRServiceImpl::~VRServiceImpl() {
  runtime_manager_->RemoveService(this);
}

void VRServiceImpl::Create(content::RenderFrameHost* render_frame_host,
                           device::mojom::VRServiceRequest request) {
  std::unique_ptr<VRServiceImpl> vr_service_impl =
      std::make_unique<VRServiceImpl>(render_frame_host);

  VRServiceImpl* impl = vr_service_impl.get();
  impl->binding_ =
      mojo::MakeStrongBinding(std::move(vr_service_impl), std::move(request));
}

void VRServiceImpl::InitializationComplete() {
  // After initialization has completed, we can correctly answer
  // supportsSession, and can provide correct display capabilities.
  initialization_complete_ = true;

  ResolvePendingRequests();
}

void VRServiceImpl::SetClient(
    device::mojom::VRServiceClientPtr service_client) {
  if (service_client_) {
    mojo::ReportBadMessage("ServiceClient should only be set once.");
    return;
  }

  service_client_ = std::move(service_client);
}

void VRServiceImpl::ResolvePendingRequests() {
  for (auto& callback : pending_requests_) {
    std::move(callback).Run();
  }
  pending_requests_.clear();
}

void VRServiceImpl::OnDisplayInfoChanged() {
  device::mojom::VRDisplayInfoPtr display_info =
      runtime_manager_->GetCurrentVRDisplayInfo(this);
  if (display_info) {
    session_clients_.ForAllPtrs(
        [&display_info](device::mojom::XRSessionClient* client) {
          client->OnChanged(display_info.Clone());
        });
  }
}

void VRServiceImpl::RuntimesChanged() {
  OnDisplayInfoChanged();

  if (service_client_) {
    service_client_->OnDeviceChanged();
  }
}

void VRServiceImpl::OnWebContentsFocused(content::RenderWidgetHost* host) {
  OnWebContentsFocusChanged(host, true);
}

void VRServiceImpl::OnWebContentsLostFocus(content::RenderWidgetHost* host) {
  OnWebContentsFocusChanged(host, false);
}

void VRServiceImpl::RenderFrameDeleted(content::RenderFrameHost* host) {
  if (host != render_frame_host_)
    return;

  // Binding should always be live here, as this is a StrongBinding.
  // Close the binding (and delete this VrServiceImpl) when the RenderFrameHost
  // is deleted.
  DCHECK(binding_.get());
  binding_->Close();
}

void VRServiceImpl::OnWebContentsFocusChanged(content::RenderWidgetHost* host,
                                              bool focused) {
  if (!render_frame_host_->GetView() ||
      render_frame_host_->GetView()->GetRenderWidgetHost() != host) {
    return;
  }
  SetInFocusedFrame(focused);
}

// static
bool VRServiceImpl::IsXrDeviceConsentPromptDisabledForTesting() {
  static bool is_xr_device_consent_prompt_disabled_for_testing =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableXrDeviceConsentPromptForTesting);
  return is_xr_device_consent_prompt_disabled_for_testing;
}

void VRServiceImpl::OnInlineSessionCreated(
    device::mojom::XRDeviceId session_runtime_id,
    device::mojom::VRService::RequestSessionCallback callback,
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

void VRServiceImpl::OnInlineSessionDisconnected(size_t session_id) {
  // Notify metrics helper that inline session was stopped.
  auto* metrics_helper = GetSessionMetricsHelper();
  metrics_helper->RecordInlineSessionStop(session_id);
}

SessionMetricsHelper* VRServiceImpl::GetSessionMetricsHelper() {
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

void VRServiceImpl::OnSessionCreated(
    device::mojom::XRDeviceId session_runtime_id,
    device::mojom::VRService::RequestSessionCallback callback,
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

void VRServiceImpl::RequestSession(
    device::mojom::XRSessionOptionsPtr options,
    device::mojom::VRService::RequestSessionCallback callback) {
  DCHECK(options);

  // Check that the request satisfies secure context requirements.
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

  if (runtime_manager_->IsOtherClientPresenting(this)) {
    // Can't create sessions while an immersive session exists.
    std::move(callback).Run(
        device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::EXISTING_IMMERSIVE_SESSION));
    return;
  }

  // Queue the request to get to when initialization has completed.
  if (!initialization_complete_) {
    pending_requests_.push_back(
        base::BindOnce(&VRServiceImpl::RequestSession, base::Unretained(this),
                       std::move(options), std::move(callback)));
    return;
  }

  // Inline sessions do not need permissions. WebVR did not require
  // permissions.
  // TODO(crbug.com/968221): Address privacy requirements for inline sessions
  if (!options->immersive || options->is_legacy_webvr) {
    DoRequestSession(std::move(options), std::move(callback));
    return;
  }

  // Check if there is the user has already granted consent earlier for this
  // device. If they did, skip the prompt.
  auto opt_device_id = runtime_manager_->GetRuntimeIdForOptions(options.get());
  if (!opt_device_id) {
    std::move(callback).Run(
        device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::NO_RUNTIME_FOUND));
    return;
  }
  if (IsConsentGrantedForDevice(opt_device_id.value())) {
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
            base::BindOnce(&VRServiceImpl::OnConsentResult,
                           weak_ptr_factory_.GetWeakPtr(), std::move(options),
                           std::move(callback)));
      }
    }
#else
    std::move(callback).Run(nullptr);
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
            base::BindOnce(&VRServiceImpl::OnConsentResult,
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
        base::BindOnce(&VRServiceImpl::OnConsentResult,
                       weak_ptr_factory_.GetWeakPtr(), std::move(options),
                       std::move(callback)));
  }
  return;

#else

  NOTREACHED();

#endif
}

void VRServiceImpl::OnConsentResult(
    device::mojom::XRSessionOptionsPtr options,
    device::mojom::VRService::RequestSessionCallback callback,
    bool is_consent_granted) {
  if (!is_consent_granted) {
    std::move(callback).Run(
        device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::USER_DENIED_CONSENT));
    return;
  }

  auto opt_device_id = runtime_manager_->GetRuntimeIdForOptions(options.get());
  if (!opt_device_id) {
    std::move(callback).Run(
        device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::NO_RUNTIME_FOUND));
    return;
  }
  AddConsentGrantedDevice(opt_device_id.value());

  // Re-check for another client instance after a potential user consent.
  if (runtime_manager_->IsOtherClientPresenting(this)) {
    // Can't create sessions while an immersive session exists.
    std::move(callback).Run(
        device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::EXISTING_IMMERSIVE_SESSION));
    return;
  }

  DoRequestSession(std::move(options), std::move(callback));
}

void VRServiceImpl::DoRequestSession(
    device::mojom::XRSessionOptionsPtr options,
    device::mojom::VRService::RequestSessionCallback callback) {
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
        base::BindOnce(&VRServiceImpl::OnSessionCreated,
                       weak_ptr_factory_.GetWeakPtr(), session_runtime_id,
                       std::move(callback));
    runtime->RequestSession(this, std::move(runtime_options),
                            std::move(immersive_callback));
  } else {
    base::OnceCallback<void(device::mojom::XRSessionPtr,
                            device::mojom::XRSessionControllerPtr)>
        non_immersive_callback =
            base::BindOnce(&VRServiceImpl::OnInlineSessionCreated,
                           weak_ptr_factory_.GetWeakPtr(), session_runtime_id,
                           std::move(callback));
    runtime->GetRuntime()->RequestSession(std::move(runtime_options),
                                          std::move(non_immersive_callback));
  }
}

void VRServiceImpl::SupportsSession(
    device::mojom::XRSessionOptionsPtr options,
    device::mojom::VRService::SupportsSessionCallback callback) {
  if (!initialization_complete_) {
    pending_requests_.push_back(
        base::BindOnce(&VRServiceImpl::SupportsSession, base::Unretained(this),
                       std::move(options), std::move(callback)));
    return;
  }
  runtime_manager_->SupportsSession(std::move(options), std::move(callback));
}

void VRServiceImpl::ExitPresent() {
  BrowserXRRuntime* immersive_runtime = runtime_manager_->GetImmersiveRuntime();
  if (immersive_runtime)
    immersive_runtime->ExitPresent(this);
}

void VRServiceImpl::SetListeningForActivate(
    device::mojom::VRDisplayClientPtr display_client) {
  display_client_ = std::move(display_client);
  BrowserXRRuntime* immersive_runtime = runtime_manager_->GetImmersiveRuntime();
  if (immersive_runtime && display_client_) {
    immersive_runtime->UpdateListeningForActivate(this);
  }
}

void VRServiceImpl::GetImmersiveVRDisplayInfo(
    device::mojom::VRService::GetImmersiveVRDisplayInfoCallback callback) {
  if (!initialization_complete_) {
    pending_requests_.push_back(
        base::BindOnce(&VRServiceImpl::GetImmersiveVRDisplayInfo,
                       base::Unretained(this), std::move(callback)));
    return;
  }

  BrowserXRRuntime* immersive_runtime = runtime_manager_->GetImmersiveRuntime();
  if (immersive_runtime) {
    immersive_runtime->InitializeAndGetDisplayInfo(render_frame_host_,
                                                   std::move(callback));
    return;
  }

  std::move(callback).Run(nullptr);
}

void VRServiceImpl::SetInFocusedFrame(bool in_focused_frame) {
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

void VRServiceImpl::OnExitPresent() {
  session_clients_.ForAllPtrs(
      [](device::mojom::XRSessionClient* client) { client->OnExitPresent(); });
}

// TODO(http://crbug.com/845283): We should store the state here and send blur
// messages to sessions that start blurred.
void VRServiceImpl::OnBlur() {
  session_clients_.ForAllPtrs(
      [](device::mojom::XRSessionClient* client) { client->OnBlur(); });
}

void VRServiceImpl::OnFocus() {
  session_clients_.ForAllPtrs(
      [](device::mojom::XRSessionClient* client) { client->OnFocus(); });
}

void VRServiceImpl::OnActivate(device::mojom::VRDisplayEventReason reason,
                               base::OnceCallback<void(bool)> on_handled) {
  if (display_client_) {
    display_client_->OnActivate(reason, std::move(on_handled));
  }
}

void VRServiceImpl::OnDeactivate(device::mojom::VRDisplayEventReason reason) {
  if (display_client_) {
    display_client_->OnDeactivate(reason);
  }
}

content::WebContents* VRServiceImpl::GetWebContents() {
  if (render_frame_host_ != nullptr) {
    return content::WebContents::FromRenderFrameHost(render_frame_host_);
  }

  // We should only have a null render_frame_host_ for some unittests, for which
  // we don't actually expect to get here.
  NOTREACHED();
  return nullptr;
}

bool VRServiceImpl::IsSecureContextRequirementSatisfied() {
  // We require secure connections unless both the webvr flag and the
  // http flag are enabled.
  static bool requires_secure_context =
      !kAllowHTTPWebVRWithFlag ||
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWebVR);
  if (!requires_secure_context)
    return true;
  return IsSecureContext(render_frame_host_);
}

bool VRServiceImpl::IsConsentGrantedForDevice(
    device::mojom::XRDeviceId device_id) {
  return consent_granted_devices_.find(device_id) !=
         consent_granted_devices_.end();
}

void VRServiceImpl::AddConsentGrantedDevice(
    device::mojom::XRDeviceId device_id) {
  DCHECK(!IsConsentGrantedForDevice(device_id));
  consent_granted_devices_.insert(device_id);
}

}  // namespace vr
