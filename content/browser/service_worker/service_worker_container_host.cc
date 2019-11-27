// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_container_host.h"

#include "base/guid.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_object_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_registration_object_host.h"
#include "content/browser/web_contents/frame_tree_node_id_registry.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_client.h"
#include "content/public/common/origin_util.h"

namespace content {

namespace {

void RunCallbacks(
    std::vector<ServiceWorkerContainerHost::ExecutionReadyCallback> callbacks) {
  for (auto& callback : callbacks) {
    std::move(callback).Run();
  }
}

ServiceWorkerMetrics::EventType PurposeToEventType(
    blink::mojom::ControllerServiceWorkerPurpose purpose) {
  switch (purpose) {
    case blink::mojom::ControllerServiceWorkerPurpose::FETCH_SUB_RESOURCE:
      return ServiceWorkerMetrics::EventType::FETCH_SUB_RESOURCE;
  }
  NOTREACHED();
  return ServiceWorkerMetrics::EventType::UNKNOWN;
}

}  // namespace

ServiceWorkerContainerHost::ServiceWorkerContainerHost(
    blink::mojom::ServiceWorkerProviderType type,
    bool is_parent_frame_secure,
    int frame_tree_node_id,
    mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
        container_remote,
    ServiceWorkerProviderHost* provider_host,
    base::WeakPtr<ServiceWorkerContextCore> context)
    : type_(type),
      create_time_(base::TimeTicks::Now()),
      is_parent_frame_secure_(is_parent_frame_secure),
      frame_tree_node_id_(frame_tree_node_id),
      web_contents_getter_(
          frame_tree_node_id == FrameTreeNode::kFrameTreeNodeInvalidId
              ? base::NullCallback()
              : base::BindRepeating(&WebContents::FromFrameTreeNodeId,
                                    frame_tree_node_id)),
      client_uuid_(base::GenerateGUID()),
      provider_host_(provider_host),
      context_(std::move(context)) {
  DCHECK(provider_host_);
  DCHECK(context_);
  if (container_remote) {
    DCHECK(IsContainerForClient());
    container_.Bind(std::move(container_remote));
  } else {
    DCHECK(IsContainerForServiceWorker());
  }
}

ServiceWorkerContainerHost::~ServiceWorkerContainerHost() {
  if (IsBackForwardCacheEnabled() &&
      ServiceWorkerContext::IsServiceWorkerOnUIEnabled() &&
      IsContainerForClient()) {
    auto* rfh = RenderFrameHostImpl::FromID(process_id_, frame_id_);
    if (rfh)
      rfh->RemoveServiceWorkerContainerHost(this);
  }

  if (fetch_request_window_id_)
    FrameTreeNodeIdRegistry::GetInstance()->Remove(fetch_request_window_id_);

  if (controller_)
    controller_->OnControlleeDestroyed(client_uuid_);

  // Remove |provider_host_| as an observer of ServiceWorkerRegistrations.
  // TODO(falken): Use ScopedObserver instead of this explicit call.
  controller_.reset();
  controller_registration_.reset();

  // Ensure callbacks awaiting execution ready are notified.
  RunExecutionReadyCallbacks();
}

void ServiceWorkerContainerHost::EnsureControllerServiceWorker(
    mojo::PendingReceiver<blink::mojom::ControllerServiceWorker> receiver,
    blink::mojom::ControllerServiceWorkerPurpose purpose) {
  // TODO(kinuko): Log the reasons we drop the request.
  if (!context_ || !controller_)
    return;

  controller_->RunAfterStartWorker(
      PurposeToEventType(purpose),
      base::BindOnce(&ServiceWorkerContainerHost::StartControllerComplete,
                     weak_factory_.GetWeakPtr(), std::move(receiver)));
}

void ServiceWorkerContainerHost::OnSkippedWaiting(
    ServiceWorkerRegistration* registration) {
  if (controller_registration_ != registration)
    return;

#if DCHECK_IS_ON()
  DCHECK(controller_);
  ServiceWorkerVersion* active = controller_registration_->active_version();
  DCHECK(active);
  DCHECK_NE(active, controller_.get());
  DCHECK_EQ(active->status(), ServiceWorkerVersion::ACTIVATING);
#endif  // DCHECK_IS_ON()

  if (ServiceWorkerContext::IsServiceWorkerOnUIEnabled() &&
      IsBackForwardCacheEnabled() && IsInBackForwardCache()) {
    // This ServiceWorkerContainerHost is evicted from BackForwardCache in
    // |ActivateWaitingVersion|, but not deleted yet. This can happen because
    // asynchronous eviction and |OnSkippedWaiting| are in the same task.
    // The controller does not have to be updated because |this| will be evicted
    // from BackForwardCache.
    // TODO(yuzus): Wire registration with ServiceWorkerContainerHost so that we
    // can check on the caller side.
    return;
  }

  UpdateController(true /* notify_controllerchange */);
}

void ServiceWorkerContainerHost::PostMessageToClient(
    ServiceWorkerVersion* version,
    blink::TransferableMessage message) {
  DCHECK(IsContainerForClient());

  blink::mojom::ServiceWorkerObjectInfoPtr info;
  base::WeakPtr<ServiceWorkerObjectHost> object_host =
      GetOrCreateServiceWorkerObjectHost(version);
  if (object_host)
    info = object_host->CreateCompleteObjectInfoToSend();
  container_->PostMessageToClient(std::move(info), std::move(message));
}

void ServiceWorkerContainerHost::CountFeature(
    blink::mojom::WebFeature feature) {
  // CountFeature is a message about the client's controller. It should be sent
  // only for clients.
  DCHECK(IsContainerForClient());

  // And only when loading finished so the controller is really settled.
  if (!IsControllerDecided())
    return;

  container_->CountFeature(feature);
}

void ServiceWorkerContainerHost::SendSetControllerServiceWorker(
    bool notify_controllerchange) {
  DCHECK(IsContainerForClient());

  auto controller_info = blink::mojom::ControllerServiceWorkerInfo::New();
  controller_info->client_id = client_uuid_;
  // Set |fetch_request_window_id| only when |controller_| is available.
  // Setting |fetch_request_window_id| should not affect correctness, however,
  // we have the extensions bug, https://crbug.com/963748, which we don't yet
  // understand.  That is why we don't set |fetch_request_window_id| if there
  // is no controller, at least, until we can fix the extension bug.
  if (controller_ && fetch_request_window_id_) {
    controller_info->fetch_request_window_id =
        base::make_optional(fetch_request_window_id_);
  }

  if (!controller_) {
    container_->SetController(std::move(controller_info),
                              notify_controllerchange);
    return;
  }

  DCHECK(controller_registration());
  DCHECK_EQ(controller_registration_->active_version(), controller_.get());

  controller_info->mode = GetControllerMode();

  // Pass an endpoint for the client to talk to this controller.
  mojo::Remote<blink::mojom::ControllerServiceWorker> remote =
      GetRemoteControllerServiceWorker();
  if (remote.is_bound()) {
    controller_info->remote_controller = remote.Unbind();
  }

  // Set the info for the JavaScript ServiceWorkerContainer#controller object.
  base::WeakPtr<ServiceWorkerObjectHost> object_host =
      GetOrCreateServiceWorkerObjectHost(controller_);
  if (object_host) {
    controller_info->object_info =
        object_host->CreateCompleteObjectInfoToSend();
  }

  // Populate used features for UseCounter purposes.
  for (const blink::mojom::WebFeature feature : controller_->used_features())
    controller_info->used_features.push_back(feature);

  container_->SetController(std::move(controller_info),
                            notify_controllerchange);
}

void ServiceWorkerContainerHost::NotifyControllerLost() {
  SetControllerRegistration(nullptr, true /* notify_controllerchange */);
}

void ServiceWorkerContainerHost::ClaimedByRegistration(
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK(IsContainerForClient());
  DCHECK(registration->active_version());
  DCHECK(is_execution_ready());

  // TODO(falken): This should just early return, or DCHECK. claim() should have
  // no effect on a page that's already using the registration.
  if (registration == controller_registration_) {
    UpdateController(true /* notify_controllerchange */);
    return;
  }

  SetControllerRegistration(registration, true /* notify_controllerchange */);
}

blink::mojom::ServiceWorkerRegistrationObjectInfoPtr
ServiceWorkerContainerHost::CreateServiceWorkerRegistrationObjectInfo(
    scoped_refptr<ServiceWorkerRegistration> registration) {
  int64_t registration_id = registration->id();
  auto existing_host = registration_object_hosts_.find(registration_id);
  if (existing_host != registration_object_hosts_.end()) {
    return existing_host->second->CreateObjectInfo();
  }
  registration_object_hosts_[registration_id] =
      std::make_unique<ServiceWorkerRegistrationObjectHost>(
          context_, this, std::move(registration));
  return registration_object_hosts_[registration_id]->CreateObjectInfo();
}

void ServiceWorkerContainerHost::RemoveServiceWorkerRegistrationObjectHost(
    int64_t registration_id) {
  DCHECK(base::Contains(registration_object_hosts_, registration_id));
  registration_object_hosts_.erase(registration_id);
}

blink::mojom::ServiceWorkerObjectInfoPtr
ServiceWorkerContainerHost::CreateServiceWorkerObjectInfoToSend(
    scoped_refptr<ServiceWorkerVersion> version) {
  int64_t version_id = version->version_id();
  auto existing_object_host = service_worker_object_hosts_.find(version_id);
  if (existing_object_host != service_worker_object_hosts_.end()) {
    return existing_object_host->second->CreateCompleteObjectInfoToSend();
  }
  service_worker_object_hosts_[version_id] =
      std::make_unique<ServiceWorkerObjectHost>(context_, this,
                                                std::move(version));
  return service_worker_object_hosts_[version_id]
      ->CreateCompleteObjectInfoToSend();
}

base::WeakPtr<ServiceWorkerObjectHost>
ServiceWorkerContainerHost::GetOrCreateServiceWorkerObjectHost(
    scoped_refptr<ServiceWorkerVersion> version) {
  if (!context_ || !version)
    return nullptr;

  const int64_t version_id = version->version_id();
  auto existing_object_host = service_worker_object_hosts_.find(version_id);
  if (existing_object_host != service_worker_object_hosts_.end())
    return existing_object_host->second->AsWeakPtr();

  service_worker_object_hosts_[version_id] =
      std::make_unique<ServiceWorkerObjectHost>(context_, this,
                                                std::move(version));
  return service_worker_object_hosts_[version_id]->AsWeakPtr();
}

void ServiceWorkerContainerHost::RemoveServiceWorkerObjectHost(
    int64_t version_id) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(base::Contains(service_worker_object_hosts_, version_id));
  service_worker_object_hosts_.erase(version_id);
}

