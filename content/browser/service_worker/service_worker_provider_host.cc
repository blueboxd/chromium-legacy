// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_provider_host.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/debug/alias.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "content/browser/bad_message.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/interface_provider_filtering.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/renderer_interface_binders.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_controllee_request_handler.h"
#include "content/browser/service_worker/service_worker_registration_object_host.h"
#include "content/browser/service_worker/service_worker_type_converters.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webtransport/quic_transport_connector_impl.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/origin_util.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/message.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

namespace {

// This function provides the next ServiceWorkerProviderHost ID.
int NextProviderId() {
  static int g_next_provider_id = 0;
  return g_next_provider_id++;
}

void GetInterfaceImpl(const std::string& interface_name,
                      mojo::ScopedMessagePipeHandle interface_pipe,
                      const url::Origin& origin,
                      int process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* process = RenderProcessHost::FromID(process_id);
  if (!process)
    return;

  // RestrictedCookieManager creation is different between frames and service
  // workers, so it's handled here.
  if (interface_name == network::mojom::RestrictedCookieManager::Name_) {
    mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver(
        std::move(interface_pipe));
    process->GetStoragePartition()->CreateRestrictedCookieManager(
        network::mojom::RestrictedCookieManagerRole::SCRIPT, origin,
        origin.GetURL(), origin, true /* is_service_worker */, process_id,
        MSG_ROUTING_NONE, std::move(receiver));
    return;
  }

  BindWorkerInterface(interface_name, std::move(interface_pipe), process,
                      origin);
}

void CreateQuicTransportConnectorImpl(
    int process_id,
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::QuicTransportConnector> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* process = RenderProcessHost::FromID(process_id);
  if (!process)
    return;

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<QuicTransportConnectorImpl>(
          process_id, origin, net::NetworkIsolationKey(origin, origin)),
      std::move(receiver));
}

}  // anonymous namespace

// RAII helper class for keeping track of versions waiting for an update hint
// from the renderer.
//
// This class is move-only.
class ServiceWorkerProviderHost::PendingUpdateVersion {
 public:
  explicit PendingUpdateVersion(scoped_refptr<ServiceWorkerVersion> version)
      : version_(std::move(version)) {
    version_->IncrementPendingUpdateHintCount();
  }

  PendingUpdateVersion(PendingUpdateVersion&& other) {
    version_ = std::move(other.version_);
  }

  ~PendingUpdateVersion() {
    if (version_)
      version_->DecrementPendingUpdateHintCount();
  }

  PendingUpdateVersion& operator=(PendingUpdateVersion&& other) {
    version_ = std::move(other.version_);
    return *this;
  }

  // Needed for base::flat_set.
  bool operator<(const PendingUpdateVersion& other) const {
    return version_ < other.version_;
  }

 private:
  scoped_refptr<ServiceWorkerVersion> version_;
  DISALLOW_COPY_AND_ASSIGN(PendingUpdateVersion);
};

// static
base::WeakPtr<ServiceWorkerProviderHost>
ServiceWorkerProviderHost::PreCreateNavigationHost(
    base::WeakPtr<ServiceWorkerContextCore> context,
    bool are_ancestors_secure,
    int frame_tree_node_id,
    mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
        host_receiver,
    mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
        container_remote) {
  DCHECK(context);
  auto host = base::WrapUnique(new ServiceWorkerProviderHost(
      blink::mojom::ServiceWorkerProviderType::kForWindow, are_ancestors_secure,
      frame_tree_node_id, std::move(host_receiver), std::move(container_remote),
      /*running_hosted_version=*/nullptr, context));
  auto weak_ptr = host->AsWeakPtr();
  RegisterToContextCore(context, std::move(host));
  return weak_ptr;
}

