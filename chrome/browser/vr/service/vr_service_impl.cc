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

vr::XrConsentPromptLevel GetRequiredConsentLevel(
    bool immersive,
    const vr::BrowserXRRuntime* runtime,
    const std::set<device::mojom::XRSessionFeature>& requested_features) {
  if (requested_features.find(
          device::mojom::XRSessionFeature::REF_SPACE_BOUNDED_FLOOR) !=
      requested_features.end()) {
    return vr::XrConsentPromptLevel::kVRFloorPlan;
  }

  // If the device supports a custom IPD and it will be exposed (via immersive),
  // we need to warn about physical features Being exposed.
  if (runtime->SupportsCustomIPD() && immersive) {
    return vr::XrConsentPromptLevel::kVRFeatures;
  }

  // If local-floor is requested and the device supports a user inputted or real
  // height, we need to warn about physical features being exposed.
  // Note that while this is also the case for bounded-floor, that is covered
  // by the stricter kVRFloorPlan Prompt set above.
  if (requested_features.find(
          device::mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR) !=
          requested_features.end() &&
      runtime->SupportsNonEmulatedHeight()) {
    return vr::XrConsentPromptLevel::kVRFeatures;
  }

  // In the absence of other items that need to be consented, an immersive
  // session always requires some level of consent.
  if (immersive) {
    return vr::XrConsentPromptLevel::kDefault;
  }

  return vr::XrConsentPromptLevel::kNone;
}

}  // namespace

namespace vr {

VRServiceImpl::VRServiceImpl(content::RenderFrameHost* render_frame_host)
    : WebContentsObserver(
          content::WebContents::FromRenderFrameHost(render_frame_host)),
      render_frame_host_(render_frame_host),
      in_focused_frame_(render_frame_host->GetView()->HasFocus()) {
  DCHECK(render_frame_host_);

  runtime_manager_ = XRRuntimeManager::GetOrCreateInstance();
  runtime_manager_->AddService(this);

  magic_window_controllers_.set_connection_error_handler(base::BindRepeating(
      &VRServiceImpl::OnInlineSessionDisconnected,
      base::Unretained(this)));  // Unretained is OK since the collection is
                                 // owned by VRServiceImpl.
}

// Constructor for testing.
VRServiceImpl::VRServiceImpl(util::PassKey<XRRuntimeManagerTest>)
    : render_frame_host_(nullptr) {
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
    mojo::PendingRemote<device::mojom::VRServiceClient> service_client) {
  if (service_client_) {
    mojo::ReportBadMessage("ServiceClient should only be set once.");
    return;
  }

  service_client_.Bind(std::move(service_client));
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

  in_focused_frame_ = focused;
  if (ListeningForActivate()) {
    BrowserXRRuntime* immersive_runtime =
        runtime_manager_->GetImmersiveRuntime();
    if (immersive_runtime)
      immersive_runtime->UpdateListeningForActivate(this);
  }

  magic_window_controllers_.ForAllPtrs(
      [focused](device::mojom::XRSessionController* controller) {
        controller->SetFrameDataRestricted(!focused);
      });
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
    const std::set<device::mojom::XRSessionFeature>& enabled_features,
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

  OnSessionCreated(session_runtime_id, std::move(callback), enabled_features,
                   std::move(session));
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
    const std::set<device::mojom::XRSessionFeature>& enabled_features,
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

  session->enabled_features.clear();
  for (const auto& feature : enabled_features) {
    session->enabled_features.push_back(feature);
  }

  session_clients_.AddPtr(std::move(client));

  std::move(callback).Run(
      device::mojom::RequestSessionResult::NewSession(std::move(session)));
}

void VRServiceImpl::RequestSession(
    device::mojom::XRSessionOptionsPtr options,
    device::mojom::VRService::RequestSessionCallback callback) {
  DCHECK(options);

  // Queue the request to get to when initialization has completed.
  if (!initialization_complete_) {
    pending_requests_.push_back(
        base::BindOnce(&VRServiceImpl::RequestSession, base::Unretained(this),
                       std::move(options), std::move(callback)));
    return;
  }

  // Check that the request satisfies secure context requirements.
  if (!IsSecureContextRequirementSatisfied()) {
    std::move(callback).Run(
        device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::ORIGIN_NOT_SECURE));
    return;
  }