bool ServiceWorkerContainerHost::IsContainerForServiceWorker() const {
  return type_ == blink::mojom::ServiceWorkerProviderType::kForServiceWorker;
}

bool ServiceWorkerContainerHost::IsContainerForClient() const {
  switch (type_) {
    case blink::mojom::ServiceWorkerProviderType::kForWindow:
    case blink::mojom::ServiceWorkerProviderType::kForDedicatedWorker:
    case blink::mojom::ServiceWorkerProviderType::kForSharedWorker:
      return true;
    case blink::mojom::ServiceWorkerProviderType::kForServiceWorker:
      return false;
    case blink::mojom::ServiceWorkerProviderType::kUnknown:
      break;
  }
  NOTREACHED() << type_;
  return false;
}

blink::mojom::ServiceWorkerClientType ServiceWorkerContainerHost::client_type()
    const {
  switch (type_) {
    case blink::mojom::ServiceWorkerProviderType::kForWindow:
      return blink::mojom::ServiceWorkerClientType::kWindow;
    case blink::mojom::ServiceWorkerProviderType::kForDedicatedWorker:
      return blink::mojom::ServiceWorkerClientType::kDedicatedWorker;
    case blink::mojom::ServiceWorkerProviderType::kForSharedWorker:
      return blink::mojom::ServiceWorkerClientType::kSharedWorker;
    case blink::mojom::ServiceWorkerProviderType::kForServiceWorker:
    case blink::mojom::ServiceWorkerProviderType::kUnknown:
      break;
  }
  NOTREACHED() << type_;
  return blink::mojom::ServiceWorkerClientType::kWindow;
}