// static
base::WeakPtr<ServiceWorkerProviderHost>
ServiceWorkerProviderHost::CreateForServiceWorker(
    base::WeakPtr<ServiceWorkerContextCore> context,
    scoped_refptr<ServiceWorkerVersion> version,
    blink::mojom::ServiceWorkerProviderInfoForStartWorkerPtr*
        out_provider_info) {
  auto host = base::WrapUnique(new ServiceWorkerProviderHost(
      blink::mojom::ServiceWorkerProviderType::kForServiceWorker,
      /*is_parent_frame_secure=*/true, FrameTreeNode::kFrameTreeNodeInvalidId,
      (*out_provider_info)->host_remote.InitWithNewEndpointAndPassReceiver(),
      /*container_remote=*/mojo::NullAssociatedRemote(), std::move(version),
      context));
  auto weak_ptr = host->AsWeakPtr();
  RegisterToContextCore(context, std::move(host));
  return weak_ptr;
}

// static
base::WeakPtr<ServiceWorkerProviderHost>
ServiceWorkerProviderHost::CreateForWebWorker(
    base::WeakPtr<ServiceWorkerContextCore> context,
    int process_id,
    blink::mojom::ServiceWorkerProviderType provider_type,
    mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
        host_receiver,
    mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
        container_remote) {
  using ServiceWorkerProviderType = blink::mojom::ServiceWorkerProviderType;
  DCHECK((base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker) &&
          provider_type == ServiceWorkerProviderType::kForDedicatedWorker) ||
         provider_type == ServiceWorkerProviderType::kForSharedWorker);
  auto host = base::WrapUnique(new ServiceWorkerProviderHost(
      provider_type, /*is_parent_frame_secure=*/true,
      FrameTreeNode::kFrameTreeNodeInvalidId, std::move(host_receiver),
      std::move(container_remote), /*running_hosted_version=*/nullptr,
      context));
  host->container_host()->SetContainerProcessId(process_id);

  auto weak_ptr = host->AsWeakPtr();
  RegisterToContextCore(context, std::move(host));
  return weak_ptr;
}

// static
void ServiceWorkerProviderHost::RegisterToContextCore(
    base::WeakPtr<ServiceWorkerContextCore> context,
    std::unique_ptr<ServiceWorkerProviderHost> host) {
  DCHECK(host->receiver_.is_bound());
  host->receiver_.set_disconnect_handler(
      base::BindOnce(&ServiceWorkerContextCore::RemoveProviderHost, context,
                     host->provider_id()));
  context->AddProviderHost(std::move(host));
}

ServiceWorkerProviderHost::ServiceWorkerProviderHost(
    blink::mojom::ServiceWorkerProviderType type,
    bool is_parent_frame_secure,
    int frame_tree_node_id,
    mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
        host_receiver,
    mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
        container_remote,
    scoped_refptr<ServiceWorkerVersion> running_hosted_version,
    base::WeakPtr<ServiceWorkerContextCore> context)
    : provider_id_(NextProviderId()),
      running_hosted_version_(std::move(running_hosted_version)),
      context_(context),
      interface_provider_binding_(this),
      container_host_(std::make_unique<content::ServiceWorkerContainerHost>(
          type,
          is_parent_frame_secure,
          frame_tree_node_id,
          std::move(container_remote),
          this,
          context)) {
  DCHECK_NE(blink::mojom::ServiceWorkerProviderType::kUnknown, type);

  if (type == blink::mojom::ServiceWorkerProviderType::kForServiceWorker) {
    DCHECK(running_hosted_version_);
    container_host_->UpdateUrls(running_hosted_version_->script_url(),
                                running_hosted_version_->script_url(),
                                running_hosted_version_->script_origin());
  }
  // For service worker clients, ServiceWorkerProviderHost::UpdateUrls() will
  // be called later.

  context_->RegisterProviderHostByClientID(container_host_->client_uuid(),
                                           this);

  DCHECK(host_receiver.is_valid());
  receiver_.Bind(std::move(host_receiver));
}

ServiceWorkerProviderHost::~ServiceWorkerProviderHost() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  if (context_)
    context_->UnregisterProviderHostByClientID(container_host_->client_uuid());

  // Explicitly destroy the ServiceWorkerContainerHost to release
  // ServiceWorkerObjectHosts and ServiceWorkerRegistrationObjectHosts owned by
  // that. Otherwise, this destructor can trigger their Mojo connection error
  // handlers, which would call back into halfway destroyed |this|. This is
  // because they are associated with the ServiceWorker interface, which can be
  // destroyed while in this destructor (|running_hosted_version_|'s
  // |event_dispatcher_|). See https://crbug.com/854993.
  container_host_.reset();

  RemoveAllMatchingRegistrations();
}