  if (runtime_manager_->IsOtherClientPresenting(this)) {
    // Can't create sessions while an immersive session exists.
    std::move(callback).Run(
        device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::EXISTING_IMMERSIVE_SESSION));
    return;
  }

  auto* runtime = runtime_manager_->GetRuntimeForOptions(options.get());
  if (!runtime) {
    std::move(callback).Run(
        device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::NO_RUNTIME_FOUND));
    return;
  }

  // GetRuntimeForOptions should only return a device that supports all required
  // features.
  std::set<device::mojom::XRSessionFeature> requested_features;
  for (const auto& feature : options->required_features) {
    requested_features.insert(feature);
  }

  // The consent flow cannot differentiate between optional and required
  // features, but we don't need to block creation if an optional feature is
  // not supported. Add all requested features to the set of supported features.
  for (const auto& feature : options->optional_features) {
    if (runtime->SupportsFeature(feature)) {
      requested_features.insert(feature);
    }
  }

  ShowConsentPrompt(std::move(options), std::move(callback), runtime,
                    std::move(requested_features));
}

void VRServiceImpl::ShowConsentPrompt(
    device::mojom::XRSessionOptionsPtr options,
    device::mojom::VRService::RequestSessionCallback callback,
    BrowserXRRuntime* runtime,
    std::set<device::mojom::XRSessionFeature> requested_features) {
  DCHECK(options);
  DCHECK(runtime);

#if defined(OS_WIN)
  DCHECK(!options->environment_integration);
#endif

  // WebVR did not require permissions.
  // TODO(crbug.com/968221): Address privacy requirements for inline sessions
  if (options->is_legacy_webvr) {
    DoRequestSession(std::move(options), std::move(callback), runtime,
                     std::move(requested_features));
    return;
  }

  XrConsentPromptLevel consent_level =
      GetRequiredConsentLevel(options->immersive, runtime, requested_features);

  // Skip the consent prompt if the user has already consented for this device,
  // or if consent is not needed.
  if (consent_level == XrConsentPromptLevel::kNone ||
      IsConsentGrantedForDevice(runtime->GetId(), consent_level) ||
      IsXrDeviceConsentPromptDisabledForTesting()) {
    DoRequestSession(std::move(options), std::move(callback), runtime,
                     std::move(requested_features));
    return;
  }

  // TODO(crbug.com/968233): Unify the below consent flow.
#if defined(OS_ANDROID)
  if (options->environment_integration) {
#if BUILDFLAG(ENABLE_ARCORE)
    ArCoreConsentPromptInterface::GetInstance()->ShowConsentPrompt(
        render_frame_host_->GetProcess()->GetID(),
        render_frame_host_->GetRoutingID(),
        base::BindOnce(&VRServiceImpl::OnConsentResult,
                       weak_ptr_factory_.GetWeakPtr(), std::move(options),
                       std::move(callback), runtime->GetId(),
                       std::move(requested_features), consent_level));
    return;
#else
    std::move(callback).Run(nullptr);
    return;
#endif
  } else {
    // GVR.
    GvrConsentHelper::GetInstance()->PromptUserAndGetConsent(
        render_frame_host_->GetProcess()->GetID(),
        render_frame_host_->GetRoutingID(), consent_level,
        base::BindOnce(&VRServiceImpl::OnConsentResult,
                       weak_ptr_factory_.GetWeakPtr(), std::move(options),
                       std::move(callback), runtime->GetId(),
                       std::move(requested_features)));
    return;
  }
#elif defined(OS_WIN)
  XRSessionRequestConsentManager::Instance()->ShowDialogAndGetConsent(
      GetWebContents(), consent_level,
      base::BindOnce(&VRServiceImpl::OnConsentResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(options),
                     std::move(callback), runtime->GetId(),
                     std::move(requested_features)));
  return;
#endif

  NOTREACHED();
}