void ServiceWorkerContainerHost::OnBeginNavigationCommit(
    int container_process_id,
    int container_frame_id,
    network::mojom::CrossOriginEmbedderPolicy cross_origin_embedder_policy) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK_EQ(blink::mojom::ServiceWorkerProviderType::kForWindow, type());

  SetContainerProcessId(container_process_id);

  DCHECK_EQ(MSG_ROUTING_NONE, frame_id_);
  DCHECK_NE(MSG_ROUTING_NONE, container_frame_id);
  frame_id_ = container_frame_id;

  DCHECK(!cross_origin_embedder_policy_.has_value());
  cross_origin_embedder_policy_ = cross_origin_embedder_policy;
  if (controller_ && controller_->fetch_handler_existence() ==
                         ServiceWorkerVersion::FetchHandlerExistence::EXISTS) {
    DCHECK(pending_controller_receiver_);
    controller_->controller()->Clone(std::move(pending_controller_receiver_),
                                     cross_origin_embedder_policy_.value());
  }

  if (IsBackForwardCacheEnabled() &&
      ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
    auto* rfh = RenderFrameHostImpl::FromID(process_id_, frame_id_);
    // |rfh| may be null in tests (but it should not happen in production).
    if (rfh)
      rfh->AddServiceWorkerContainerHost(this);
  }

  TransitionToClientPhase(ClientPhase::kResponseCommitted);
}