void ServiceWorkerProviderHost::OnVersionAttributesChanged(
    ServiceWorkerRegistration* registration,
    blink::mojom::ChangedServiceWorkerObjectsMaskPtr changed_mask,
    const ServiceWorkerRegistrationInfo& /* info */) {
  if (!get_ready_callback_ || get_ready_callback_->is_null())
    return;
  if (changed_mask->active && registration->active_version()) {
    // Wait until the state change so we don't send the get for ready
    // registration complete message before set version attributes message.
    registration->active_version()->RegisterStatusChangeCallback(base::BindOnce(
        &ServiceWorkerProviderHost::ReturnRegistrationForReadyIfNeeded,
        AsWeakPtr()));
  }
}

void ServiceWorkerProviderHost::OnRegistrationFailed(
    ServiceWorkerRegistration* registration) {
  RemoveMatchingRegistration(registration);
}

void ServiceWorkerProviderHost::OnRegistrationFinishedUninstalling(
    ServiceWorkerRegistration* registration) {
  RemoveMatchingRegistration(registration);
}

void ServiceWorkerProviderHost::OnSkippedWaiting(
    ServiceWorkerRegistration* registration) {
  container_host_->OnSkippedWaiting(registration);
}

ServiceWorkerVersion* ServiceWorkerProviderHost::running_hosted_version()
    const {
  DCHECK(!running_hosted_version_ ||
         container_host_->IsContainerForServiceWorker());
  return running_hosted_version_.get();
}

void ServiceWorkerProviderHost::UpdateUrls(
    const GURL& url,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& top_frame_origin) {
  DCHECK(IsProviderForClient());

  container_host_->UpdateUrls(url, site_for_cookies, top_frame_origin);
  // TODO(https://crbug.com/931087): Move the remaining part to
  // ServiceWorkerContainerHost::UpdateUrls().

  SyncMatchingRegistrations();
}

blink::mojom::ServiceWorkerProviderType
ServiceWorkerProviderHost::provider_type() const {
  return container_host_->type();
}

bool ServiceWorkerProviderHost::IsProviderForServiceWorker() const {
  return container_host_->IsContainerForServiceWorker();
}

bool ServiceWorkerProviderHost::IsProviderForClient() const {
  return container_host_->IsContainerForClient();
}

void ServiceWorkerProviderHost::AddMatchingRegistration(
    ServiceWorkerRegistration* registration) {
  DCHECK(ServiceWorkerUtils::ScopeMatches(registration->scope(),
                                          container_host_->url()));
  if (!container_host_->IsContextSecureForServiceWorker())
    return;
  size_t key = registration->scope().spec().size();
  if (base::Contains(matching_registrations_, key))
    return;
  registration->AddListener(this);
  matching_registrations_[key] = registration;
  ReturnRegistrationForReadyIfNeeded();
}

void ServiceWorkerProviderHost::RemoveMatchingRegistration(
    ServiceWorkerRegistration* registration) {
  DCHECK_NE(container_host_->controller_registration(), registration);
#if DCHECK_IS_ON()
  DCHECK(IsMatchingRegistration(registration));
#endif  // DCHECK_IS_ON()

  registration->RemoveListener(this);
  size_t key = registration->scope().spec().size();
  matching_registrations_.erase(key);
}

ServiceWorkerRegistration* ServiceWorkerProviderHost::MatchRegistration()
    const {
  auto it = matching_registrations_.rbegin();
  for (; it != matching_registrations_.rend(); ++it) {
    if (it->second->is_uninstalled())
      continue;
    if (it->second->is_uninstalling())
      return nullptr;
    return it->second.get();
  }
  return nullptr;
}

void ServiceWorkerProviderHost::AddServiceWorkerToUpdate(
    scoped_refptr<ServiceWorkerVersion> version) {
  // This is only called for windows now, but it should be called for all
  // clients someday.
  DCHECK_EQ(provider_type(),
            blink::mojom::ServiceWorkerProviderType::kForWindow);

  versions_to_update_.emplace(std::move(version));
}