void VRServiceImpl::OnConsentResult(
    device::mojom::XRSessionOptionsPtr options,
    device::mojom::VRService::RequestSessionCallback callback,
    device::mojom::XRDeviceId expected_runtime_id,
    std::set<device::mojom::XRSessionFeature> enabled_features,
    XrConsentPromptLevel consent_level,
    bool is_consent_granted) {
  if (!is_consent_granted) {
    std::move(callback).Run(
        device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::USER_DENIED_CONSENT));
    return;
  }

  // Get the runtime again, since we're running in an async context
  // and the pointer returned from `GetRuntimeForOptions` is non-owning.
  auto* runtime = runtime_manager_->GetRuntimeForOptions(options.get());

  // Ensure that it's the same runtime as the one we expect.
  if (!runtime || runtime->GetId() != expected_runtime_id) {
    std::move(callback).Run(
        device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::UNKNOWN_RUNTIME_ERROR));
    return;
  }
  AddConsentGrantedDevice(runtime->GetId(), consent_level);

  // Re-check for another client instance after a potential user consent.
  if (runtime_manager_->IsOtherClientPresenting(this)) {
    // Can't create sessions while an immersive session exists.
    std::move(callback).Run(
        device::mojom::RequestSessionResult::NewFailureReason(
            device::mojom::RequestSessionError::EXISTING_IMMERSIVE_SESSION));
    return;
  }

  DoRequestSession(std::move(options), std::move(callback), runtime,
                   std::move(enabled_features));
}

void VRServiceImpl::DoRequestSession(
    device::mojom::XRSessionOptionsPtr options,
    device::mojom::VRService::RequestSessionCallback callback,
    BrowserXRRuntime* runtime,
    std::set<device::mojom::XRSessionFeature> enabled_features) {
  // Get the runtime we'll be creating a session with.
  DCHECK(runtime);

  device::mojom::XRDeviceId session_runtime_id = runtime->GetId();
  TRACE_EVENT_INSTANT1("xr", "GetRuntimeForOptions", TRACE_EVENT_SCOPE_THREAD,
                       "id", session_runtime_id);

  auto runtime_options = GetRuntimeOptions(options.get());

#if defined(OS_ANDROID) && BUILDFLAG(ENABLE_ARCORE)
  if (session_runtime_id == device::mojom::XRDeviceId::ARCORE_DEVICE_ID) {
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
                       std::move(callback), std::move(enabled_features));
    runtime->RequestSession(this, std::move(runtime_options),
                            std::move(immersive_callback));
  } else {
    base::OnceCallback<void(device::mojom::XRSessionPtr,
                            device::mojom::XRSessionControllerPtr)>
        non_immersive_callback =
            base::BindOnce(&VRServiceImpl::OnInlineSessionCreated,
                           weak_ptr_factory_.GetWeakPtr(), session_runtime_id,
                           std::move(callback), std::move(enabled_features));
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
    mojo::PendingRemote<device::mojom::VRDisplayClient> display_client) {
  // TODO(crbug.com/999745): Remove the check if the condition to check if
  // |display_client| is nullptr is not required.
  if (display_client)
    display_client_.Bind(std::move(display_client));
  else
    display_client_.reset();
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

void VRServiceImpl::OnExitPresent() {
  session_clients_.ForAllPtrs(
      [](device::mojom::XRSessionClient* client) { client->OnExitPresent(); });
}

void VRServiceImpl::OnVisibilityStateChanged(
    device::mojom::XRVisibilityState visiblity_state) {
  session_clients_.ForAllPtrs(
      [visiblity_state](device::mojom::XRSessionClient* client) {
        client->OnVisibilityStateChanged(visiblity_state);
      });
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
  return content::WebContents::FromRenderFrameHost(render_frame_host_);
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
    device::mojom::XRDeviceId device_id,
    XrConsentPromptLevel consent_level) {
  auto it = consent_granted_devices_.find(device_id);
  return it != consent_granted_devices_.end() && it->second >= consent_level;
}

void VRServiceImpl::AddConsentGrantedDevice(
    device::mojom::XRDeviceId device_id,
    XrConsentPromptLevel consent_level) {
  auto it = consent_granted_devices_.find(device_id);
  if (it == consent_granted_devices_.end() || it->second < consent_level) {
    consent_granted_devices_[device_id] = consent_level;
  }
}

}  // namespace vr