void ServiceWorkerContainerHost::CompleteWebWorkerPreparation(
    network::mojom::CrossOriginEmbedderPolicy cross_origin_embedder_policy) {
  using ServiceWorkerProviderType = blink::mojom::ServiceWorkerProviderType;
  DCHECK(type_ == ServiceWorkerProviderType::kForDedicatedWorker ||
         type_ == ServiceWorkerProviderType::kForSharedWorker);

  DCHECK(!cross_origin_embedder_policy_.has_value());
  cross_origin_embedder_policy_ = cross_origin_embedder_policy;
  if (controller_ && controller_->fetch_handler_existence() ==
                         ServiceWorkerVersion::FetchHandlerExistence::EXISTS) {
    DCHECK(pending_controller_receiver_);
    controller_->controller()->Clone(std::move(pending_controller_receiver_),
                                     cross_origin_embedder_policy_.value());
  }

  TransitionToClientPhase(ClientPhase::kResponseCommitted);
  SetExecutionReady();
}

void ServiceWorkerContainerHost::UpdateUrls(
    const GURL& url,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& top_frame_origin) {
  GURL previous_url = url_;

  DCHECK(!url.has_ref());
  url_ = url;
  site_for_cookies_ = site_for_cookies;
  top_frame_origin_ = top_frame_origin;

  if (previous_url != url) {
    // Revoke the token on URL change since any service worker holding the token
    // may no longer be the potential controller of this frame and shouldn't
    // have the power to display SSL dialogs for it.
    if (type_ == blink::mojom::ServiceWorkerProviderType::kForWindow) {
      auto* registry = FrameTreeNodeIdRegistry::GetInstance();
      registry->Remove(fetch_request_window_id_);
      fetch_request_window_id_ = base::UnguessableToken::Create();
      registry->Add(fetch_request_window_id_, frame_tree_node_id_);
    }
  }

  auto previous_origin = url::Origin::Create(previous_url);
  auto new_origin = url::Origin::Create(url);
  // Update client id on cross origin redirects. This corresponds to the HTML
  // standard's "process a navigation fetch" algorithm's step for discarding
  // |reservedEnvironment|.
  // https://html.spec.whatwg.org/multipage/browsing-the-web.html#process-a-navigate-fetch
  // "If |reservedEnvironment| is not null and |currentURL|'s origin is not the
  // same as |reservedEnvironment|'s creation URL's origin, then:
  //    1. Run the environment discarding steps for |reservedEnvironment|.
  //    2. Set |reservedEnvironment| to null."
  if (previous_url.is_valid() &&
      !new_origin.IsSameOriginWith(previous_origin)) {
    // Remove old controller since we know the controller is definitely
    // changed. We need to remove |this| from |controller_|'s controllee before
    // updating UUID since ServiceWorkerVersion has a map from uuid to provider
    // hosts.
    SetControllerRegistration(nullptr, false /* notify_controllerchange */);

    // Set UUID to the new one.
    if (context_)
      context_->UnregisterProviderHostByClientID(client_uuid_);
    client_uuid_ = base::GenerateGUID();
    if (context_)
      context_->RegisterProviderHostByClientID(client_uuid_, provider_host_);
  }
}