void ServiceWorkerProviderHost::CompleteStartWorkerPreparation(
    int process_id,
    mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
        interface_provider_receiver,
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
        broker_receiver) {
  DCHECK(context_);
  DCHECK_EQ(ChildProcessHost::kInvalidUniqueID, worker_process_id_);
  DCHECK_NE(ChildProcessHost::kInvalidUniqueID, process_id);
  DCHECK(IsProviderForServiceWorker());
  SetWorkerProcessId(process_id);

  interface_provider_binding_.Bind(FilterRendererExposedInterfaces(
      blink::mojom::kNavigation_ServiceWorkerSpec, process_id,
      std::move(interface_provider_receiver)));

  broker_receiver_.Bind(std::move(broker_receiver));
}

void ServiceWorkerProviderHost::SyncMatchingRegistrations() {
  DCHECK(!container_host_->controller_registration());

  RemoveAllMatchingRegistrations();
  if (!context_)
    return;
  const auto& registrations = context_->GetLiveRegistrations();
  for (const auto& key_registration : registrations) {
    ServiceWorkerRegistration* registration = key_registration.second;
    if (!registration->is_uninstalled() &&
        ServiceWorkerUtils::ScopeMatches(registration->scope(),
                                         container_host_->url())) {
      AddMatchingRegistration(registration);
    }
  }
}

#if DCHECK_IS_ON()
bool ServiceWorkerProviderHost::IsMatchingRegistration(
    ServiceWorkerRegistration* registration) const {
  std::string spec = registration->scope().spec();
  size_t key = spec.size();

  auto iter = matching_registrations_.find(key);
  if (iter == matching_registrations_.end())
    return false;
  if (iter->second.get() != registration)
    return false;
  return true;
}
#endif  // DCHECK_IS_ON()

void ServiceWorkerProviderHost::RemoveAllMatchingRegistrations() {
  DCHECK(!container_host_ || !container_host_->controller_registration());
  for (const auto& it : matching_registrations_) {
    ServiceWorkerRegistration* registration = it.second.get();
    registration->RemoveListener(this);
  }
  matching_registrations_.clear();
}

void ServiceWorkerProviderHost::ReturnRegistrationForReadyIfNeeded() {
  if (!get_ready_callback_ || get_ready_callback_->is_null())
    return;
  ServiceWorkerRegistration* registration = MatchRegistration();
  if (!registration || !registration->active_version())
    return;
  TRACE_EVENT_ASYNC_END1("ServiceWorker",
                         "ServiceWorkerProviderHost::GetRegistrationForReady",
                         this, "Registration ID", registration->id());
  if (!context_) {
    // Here no need to run or destroy |get_ready_callback_|, which will destroy
    // together with |receiver_| when |this| destroys.
    return;
  }

  std::move(*get_ready_callback_)
      .Run(container_host_->CreateServiceWorkerRegistrationObjectInfo(
          scoped_refptr<ServiceWorkerRegistration>(registration)));
}

