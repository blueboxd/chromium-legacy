// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/browser_state.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/process/process_handle.h"
#include "base/task/post_task.h"
#include "base/token.h"
#include "ios/web/public/init/network_context_owner.h"
#include "ios/web/public/security/certificate_policy_cache.h"
#include "ios/web/public/service/service_manager_connection.h"
#include "ios/web/public/service/service_names.mojom.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "ios/web/public/web_client.h"
#include "ios/web/webui/url_data_manager_ios_backend.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_context_getter_observer.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/mojom/service.mojom.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace {

// Maps service instance group IDs to associated BrowserState instances.
std::map<base::Token, BrowserState*>& GetInstanceGroupToBrowserState() {
  static base::NoDestructor<std::map<base::Token, BrowserState*>>
      instance_group_to_browser_state;
  return *instance_group_to_browser_state;
}

// Private key used for safe conversion of base::SupportsUserData to
// web::BrowserState in web::BrowserState::FromSupportsUserData.
const char kBrowserStateIdentifierKey[] = "BrowserStateIdentifierKey";

// Data key names.
const char kCertificatePolicyCacheKeyName[] = "cert_policy_cache";
const char kServiceManagerConnection[] = "service-manager-connection";
const char kServiceInstanceGroup[] = "service-instance-group";

// Wraps a CertificatePolicyCache as a SupportsUserData::Data; this is necessary
// since reference counted objects can't be user data.
struct CertificatePolicyCacheHandle : public base::SupportsUserData::Data {
  explicit CertificatePolicyCacheHandle(CertificatePolicyCache* cache)
      : policy_cache(cache) {}

  scoped_refptr<CertificatePolicyCache> policy_cache;
};

// Container for a service instance group ID to support association between
// BrowserStates and service instance groups.
class ServiceInstanceGroupHolder : public base::SupportsUserData::Data {
 public:
  explicit ServiceInstanceGroupHolder(const base::Token& instance_group)
      : instance_group_(instance_group) {}
  ~ServiceInstanceGroupHolder() override = default;

  const base::Token& instance_group() const { return instance_group_; }

 private:
  base::Token instance_group_;

  DISALLOW_COPY_AND_ASSIGN(ServiceInstanceGroupHolder);
};

// Eliminates the mapping from |browser_state|'s associated instance group ID
// (if any) to |browser_state|.
void RemoveBrowserStateFromInstanceGroupMap(BrowserState* browser_state) {
  ServiceInstanceGroupHolder* holder = static_cast<ServiceInstanceGroupHolder*>(
      browser_state->GetUserData(kServiceInstanceGroup));
  if (holder) {
    GetInstanceGroupToBrowserState().erase(holder->instance_group());
  }
}

// Container for a ServiceManagerConnection to support association between
// a BrowserState and the ServiceManagerConnection initiated on behalf of that
// BrowserState.
class BrowserStateServiceManagerConnectionHolder
    : public base::SupportsUserData::Data {
 public:
  BrowserStateServiceManagerConnectionHolder(
      BrowserState* browser_state,
      service_manager::mojom::ServiceRequest request)
      : browser_state_(browser_state),
        service_manager_connection_(ServiceManagerConnection::Create(
            std::move(request),
            base::CreateSingleThreadTaskRunnerWithTraits({WebThread::IO}))) {
    service_manager_connection_->SetDefaultServiceRequestHandler(
        base::BindRepeating(
            &BrowserStateServiceManagerConnectionHolder::OnServiceRequest,
            weak_ptr_factory_.GetWeakPtr()));
  }
  ~BrowserStateServiceManagerConnectionHolder() override {}

  ServiceManagerConnection* service_manager_connection() {
    return service_manager_connection_.get();
  }

 private:
  void OnServiceRequest(const std::string& service_name,
                        service_manager::mojom::ServiceRequest request) {
    std::unique_ptr<service_manager::Service> service =
        browser_state_->HandleServiceRequest(service_name, std::move(request));
    if (!service) {
      LOG(ERROR) << "Ignoring request for unknown per-browser-state service:"
                 << service_name;
      return;
    }

    auto* raw_service = service.get();
    service->set_termination_closure(base::BindOnce(
        &BrowserStateServiceManagerConnectionHolder::OnServiceQuit,
        base::Unretained(this), raw_service));
    running_services_.emplace(raw_service, std::move(service));
  }

  void OnServiceQuit(service_manager::Service* service) {
    running_services_.erase(service);
  }

  BrowserState* const browser_state_;
  std::unique_ptr<ServiceManagerConnection> service_manager_connection_;
  std::map<service_manager::Service*, std::unique_ptr<service_manager::Service>>
      running_services_;

  base::WeakPtrFactory<BrowserStateServiceManagerConnectionHolder>
      weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BrowserStateServiceManagerConnectionHolder);
};

}  // namespace