void ServiceWorkerContainerHost::SetControllerRegistration(
    scoped_refptr<ServiceWorkerRegistration> controller_registration,
    bool notify_controllerchange) {
  DCHECK(IsContainerForClient());

  if (controller_registration) {
    CHECK(IsContextSecureForServiceWorker());
    DCHECK(controller_registration->active_version());
#if DCHECK_IS_ON()
    // TODO(https://crbug.com/931087): Enable this check again after
    // IsMatchingRegistration() is moved from ServiceWorkerProviderHost to
    // ServiceWorkerContainerHost.
    // DCHECK(IsMatchingRegistration(controller_registration.get()));
#endif  // DCHECK_IS_ON()
  }

  controller_registration_ = controller_registration;
  UpdateController(notify_controllerchange);
}

mojo::Remote<blink::mojom::ControllerServiceWorker>
ServiceWorkerContainerHost::GetRemoteControllerServiceWorker() {
  DCHECK(controller_);
  if (controller_->fetch_handler_existence() ==
      ServiceWorkerVersion::FetchHandlerExistence::DOES_NOT_EXIST) {
    return mojo::Remote<blink::mojom::ControllerServiceWorker>();
  }

  mojo::Remote<blink::mojom::ControllerServiceWorker> remote_controller;
  if (!is_response_committed()) {
    // The receiver will be connected to the controller in
    // OnBeginNavigationCommit() or CompleteWebWorkerPreparation(). The pair of
    // Mojo endpoints is created on each main resource response including
    // redirect. The final Mojo endpoint which is corresponding to the OK
    // response will be sent to the service worker.
    pending_controller_receiver_ =
        remote_controller.BindNewPipeAndPassReceiver();
  } else {
    controller_->controller()->Clone(
        remote_controller.BindNewPipeAndPassReceiver(),
        cross_origin_embedder_policy_.value());
  }
  return remote_controller;
}

bool ServiceWorkerContainerHost::AllowServiceWorker(const GURL& scope,
                                                    const GURL& script_url) {
  DCHECK(context_);
  if (ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
    return GetContentClient()->browser()->AllowServiceWorkerOnUI(
        scope, site_for_cookies(), top_frame_origin(), script_url,
        context_->wrapper()->browser_context(),
        base::BindRepeating(&WebContentsImpl::FromRenderFrameHostID,
                            process_id_, frame_id_));
  } else {
    return GetContentClient()->browser()->AllowServiceWorkerOnIO(
        scope, site_for_cookies(), top_frame_origin(), script_url,
        context_->wrapper()->resource_context(),
        base::BindRepeating(&WebContentsImpl::FromRenderFrameHostID,
                            process_id_, frame_id_));
  }
}

bool ServiceWorkerContainerHost::IsContextSecureForServiceWorker() const {
  DCHECK(IsContainerForClient());

  if (!url_.is_valid())
    return false;
  if (!OriginCanAccessServiceWorkers(url_))
    return false;

  if (is_parent_frame_secure_)
    return true;

  std::set<std::string> schemes;
  GetContentClient()->browser()->GetSchemesBypassingSecureContextCheckWhitelist(
      &schemes);
  return schemes.find(url_.scheme()) != schemes.end();
}

bool ServiceWorkerContainerHost::is_response_committed() const {
  DCHECK(IsContainerForClient());
  switch (client_phase_) {
    case ClientPhase::kInitial:
      return false;
    case ClientPhase::kResponseCommitted:
    case ClientPhase::kExecutionReady:
      return true;
  }
  NOTREACHED();
  return false;
}