void ServiceWorkerProviderHost::Register(
    const GURL& script_url,
    blink::mojom::ServiceWorkerRegistrationOptionsPtr options,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    RegisterCallback callback) {
  if (!CanServeContainerHostMethods(
          &callback, options->scope, script_url,
          base::StringPrintf(
              ServiceWorkerConsts::kServiceWorkerRegisterErrorPrefix,
              options->scope.spec().c_str(), script_url.spec().c_str())
              .c_str(),
          nullptr)) {
    return;
  }
  if (container_host_->client_type() !=
      blink::mojom::ServiceWorkerClientType::kWindow) {
    mojo::ReportBadMessage(ServiceWorkerConsts::kBadMessageFromNonWindow);
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kUnknown,
                            std::string(), nullptr);
    return;
  }
  std::vector<GURL> urls = {container_host_->url(), options->scope, script_url};
  if (!ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(urls)) {
    mojo::ReportBadMessage(ServiceWorkerConsts::kBadMessageImproperOrigins);
    // ReportBadMessage() will kill the renderer process, but Mojo complains if
    // the callback is not run. Just run it with nonsense arguments.
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kUnknown,
                            std::string(), nullptr);
    return;
  }
  int64_t trace_id = base::TimeTicks::Now().since_origin().InMicroseconds();
  TRACE_EVENT_ASYNC_BEGIN2(
      "ServiceWorker", "ServiceWorkerProviderHost::Register", trace_id, "Scope",
      options->scope.spec(), "Script URL", script_url.spec());

  // Wrap the callback with default invoke before passing it, since
  // RegisterServiceWorker() can drop the callback on service worker
  // context core shutdown (i.e., browser session shutdown or
  // DeleteAndStartOver()) and a DCHECK would happen.
  // TODO(crbug.com/1002776): Remove this wrapper and have the Mojo connections
  // drop during shutdown, so the callback can be dropped without crash. Note
  // that we currently would need to add this WrapCallback to *ALL* Mojo
  // callbacks that go through ServiceWorkerContextCore or its members like
  // ServiceWorkerStorage. We're only adding it to Register() now because a
  // browser test fails without it.
  auto wrapped_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), blink::mojom::ServiceWorkerErrorType::kUnknown,
      std::string(), nullptr);
  context_->RegisterServiceWorker(
      script_url, *options, std::move(outside_fetch_client_settings_object),
      base::BindOnce(&ServiceWorkerProviderHost::RegistrationComplete,
                     AsWeakPtr(), GURL(script_url), GURL(options->scope),
                     std::move(wrapped_callback), trace_id,
                     mojo::GetBadMessageCallback()));
}

void ServiceWorkerProviderHost::RegistrationComplete(
    const GURL& script_url,
    const GURL& scope,
    RegisterCallback callback,
    int64_t trace_id,
    mojo::ReportBadMessageCallback bad_message_callback,
    blink::ServiceWorkerStatusCode status,
    const std::string& status_message,
    int64_t registration_id) {
  TRACE_EVENT_ASYNC_END2("ServiceWorker", "ServiceWorkerProviderHost::Register",
                         trace_id, "Status",
                         blink::ServiceWorkerStatusToString(status),
                         "Registration ID", registration_id);
  // kErrorInvalidArguments means the renderer gave unexpectedly bad arguments,
  // so terminate it.
  if (status == blink::ServiceWorkerStatusCode::kErrorInvalidArguments) {
    std::move(bad_message_callback).Run(status_message);
    // |bad_message_callback| will kill the renderer process, but Mojo complains
    // if the callback is not run. Just run it with nonsense arguments.
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kUnknown,
                            std::string(), nullptr);
    return;
  }
  if (!context_) {
    std::move(callback).Run(
        blink::mojom::ServiceWorkerErrorType::kAbort,
        base::StringPrintf(
            ServiceWorkerConsts::kServiceWorkerRegisterErrorPrefix,
            scope.spec().c_str(), script_url.spec().c_str()) +
            std::string(ServiceWorkerConsts::kShutdownErrorMessage),
        nullptr);
    return;
  }

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::string error_message;
    blink::mojom::ServiceWorkerErrorType error_type;
    GetServiceWorkerErrorTypeForRegistration(status, status_message,
                                             &error_type, &error_message);
    std::move(callback).Run(
        error_type,
        base::StringPrintf(
            ServiceWorkerConsts::kServiceWorkerRegisterErrorPrefix,
            scope.spec().c_str(), script_url.spec().c_str()) +
            error_message,
        nullptr);
    return;
  }

  ServiceWorkerRegistration* registration =
      context_->GetLiveRegistration(registration_id);
  // ServiceWorkerRegisterJob calls its completion callback, which results in
  // this function being called, while the registration is live.
  DCHECK(registration);

  std::move(callback).Run(
      blink::mojom::ServiceWorkerErrorType::kNone, base::nullopt,
      container_host_->CreateServiceWorkerRegistrationObjectInfo(
          scoped_refptr<ServiceWorkerRegistration>(registration)));
}