// static
scoped_refptr<CertificatePolicyCache> BrowserState::GetCertificatePolicyCache(
    BrowserState* browser_state) {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  if (!browser_state->GetUserData(kCertificatePolicyCacheKeyName)) {
    browser_state->SetUserData(kCertificatePolicyCacheKeyName,
                               std::make_unique<CertificatePolicyCacheHandle>(
                                   new CertificatePolicyCache()));
  }

  CertificatePolicyCacheHandle* handle =
      static_cast<CertificatePolicyCacheHandle*>(
          browser_state->GetUserData(kCertificatePolicyCacheKeyName));
  return handle->policy_cache;
}

BrowserState::BrowserState() : url_data_manager_ios_backend_(nullptr) {
  // (Refcounted)?BrowserStateKeyedServiceFactories needs to be able to convert
  // a base::SupportsUserData to a BrowserState. Moreover, since the factories
  // may be passed a content::BrowserContext instead of a BrowserState, attach
  // an empty object to this via a private key.
  SetUserData(kBrowserStateIdentifierKey,
              std::make_unique<SupportsUserData::Data>());

  // Set up shared_url_loader_factory_ for lazy creation.
  shared_url_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          base::BindOnce(&BrowserState::GetURLLoaderFactory,
                         base::Unretained(this) /* safe due to Detach call */));
}

BrowserState::~BrowserState() {
  CHECK(GetUserData(kServiceInstanceGroup))
      << "Attempting to destroy a BrowserState that never called "
      << "Initialize()";
  shared_url_loader_factory_->Detach();

  if (network_context_) {
    web::WebThread::DeleteSoon(web::WebThread::IO, FROM_HERE,
                               network_context_owner_.release());
  }

  RemoveBrowserStateFromInstanceGroupMap(this);

  // Delete the URLDataManagerIOSBackend instance on the IO thread if it has
  // been created. Note that while this check can theoretically race with a
  // call to |GetURLDataManagerIOSBackendOnIOThread()|, if any clients of this
  // BrowserState are still accessing it on the IO thread at this point,
  // they're going to have a bad time anyway.
  if (url_data_manager_ios_backend_) {
    bool posted = web::WebThread::DeleteSoon(web::WebThread::IO, FROM_HERE,
                                             url_data_manager_ios_backend_);
    if (!posted)
      delete url_data_manager_ios_backend_;
  }
}

network::mojom::URLLoaderFactory* BrowserState::GetURLLoaderFactory() {
  if (!url_loader_factory_) {
    CreateNetworkContextIfNecessary();
    auto url_loader_factory_params =
        network::mojom::URLLoaderFactoryParams::New();
    url_loader_factory_params->process_id = network::mojom::kBrowserProcessId;
    url_loader_factory_params->is_corb_enabled = false;
    network_context_->CreateURLLoaderFactory(
        mojo::MakeRequest(&url_loader_factory_),
        std::move(url_loader_factory_params));
  }

  return url_loader_factory_.get();
}

network::mojom::CookieManager* BrowserState::GetCookieManager() {
  if (!cookie_manager_) {
    CreateNetworkContextIfNecessary();
    network_context_->GetCookieManager(mojo::MakeRequest(&cookie_manager_));
  }
  return cookie_manager_.get();
}

void BrowserState::GetProxyResolvingSocketFactory(
    network::mojom::ProxyResolvingSocketFactoryRequest request) {
  CreateNetworkContextIfNecessary();

  network_context_->CreateProxyResolvingSocketFactory(std::move(request));
}

scoped_refptr<network::SharedURLLoaderFactory>
BrowserState::GetSharedURLLoaderFactory() {
  return shared_url_loader_factory_;
}

URLDataManagerIOSBackend*
BrowserState::GetURLDataManagerIOSBackendOnIOThread() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  if (!url_data_manager_ios_backend_)
    url_data_manager_ios_backend_ = new URLDataManagerIOSBackend();
  return url_data_manager_ios_backend_;
}