void ServiceWorkerContainerHost::AddExecutionReadyCallback(
    ExecutionReadyCallback callback) {
  DCHECK(!is_execution_ready());
  execution_ready_callbacks_.push_back(std::move(callback));
}

bool ServiceWorkerContainerHost::is_execution_ready() const {
  DCHECK(IsContainerForClient());
  return client_phase_ == ClientPhase::kExecutionReady;
}

void ServiceWorkerContainerHost::SetExecutionReady() {
  DCHECK(!is_execution_ready());
  TransitionToClientPhase(ClientPhase::kExecutionReady);
  RunExecutionReadyCallbacks();
}

void ServiceWorkerContainerHost::TransitionToClientPhase(
    ClientPhase new_phase) {
  if (client_phase_ == new_phase)
    return;
  switch (client_phase_) {
    case ClientPhase::kInitial:
      DCHECK_EQ(new_phase, ClientPhase::kResponseCommitted);
      break;
    case ClientPhase::kResponseCommitted:
      DCHECK_EQ(new_phase, ClientPhase::kExecutionReady);
      break;
    case ClientPhase::kExecutionReady:
      NOTREACHED();
      break;
  }
  client_phase_ = new_phase;
}

bool ServiceWorkerContainerHost::IsControllerDecided() const {
  DCHECK(IsContainerForClient());

  if (is_execution_ready())
    return true;

  // TODO(falken): This function just becomes |is_execution_ready()|
  // when NetworkService is enabled, so remove/simplify it when
  // non-NetworkService code is removed.

  switch (client_type()) {
    case blink::mojom::ServiceWorkerClientType::kWindow:
      // |this| is hosting a reserved client undergoing navigation. Don't send
      // the controller since it can be changed again before the final
      // response. The controller will be sent on navigation commit. See
      // CommitNavigation in frame.mojom.
      return false;
    case blink::mojom::ServiceWorkerClientType::kDedicatedWorker:
    case blink::mojom::ServiceWorkerClientType::kSharedWorker:
      // When PlzWorker is enabled, the controller will be sent when the
      // response is committed to the renderer.
      return false;
    case blink::mojom::ServiceWorkerClientType::kAll:
      NOTREACHED();
  }

  NOTREACHED();
  return true;
}

void ServiceWorkerContainerHost::SetContainerProcessId(
    int container_process_id) {
  DCHECK_EQ(ChildProcessHost::kInvalidUniqueID, process_id_);
  DCHECK_NE(ChildProcessHost::kInvalidUniqueID, container_process_id);
  process_id_ = container_process_id;
  if (controller_)
    controller_->UpdateForegroundPriority();
}

blink::mojom::ControllerServiceWorkerMode
ServiceWorkerContainerHost::GetControllerMode() const {
  DCHECK(IsContainerForClient());
  if (!controller_)
    return blink::mojom::ControllerServiceWorkerMode::kNoController;
  switch (controller_->fetch_handler_existence()) {
    case ServiceWorkerVersion::FetchHandlerExistence::DOES_NOT_EXIST:
      return blink::mojom::ControllerServiceWorkerMode::kNoFetchEventHandler;
    case ServiceWorkerVersion::FetchHandlerExistence::EXISTS:
      return blink::mojom::ControllerServiceWorkerMode::kControlled;
    case ServiceWorkerVersion::FetchHandlerExistence::UNKNOWN:
      // UNKNOWN means the controller is still installing. It's not possible to
      // have a controller that hasn't finished installing.
      NOTREACHED();
  }
  NOTREACHED();
  return blink::mojom::ControllerServiceWorkerMode::kNoController;
}

ServiceWorkerVersion* ServiceWorkerContainerHost::controller() const {
#if DCHECK_IS_ON()
  CheckControllerConsistency(false);
#endif  // DCHECK_IS_ON()
  return controller_.get();
}

ServiceWorkerRegistration* ServiceWorkerContainerHost::controller_registration()
    const {
#if DCHECK_IS_ON()
  CheckControllerConsistency(false);
#endif  // DCHECK_IS_ON()
  return controller_registration_.get();
}

bool ServiceWorkerContainerHost::IsInBackForwardCache() const {
  DCHECK(ServiceWorkerContext::IsServiceWorkerOnUIEnabled());
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return is_in_back_forward_cache_;
}