void ServiceWorkerProviderHost::GetRegistration(
    const GURL& client_url,
    GetRegistrationCallback callback) {
  if (!CanServeContainerHostMethods(
          &callback, container_host_->url(), GURL(),
          ServiceWorkerConsts::kServiceWorkerGetRegistrationErrorPrefix,
          nullptr)) {
    return;
  }

  std::string error_message;
  if (!IsValidGetRegistrationMessage(client_url, &error_message)) {
    mojo::ReportBadMessage(error_message);
    // ReportBadMessage() will kill the renderer process, but Mojo complains if
    // the callback is not run. Just run it with nonsense arguments.
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kUnknown,
                            std::string(), nullptr);
    return;
  }

  int64_t trace_id = base::TimeTicks::Now().since_origin().InMicroseconds();
  TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker",
                           "ServiceWorkerProviderHost::GetRegistration",
                           trace_id, "Client URL", client_url.spec());
  context_->storage()->FindRegistrationForClientUrl(
      client_url, base::AdaptCallbackForRepeating(base::BindOnce(
                      &ServiceWorkerProviderHost::GetRegistrationComplete,
                      AsWeakPtr(), std::move(callback), trace_id)));
}

void ServiceWorkerProviderHost::GetRegistrations(
    GetRegistrationsCallback callback) {
  if (!CanServeContainerHostMethods(
          &callback, container_host_->url(), GURL(),
          ServiceWorkerConsts::kServiceWorkerGetRegistrationsErrorPrefix,
          base::nullopt)) {
    return;
  }

  std::string error_message;
  if (!IsValidGetRegistrationsMessage(&error_message)) {
    mojo::ReportBadMessage(error_message);
    // ReportBadMessage() will kill the renderer process, but Mojo complains if
    // the callback is not run. Just run it with nonsense arguments.
    std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kUnknown,
                            std::string(), base::nullopt);
    return;
  }

  int64_t trace_id = base::TimeTicks::Now().since_origin().InMicroseconds();
  TRACE_EVENT_ASYNC_BEGIN0(
      "ServiceWorker", "ServiceWorkerProviderHost::GetRegistrations", trace_id);
  context_->storage()->GetRegistrationsForOrigin(
      container_host_->url().GetOrigin(),
      base::AdaptCallbackForRepeating(
          base::BindOnce(&ServiceWorkerProviderHost::GetRegistrationsComplete,
                         AsWeakPtr(), std::move(callback), trace_id)));
}

void ServiceWorkerProviderHost::GetRegistrationComplete(
    GetRegistrationCallback callback,
    int64_t trace_id,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  TRACE_EVENT_ASYNC_END2(
      "ServiceWorker", "ServiceWorkerProviderHost::GetRegistration", trace_id,
      "Status", blink::ServiceWorkerStatusToString(status), "Registration ID",
      registration ? registration->id()
                   : blink::mojom::kInvalidServiceWorkerRegistrationId);
  if (!context_) {
    std::move(callback).Run(
        blink::mojom::ServiceWorkerErrorType::kAbort,
        std::string(
            ServiceWorkerConsts::kServiceWorkerGetRegistrationErrorPrefix) +
            std::string(ServiceWorkerConsts::kShutdownErrorMessage),
        nullptr);
    return;
  }

  if (status != blink::ServiceWorkerStatusCode::kOk &&
      status != blink::ServiceWorkerStatusCode::kErrorNotFound) {
    std::string error_message;
    blink::mojom::ServiceWorkerErrorType error_type;
    GetServiceWorkerErrorTypeForRegistration(status, std::string(), &error_type,
                                             &error_message);
    std::move(callback).Run(
        error_type,
        ServiceWorkerConsts::kServiceWorkerGetRegistrationErrorPrefix +
            error_message,
        nullptr);
    return;
  }

  DCHECK(status != blink::ServiceWorkerStatusCode::kOk || registration);
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info;
  if (status == blink::ServiceWorkerStatusCode::kOk &&
      !registration->is_uninstalling()) {
    info = container_host_->CreateServiceWorkerRegistrationObjectInfo(
        std::move(registration));
  }

  std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                          base::nullopt, std::move(info));
}