void BrowserState::CreateNetworkContextIfNecessary() {
  if (network_context_owner_)
    return;

  DCHECK(!network_context_);

  net::URLRequestContextGetter* request_context = GetRequestContext();
  DCHECK(request_context);
  network::mojom::NetworkContextParamsPtr network_context_params =
      network::mojom::NetworkContextParams::New();
  UpdateCorsExemptHeader(network_context_params.get());
  network_context_owner_ = std::make_unique<NetworkContextOwner>(
      request_context, network_context_params->cors_exempt_header_list,
      &network_context_);
}

// static
BrowserState* BrowserState::FromSupportsUserData(
    base::SupportsUserData* supports_user_data) {
  if (!supports_user_data ||
      !supports_user_data->GetUserData(kBrowserStateIdentifierKey)) {
    return nullptr;
  }
  return static_cast<BrowserState*>(supports_user_data);
}

// static
void BrowserState::Initialize(BrowserState* browser_state,
                              const base::FilePath& path) {
  base::Token new_group = base::Token::CreateRandom();

  // Note: If the file service is ever used on iOS, code needs to be added here
  // to have the file service associate |path| as the user dir of the instance
  // group of |browser_state| (see corresponding code in
  // content::BrowserContext::Initialize). crbug.com/739450

  RemoveBrowserStateFromInstanceGroupMap(browser_state);
  GetInstanceGroupToBrowserState()[new_group] = browser_state;
  browser_state->SetUserData(
      kServiceInstanceGroup,
      std::make_unique<ServiceInstanceGroupHolder>(new_group));

  ServiceManagerConnection* service_manager_connection =
      ServiceManagerConnection::Get();
  if (service_manager_connection && base::ThreadTaskRunnerHandle::IsSet()) {
    // NOTE: Many unit tests create a TestBrowserState without initializing
    // Mojo or the global service manager connection.

    // Have the global service manager connection start an instance of the
    // web_browser service that is associated with this BrowserState (via
    // |new_group|).
    mojo::PendingRemote<service_manager::mojom::Service> service;
    auto service_receiver = service.InitWithNewPipeAndPassReceiver();

    mojo::Remote<service_manager::mojom::ProcessMetadata> metadata;
    service_manager::Identity identity(mojom::kBrowserServiceName, new_group,
                                       base::Token{},
                                       base::Token::CreateRandom());
    service_manager_connection->GetConnector()->RegisterServiceInstance(
        identity, std::move(service), metadata.BindNewPipeAndPassReceiver());
    metadata->SetPID(base::GetCurrentProcId());

    auto connection_holder =
        std::make_unique<BrowserStateServiceManagerConnectionHolder>(
            browser_state, std::move(service_receiver));

    ServiceManagerConnection* connection =
        connection_holder->service_manager_connection();

    browser_state->SetUserData(kServiceManagerConnection,
                               std::move(connection_holder));

    connection->Start();
  }
}

// static
const base::Token& BrowserState::GetServiceInstanceGroupFor(
    BrowserState* browser_state) {
  ServiceInstanceGroupHolder* holder = static_cast<ServiceInstanceGroupHolder*>(
      browser_state->GetUserData(kServiceInstanceGroup));
  CHECK(holder)
      << "Attempting to get the instance group for a BrowserState that was "
      << "never Initialize()ed.";
  return holder->instance_group();
}

// static
service_manager::Connector* BrowserState::GetConnectorFor(
    BrowserState* browser_state) {
  ServiceManagerConnection* connection =
      GetServiceManagerConnectionFor(browser_state);
  return connection ? connection->GetConnector() : nullptr;
}

// static
ServiceManagerConnection* BrowserState::GetServiceManagerConnectionFor(
    BrowserState* browser_state) {
  BrowserStateServiceManagerConnectionHolder* connection_holder =
      static_cast<BrowserStateServiceManagerConnectionHolder*>(
          browser_state->GetUserData(kServiceManagerConnection));
  return connection_holder ? connection_holder->service_manager_connection()
                           : nullptr;
}

std::unique_ptr<service_manager::Service> BrowserState::HandleServiceRequest(
    const std::string& service_name,
    service_manager::mojom::ServiceRequest request) {
  return nullptr;
}

}  // namespace web