void ServiceWorkerContainerHost::EvictFromBackForwardCache(
    BackForwardCacheMetrics::NotRestoredReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsBackForwardCacheEnabled());
  DCHECK_EQ(type_, blink::mojom::ServiceWorkerProviderType::kForWindow);
  is_in_back_forward_cache_ = false;
  auto* rfh = RenderFrameHostImpl::FromID(process_id_, frame_id_);
  // |rfh| could be evicted before this function is called.
  if (!rfh || !rfh->is_in_back_forward_cache())
    return;
  rfh->EvictFromBackForwardCacheWithReason(reason);
}

void ServiceWorkerContainerHost::OnEnterBackForwardCache() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsBackForwardCacheEnabled());
  DCHECK_EQ(type_, blink::mojom::ServiceWorkerProviderType::kForWindow);
  if (controller_)
    controller_->MoveControlleeToBackForwardCacheMap(client_uuid_);
  is_in_back_forward_cache_ = true;
}

void ServiceWorkerContainerHost::OnRestoreFromBackForwardCache() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsBackForwardCacheEnabled());
  DCHECK_EQ(type_, blink::mojom::ServiceWorkerProviderType::kForWindow);
  if (controller_)
    controller_->RestoreControlleeFromBackForwardCacheMap(client_uuid_);
  is_in_back_forward_cache_ = false;
}

void ServiceWorkerContainerHost::RunExecutionReadyCallbacks() {
  std::vector<ExecutionReadyCallback> callbacks;
  execution_ready_callbacks_.swap(callbacks);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&RunCallbacks, std::move(callbacks)));
}

void ServiceWorkerContainerHost::UpdateController(
    bool notify_controllerchange) {
  ServiceWorkerVersion* version =
      controller_registration_ ? controller_registration_->active_version()
                               : nullptr;
  CHECK(!version || IsContextSecureForServiceWorker());
  if (version == controller_.get())
    return;

  scoped_refptr<ServiceWorkerVersion> previous_version = controller_;
  controller_ = version;

  if (version)
    version->AddControllee(this);
  if (previous_version)
    previous_version->RemoveControllee(client_uuid_);

  // SetController message should be sent only for clients.
  DCHECK(IsContainerForClient());

  if (!IsControllerDecided())
    return;

  SendSetControllerServiceWorker(notify_controllerchange);
}

#if DCHECK_IS_ON()
void ServiceWorkerContainerHost::CheckControllerConsistency(
    bool should_crash) const {
  if (!controller_) {
    DCHECK(!controller_registration_);
    return;
  }

  DCHECK(IsContainerForClient());
  DCHECK(controller_registration_);
  DCHECK_EQ(controller_->registration_id(), controller_registration_->id());

  switch (controller_->status()) {
    case ServiceWorkerVersion::NEW:
    case ServiceWorkerVersion::INSTALLING:
    case ServiceWorkerVersion::INSTALLED:
      if (should_crash) {
        ServiceWorkerVersion::Status status = controller_->status();
        base::debug::Alias(&status);
        CHECK(false) << "Controller service worker has a bad status: "
                     << ServiceWorkerVersion::VersionStatusToString(status);
      }
      break;
    case ServiceWorkerVersion::REDUNDANT: {
      if (should_crash) {
        DEBUG_ALIAS_FOR_CSTR(
            redundant_callstack_str,
            controller_->redundant_state_callstack().ToString().c_str(), 1024);
        CHECK(false);
      }
      break;
    }
    case ServiceWorkerVersion::ACTIVATING:
    case ServiceWorkerVersion::ACTIVATED:
      // Valid status, controller is being activated.
      break;
  }
}
#endif  // DCHECK_IS_ON()

void ServiceWorkerContainerHost::StartControllerComplete(
    mojo::PendingReceiver<blink::mojom::ControllerServiceWorker> receiver,
    blink::ServiceWorkerStatusCode status) {
  if (status == blink::ServiceWorkerStatusCode::kOk) {
    DCHECK(is_response_committed());
    controller_->controller()->Clone(std::move(receiver),
                                     cross_origin_embedder_policy_.value());
  }
}

}  // namespace content