void ServiceWorkerProviderHost::GetRegistrationsComplete(
    GetRegistrationsCallback callback,
    int64_t trace_id,
    blink::ServiceWorkerStatusCode status,
    const std::vector<scoped_refptr<ServiceWorkerRegistration>>&
        registrations) {
  TRACE_EVENT_ASYNC_END1(
      "ServiceWorker", "ServiceWorkerProviderHost::GetRegistrations", trace_id,
      "Status", blink::ServiceWorkerStatusToString(status));
  if (!context_) {
    std::move(callback).Run(
        blink::mojom::ServiceWorkerErrorType::kAbort,
        std::string(
            ServiceWorkerConsts::kServiceWorkerGetRegistrationsErrorPrefix) +
            std::string(ServiceWorkerConsts::kShutdownErrorMessage),
        base::nullopt);
    return;
  }

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::string error_message;
    blink::mojom::ServiceWorkerErrorType error_type;
    GetServiceWorkerErrorTypeForRegistration(status, std::string(), &error_type,
                                             &error_message);
    std::move(callback).Run(
        error_type,
        ServiceWorkerConsts::kServiceWorkerGetRegistrationsErrorPrefix +
            error_message,
        base::nullopt);
    return;
  }

  std::vector<blink::mojom::ServiceWorkerRegistrationObjectInfoPtr>
      object_infos;

  for (const auto& registration : registrations) {
    DCHECK(registration.get());
    if (!registration->is_uninstalling()) {
      object_infos.push_back(
          container_host_->CreateServiceWorkerRegistrationObjectInfo(
              std::move(registration)));
    }
  }

  std::move(callback).Run(blink::mojom::ServiceWorkerErrorType::kNone,
                          base::nullopt, std::move(object_infos));
}

void ServiceWorkerProviderHost::GetRegistrationForReady(
    GetRegistrationForReadyCallback callback) {
  std::string error_message;
  if (!IsValidGetRegistrationForReadyMessage(&error_message)) {
    mojo::ReportBadMessage(error_message);
    // ReportBadMessage() will kill the renderer process, but Mojo complains if
    // the callback is not run. Just run it with nonsense arguments.
    std::move(callback).Run(nullptr);
    return;
  }

  TRACE_EVENT_ASYNC_BEGIN0("ServiceWorker",
                           "ServiceWorkerProviderHost::GetRegistrationForReady",
                           this);
  DCHECK(!get_ready_callback_);
  get_ready_callback_ =
      std::make_unique<GetRegistrationForReadyCallback>(std::move(callback));
  ReturnRegistrationForReadyIfNeeded();
}

void ServiceWorkerProviderHost::EnsureControllerServiceWorker(
    mojo::PendingReceiver<blink::mojom::ControllerServiceWorker> receiver,
    blink::mojom::ControllerServiceWorkerPurpose purpose) {
  container_host_->EnsureControllerServiceWorker(std::move(receiver), purpose);
}

void ServiceWorkerProviderHost::CloneContainerHost(
    mojo::PendingReceiver<blink::mojom::ServiceWorkerContainerHost> receiver) {
  additional_receivers_.Add(this, std::move(receiver));
}

void ServiceWorkerProviderHost::HintToUpdateServiceWorker() {
  if (!IsProviderForClient()) {
    mojo::ReportBadMessage("SWPH_HTUSW_NOT_CLIENT");
    return;
  }

  // The destructors notify the ServiceWorkerVersions to update.
  versions_to_update_.clear();
}

void ServiceWorkerProviderHost::OnExecutionReady() {
  if (!IsProviderForClient()) {
    mojo::ReportBadMessage("SWPH_OER_NOT_CLIENT");
    return;
  }

  if (container_host_->is_execution_ready()) {
    mojo::ReportBadMessage("SWPH_OER_ALREADY_READY");
    return;
  }

  // The controller was sent on navigation commit but we must send it again here
  // because 1) the controller might have changed since navigation commit due to
  // skipWaiting(), and 2) the UseCounter might have changed since navigation
  // commit, in such cases the updated information was prevented being sent due
  // to false ServiceWorkerContainerHost::IsControllerDecided().
  // TODO(leonhsl): Create some layout tests covering the above case 1), in
  // which case we may also need to set |notify_controllerchange| correctly.
  container_host_->SendSetControllerServiceWorker(
      false /* notify_controllerchange */);

  container_host_->SetExecutionReady();
}

bool ServiceWorkerProviderHost::IsValidGetRegistrationMessage(
    const GURL& client_url,
    std::string* out_error) const {
  if (container_host_->client_type() !=
      blink::mojom::ServiceWorkerClientType::kWindow) {
    *out_error = ServiceWorkerConsts::kBadMessageFromNonWindow;
    return false;
  }
  if (!client_url.is_valid()) {
    *out_error = ServiceWorkerConsts::kBadMessageInvalidURL;
    return false;
  }
  std::vector<GURL> urls = {container_host_->url(), client_url};
  if (!ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(urls)) {
    *out_error = ServiceWorkerConsts::kBadMessageImproperOrigins;
    return false;
  }

  return true;
}

bool ServiceWorkerProviderHost::IsValidGetRegistrationsMessage(
    std::string* out_error) const {
  if (container_host_->client_type() !=
      blink::mojom::ServiceWorkerClientType::kWindow) {
    *out_error = ServiceWorkerConsts::kBadMessageFromNonWindow;
    return false;
  }
  if (!OriginCanAccessServiceWorkers(container_host_->url())) {
    *out_error = ServiceWorkerConsts::kBadMessageImproperOrigins;
    return false;
  }

  return true;
}

bool ServiceWorkerProviderHost::IsValidGetRegistrationForReadyMessage(
    std::string* out_error) const {
  if (container_host_->client_type() !=
      blink::mojom::ServiceWorkerClientType::kWindow) {
    *out_error = ServiceWorkerConsts::kBadMessageFromNonWindow;
    return false;
  }

  if (get_ready_callback_) {
    *out_error =
        ServiceWorkerConsts::kBadMessageGetRegistrationForReadyDuplicated;
    return false;
  }

  return true;
}

void ServiceWorkerProviderHost::GetInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(IsProviderForServiceWorker());
  RunOrPostTaskOnThread(FROM_HERE, BrowserThread::UI,
                        base::BindOnce(&GetInterfaceImpl, interface_name,
                                       std::move(interface_pipe),
                                       running_hosted_version_->script_origin(),
                                       worker_process_id_));
}

template <typename CallbackType, typename... Args>
bool ServiceWorkerProviderHost::CanServeContainerHostMethods(
    CallbackType* callback,
    const GURL& scope,
    const GURL& script_url,
    const char* error_prefix,
    Args... args) {
  if (!context_) {
    std::move(*callback).Run(
        blink::mojom::ServiceWorkerErrorType::kAbort,
        std::string(error_prefix) +
            std::string(ServiceWorkerConsts::kShutdownErrorMessage),
        args...);
    return false;
  }

  // TODO(falken): This check can be removed once crbug.com/439697 is fixed.
  // (Also see crbug.com/776408)
  if (container_host_->url().is_empty()) {
    std::move(*callback).Run(
        blink::mojom::ServiceWorkerErrorType::kSecurity,
        std::string(error_prefix) +
            std::string(ServiceWorkerConsts::kNoDocumentURLErrorMessage),
        args...);
    return false;
  }

  if (!container_host_->AllowServiceWorker(scope, script_url)) {
    std::move(*callback).Run(
        blink::mojom::ServiceWorkerErrorType::kDisabled,
        std::string(error_prefix) +
            std::string(ServiceWorkerConsts::kUserDeniedPermissionMessage),
        args...);
    return false;
  }

  return true;
}

void ServiceWorkerProviderHost::CreateQuicTransportConnector(
    mojo::PendingReceiver<blink::mojom::QuicTransportConnector> receiver) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(IsProviderForServiceWorker());
  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(&CreateQuicTransportConnectorImpl, worker_process_id_,
                     running_hosted_version_->script_origin(),
                     std::move(receiver)));
}

void ServiceWorkerProviderHost::SetWorkerProcessId(int worker_process_id) {
  DCHECK(IsProviderForServiceWorker());
  worker_process_id_ = worker_process_id;
}

}  // namespace content
