// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network_service_client.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/threading/sequence_bound.h"
#include "base/unguessable_token.h"
#include "content/browser/browsing_data/clear_site_data_handler.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/devtools/devtools_url_loader_interceptor.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/webrtc_connections_observer.h"
#include "content/browser/ssl/ssl_client_auth_handler.h"
#include "content/browser/ssl/ssl_error_handler.h"
#include "content/browser/ssl/ssl_manager.h"
#include "content/browser/ssl_private_key_impl.h"
#include "content/browser/web_contents/frame_tree_node_id_registry.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/content_features.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/resource_type.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/http/http_auth_preferences.h"
#include "net/ssl/client_cert_store.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"

#if defined(OS_ANDROID)
#include "base/android/content_uri_utils.h"
#include "net/android/http_auth_negotiate_android.h"
#endif

namespace content {
namespace {

class SSLErrorDelegate : public SSLErrorHandler::Delegate {
 public:
  explicit SSLErrorDelegate(
      network::mojom::NetworkServiceClient::OnSSLCertificateErrorCallback
          response)
      : response_(std::move(response)) {}
  ~SSLErrorDelegate() override {}
  void CancelSSLRequest(int error, const net::SSLInfo* ssl_info) override {
    std::move(response_).Run(error);
    delete this;
  }
  void ContinueSSLRequest() override {
    std::move(response_).Run(net::OK);
    delete this;
  }
  base::WeakPtr<SSLErrorDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  network::mojom::NetworkServiceClient::OnSSLCertificateErrorCallback response_;
  base::WeakPtrFactory<SSLErrorDelegate> weak_factory_{this};
};

// This class lives on the IO thread. It is self-owned and will delete itself
// after any of the SSLClientAuthHandler::Delegate methods are invoked (or when
// a mojo connection error occurs).
class SSLClientAuthDelegate : public SSLClientAuthHandler::Delegate {
 public:
  SSLClientAuthDelegate(
      network::mojom::ClientCertificateResponderPtrInfo
          client_cert_responder_info,
      content::ResourceContext* resource_context,
      ResourceRequestInfo::WebContentsGetter web_contents_getter,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_info)
      : client_cert_responder_(std::move(client_cert_responder_info)),
        ssl_client_auth_handler_(std::make_unique<SSLClientAuthHandler>(
            GetContentClient()->browser()->CreateClientCertStore(
                resource_context),
            std::move(web_contents_getter),
            std::move(cert_info.get()),
            this)) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(client_cert_responder_);
    ssl_client_auth_handler_->SelectCertificate();
    client_cert_responder_.set_connection_error_handler(base::BindOnce(
        &SSLClientAuthDelegate::DeleteSelf, base::Unretained(this)));
  }

  ~SSLClientAuthDelegate() override { DCHECK_CURRENTLY_ON(BrowserThread::IO); }

  void DeleteSelf() { delete this; }

  // SSLClientAuthHandler::Delegate:
  void CancelCertificateSelection() override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    client_cert_responder_->CancelRequest();
    DeleteSelf();
  }

  // SSLClientAuthHandler::Delegate:
  void ContinueWithCertificate(
      scoped_refptr<net::X509Certificate> cert,
      scoped_refptr<net::SSLPrivateKey> private_key) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK((cert && private_key) || (!cert && !private_key));

    if (cert && private_key) {
      network::mojom::SSLPrivateKeyPtr ssl_private_key;

      mojo::MakeStrongBinding(std::make_unique<SSLPrivateKeyImpl>(private_key),
                              mojo::MakeRequest(&ssl_private_key));

      client_cert_responder_->ContinueWithCertificate(
          cert, private_key->GetProviderName(),
          private_key->GetAlgorithmPreferences(), std::move(ssl_private_key));
    } else {
      client_cert_responder_->ContinueWithoutCertificate();
    }

    DeleteSelf();
  }

 private:
  network::mojom::ClientCertificateResponderPtr client_cert_responder_;
  std::unique_ptr<SSLClientAuthHandler> ssl_client_auth_handler_;
};

// LoginHandlerDelegate manages HTTP auth. It is self-owning and deletes itself
// when the credentials are resolved or the AuthChallengeResponder is cancelled.
class LoginHandlerDelegate {
 public:
  LoginHandlerDelegate(
      network::mojom::AuthChallengeResponderPtr auth_challenge_responder,
      ResourceRequestInfo::WebContentsGetter web_contents_getter,
      const net::AuthChallengeInfo& auth_info,
      bool is_request_for_main_frame,
      uint32_t process_id,
      uint32_t routing_id,
      uint32_t request_id,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      bool first_auth_attempt)
      : auth_challenge_responder_(std::move(auth_challenge_responder)),
        auth_info_(auth_info),
        request_id_(process_id, request_id),
        routing_id_(routing_id),
        is_request_for_main_frame_(is_request_for_main_frame),
        creating_login_delegate_(false),
        url_(url),
        response_headers_(std::move(response_headers)),
        first_auth_attempt_(first_auth_attempt),
        web_contents_getter_(web_contents_getter) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    auth_challenge_responder_.set_connection_error_handler(base::BindOnce(
        &LoginHandlerDelegate::OnRequestCancelled, base::Unretained(this)));

    auto continue_after_inteceptor_io =
        base::BindOnce(&LoginHandlerDelegate::ContinueAfterInterceptorIO,
                       weak_factory_.GetWeakPtr());
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&DevToolsURLLoaderInterceptor::HandleAuthRequest,
                       request_id_.child_id, routing_id_,
                       request_id_.request_id, auth_info_,
                       std::move(continue_after_inteceptor_io)));
  }

 private:
  void OnRequestCancelled() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    // This will destroy |login_handler_io_| on the IO thread and, if needed,
    // inform the delegate.
    delete this;
  }

  static void ContinueAfterInterceptorIO(
      base::WeakPtr<LoginHandlerDelegate> self_weak,
      bool use_fallback,
      const base::Optional<net::AuthCredentials>& auth_credentials) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&LoginHandlerDelegate::ContinueAfterInterceptorUI,
                       std::move(self_weak), use_fallback, auth_credentials));
  }

  void ContinueAfterInterceptorUI(
      bool use_fallback,
      const base::Optional<net::AuthCredentials>& auth_credentials) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(!(use_fallback && auth_credentials.has_value()));
    if (!use_fallback) {
      OnAuthCredentials(auth_credentials);
      return;
    }

    WebContents* web_contents = web_contents_getter_.Run();
    if (!web_contents) {
      OnAuthCredentials(base::nullopt);
      return;
    }

    // WeakPtr is not strictly necessary here due to OnRequestCancelled.
    creating_login_delegate_ = true;
    login_delegate_ = GetContentClient()->browser()->CreateLoginDelegate(
        auth_info_, web_contents, request_id_, is_request_for_main_frame_, url_,
        response_headers_, first_auth_attempt_,
        base::BindOnce(&LoginHandlerDelegate::OnAuthCredentials,
                       weak_factory_.GetWeakPtr()));
    creating_login_delegate_ = false;
    if (!login_delegate_) {
      OnAuthCredentials(base::nullopt);
      return;
    }
  }

  void OnAuthCredentials(
      const base::Optional<net::AuthCredentials>& auth_credentials) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    // CreateLoginDelegate must not call the callback reentrantly. For
    // robustness, detect this mistake.
    CHECK(!creating_login_delegate_);
    auth_challenge_responder_->OnAuthCredentials(auth_credentials);
    delete this;
  }

  network::mojom::AuthChallengeResponderPtr auth_challenge_responder_;
  net::AuthChallengeInfo auth_info_;
  const content::GlobalRequestID request_id_;
  const uint32_t routing_id_;
  bool is_request_for_main_frame_;
  bool creating_login_delegate_;
  GURL url_;
  const scoped_refptr<net::HttpResponseHeaders> response_headers_;
  bool first_auth_attempt_;
  ResourceRequestInfo::WebContentsGetter web_contents_getter_;
  std::unique_ptr<LoginDelegate> login_delegate_;
  base::WeakPtrFactory<LoginHandlerDelegate> weak_factory_{this};
};

void HandleFileUploadRequest(
    uint32_t process_id,
    bool async,
    const std::vector<base::FilePath>& file_paths,
    NetworkServiceClient::OnFileUploadRequestedCallback callback,
    scoped_refptr<base::TaskRunner> task_runner) {
  std::vector<base::File> files;
  uint32_t file_flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
                        (async ? base::File::FLAG_ASYNC : 0);
  ChildProcessSecurityPolicy* cpsp = ChildProcessSecurityPolicy::GetInstance();
  for (const auto& file_path : file_paths) {
    if (process_id != network::mojom::kBrowserProcessId &&
        !cpsp->CanReadFile(process_id, file_path)) {
      task_runner->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), net::ERR_ACCESS_DENIED,
                                    std::vector<base::File>()));
      return;
    }
#if defined(OS_ANDROID)
    if (file_path.IsContentUri()) {
      files.push_back(base::OpenContentUriForRead(file_path));
    } else {
      files.emplace_back(file_path, file_flags);
    }
#else
    files.emplace_back(file_path, file_flags);
#endif
    if (!files.back().IsValid()) {
      task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback),
                         net::FileErrorToNetError(files.back().error_details()),
                         std::vector<base::File>()));
      return;
    }
  }
  task_runner->PostTask(FROM_HERE, base::BindOnce(std::move(callback), net::OK,
                                                  std::move(files)));
}

FrameTreeNodeIdRegistry::IsMainFrameGetter GetIsMainFrameFromRegistry(
    const base::UnguessableToken& window_id) {
  return FrameTreeNodeIdRegistry::GetInstance()->GetIsMainFrameGetter(
      window_id);
}

base::RepeatingCallback<WebContents*(void)> GetWebContentsFromRegistry(
    const base::UnguessableToken& window_id) {
  return FrameTreeNodeIdRegistry::GetInstance()->GetWebContentsGetter(
      window_id);
}

WebContents* GetWebContents(int process_id, int routing_id) {
  if (process_id != network::mojom::kBrowserProcessId) {
    return WebContentsImpl::FromRenderFrameHostID(process_id, routing_id);
  }
  return WebContents::FromFrameTreeNodeId(routing_id);
}

bool IsMainFrameRequest(int process_id, int routing_id) {
  if (process_id != network::mojom::kBrowserProcessId)
    return false;

  auto* frame_tree_node = FrameTreeNode::GloballyFindByID(routing_id);
  return frame_tree_node && frame_tree_node->IsMainFrame();
}

void OnAuthRequiredContinuation(
    uint32_t process_id,
    uint32_t routing_id,
    uint32_t request_id,
    const GURL& url,
    bool is_request_for_main_frame,
    bool first_auth_attempt,
    const net::AuthChallengeInfo& auth_info,
    const base::Optional<network::ResourceResponseHead>& head,
    network::mojom::AuthChallengeResponderPtr auth_challenge_responder,
    base::RepeatingCallback<WebContents*(void)> web_contents_getter) {
  if (!web_contents_getter) {
    web_contents_getter =
        base::BindRepeating(GetWebContents, process_id, routing_id);
  }
  if (!web_contents_getter.Run()) {
    std::move(auth_challenge_responder)->OnAuthCredentials(base::nullopt);
    return;
  }
  new LoginHandlerDelegate(std::move(auth_challenge_responder),
                           std::move(web_contents_getter), auth_info,
                           is_request_for_main_frame, process_id, routing_id,
                           request_id, url, head ? head->headers : nullptr,
                           first_auth_attempt);  // deletes self
}

void OnAuthRequiredContinuationForWindowId(
    const base::UnguessableToken& window_id,
    uint32_t process_id,
    uint32_t routing_id,
    uint32_t request_id,
    const GURL& url,
    bool first_auth_attempt,
    const net::AuthChallengeInfo& auth_info,
    const base::Optional<network::ResourceResponseHead>& head,
    network::mojom::AuthChallengeResponderPtr auth_challenge_responder,
    FrameTreeNodeIdRegistry::IsMainFrameGetter is_main_frame_getter) {
  if (!is_main_frame_getter) {
    // FrameTreeNode id may already be removed from FrameTreeNodeIdRegistry
    // due to thread hopping.
    std::move(auth_challenge_responder)->OnAuthCredentials(base::nullopt);
    return;
  }
  base::Optional<bool> is_main_frame_opt = is_main_frame_getter.Run();
  // The frame may already be gone due to thread hopping.
  if (!is_main_frame_opt) {
    std::move(auth_challenge_responder)->OnAuthCredentials(base::nullopt);
    return;
  }
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&GetWebContentsFromRegistry, window_id),
      base::BindOnce(&OnAuthRequiredContinuation, process_id, routing_id,
                     request_id, url, *is_main_frame_opt, first_auth_attempt,
                     auth_info, head, std::move(auth_challenge_responder)));
}

void CreateSSLClientAuthDelegateOnIO(
    network::mojom::ClientCertificateResponderPtrInfo
        client_cert_responder_info,
    content::ResourceContext* resource_context,
    ResourceRequestInfo::WebContentsGetter web_contents_getter,
    scoped_refptr<net::SSLCertRequestInfo> cert_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  new SSLClientAuthDelegate(std::move(client_cert_responder_info),
                            resource_context, std::move(web_contents_getter),
                            cert_info);  // deletes self
}

void OnCertificateRequestedContinuation(
    uint32_t process_id,
    uint32_t routing_id,
    uint32_t request_id,
    const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
    network::mojom::ClientCertificateResponderPtrInfo
        client_cert_responder_info,
    base::RepeatingCallback<WebContents*(void)> web_contents_getter) {
  if (!web_contents_getter) {
    web_contents_getter =
        base::BindRepeating(GetWebContents, process_id, routing_id);
  }
  WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents) {
    DCHECK(client_cert_responder_info);
    network::mojom::ClientCertificateResponderPtr client_cert_responder(
        std::move(client_cert_responder_info));
    client_cert_responder->CancelRequest();
    return;
  }

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&CreateSSLClientAuthDelegateOnIO,
                     std::move(client_cert_responder_info),
                     web_contents->GetBrowserContext()->GetResourceContext(),
                     std::move(web_contents_getter), cert_info));
}

#if defined(OS_ANDROID)
void FinishGenerateNegotiateAuthToken(
    std::unique_ptr<net::android::HttpAuthNegotiateAndroid> auth_negotiate,
    std::unique_ptr<std::string> auth_token,
    std::unique_ptr<net::HttpAuthPreferences> prefs,
    NetworkServiceClient::OnGenerateHttpNegotiateAuthTokenCallback callback,
    int result) {
  std::move(callback).Run(result, *auth_token);
}
#endif

}  // namespace

NetworkServiceClient::NetworkServiceClient(
    network::mojom::NetworkServiceClientRequest network_service_client_request)
    : binding_(this, std::move(network_service_client_request))
#if defined(OS_ANDROID)
      ,
      app_status_listener_(base::android::ApplicationStatusListener::New(
          base::BindRepeating(&NetworkServiceClient::OnApplicationStateChange,
                              base::Unretained(this))))
#endif
{

#if defined(OS_MACOSX)
  if (base::MessageLoopCurrentForUI::IsSet())  // Not set in some unit tests.
    net::CertDatabase::GetInstance()->StartListeningForKeychainEvents();
#endif

  if (IsOutOfProcessNetworkService()) {
    net::CertDatabase::GetInstance()->AddObserver(this);
    memory_pressure_listener_ =
        std::make_unique<base::MemoryPressureListener>(base::BindRepeating(
            &NetworkServiceClient::OnMemoryPressure, base::Unretained(this)));

#if defined(OS_ANDROID)
    DCHECK(net::NetworkChangeNotifier::HasNetworkChangeNotifier());
    GetNetworkService()->GetNetworkChangeManager(
        mojo::MakeRequest(&network_change_manager_));
    net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
    net::NetworkChangeNotifier::AddMaxBandwidthObserver(this);
    net::NetworkChangeNotifier::AddIPAddressObserver(this);
    net::NetworkChangeNotifier::AddDNSObserver(this);
#endif
  }

  webrtc_connections_observer_ =
      std::make_unique<content::WebRtcConnectionsObserver>(base::BindRepeating(
          &NetworkServiceClient::OnPeerToPeerConnectionsCountChange,
          base::Unretained(this)));
}

NetworkServiceClient::~NetworkServiceClient() {
  if (IsOutOfProcessNetworkService()) {
    net::CertDatabase::GetInstance()->RemoveObserver(this);
#if defined(OS_ANDROID)
    net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
    net::NetworkChangeNotifier::RemoveMaxBandwidthObserver(this);
    net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
    net::NetworkChangeNotifier::RemoveDNSObserver(this);
#endif
  }
}

void NetworkServiceClient::OnAuthRequired(
    const base::Optional<base::UnguessableToken>& window_id,
    uint32_t process_id,
    uint32_t routing_id,
    uint32_t request_id,
    const GURL& url,
    bool first_auth_attempt,
    const net::AuthChallengeInfo& auth_info,
    const base::Optional<network::ResourceResponseHead>& head,
    network::mojom::AuthChallengeResponderPtr auth_challenge_responder) {
  if (window_id) {
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&GetIsMainFrameFromRegistry, *window_id),
        base::BindOnce(&OnAuthRequiredContinuationForWindowId, *window_id,
                       process_id, routing_id, request_id, url,
                       first_auth_attempt, auth_info, head,
                       std::move(auth_challenge_responder)));
    return;
  }
  OnAuthRequiredContinuation(process_id, routing_id, request_id, url,
                             IsMainFrameRequest(process_id, routing_id),
                             first_auth_attempt, auth_info, head,
                             std::move(auth_challenge_responder), {});
}

void NetworkServiceClient::OnCertificateRequested(
    const base::Optional<base::UnguessableToken>& window_id,
    uint32_t process_id,
    uint32_t routing_id,
    uint32_t request_id,
    const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
    network::mojom::ClientCertificateResponderPtr cert_responder) {
  // Use |window_id| if it's provided.
  if (window_id) {
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&GetWebContentsFromRegistry, *window_id),
        base::BindOnce(&OnCertificateRequestedContinuation, process_id,
                       routing_id, request_id, cert_info,
                       cert_responder.PassInterface()));
    return;
  }

  OnCertificateRequestedContinuation(process_id, routing_id, request_id,
                                     cert_info, cert_responder.PassInterface(),
                                     {});
}

void NetworkServiceClient::OnSSLCertificateError(
    uint32_t process_id,
    uint32_t routing_id,
    const GURL& url,
    int net_error,
    const net::SSLInfo& ssl_info,
    bool fatal,
    OnSSLCertificateErrorCallback response) {
  SSLErrorDelegate* delegate =
      new SSLErrorDelegate(std::move(response));  // deletes self
  base::RepeatingCallback<WebContents*(void)> web_contents_getter =
      base::BindRepeating(GetWebContents, process_id, routing_id);
  bool is_main_frame_request = IsMainFrameRequest(process_id, routing_id);
  SSLManager::OnSSLCertificateError(
      delegate->GetWeakPtr(), is_main_frame_request, url,
      std::move(web_contents_getter), net_error, ssl_info, fatal);
}

#if defined(OS_CHROMEOS)
void NetworkServiceClient::OnTrustAnchorUsed(const std::string& username_hash) {
  GetContentClient()->browser()->OnTrustAnchorUsed(username_hash);
}
#endif

void NetworkServiceClient::OnFileUploadRequested(
    uint32_t process_id,
    bool async,
    const std::vector<base::FilePath>& file_paths,
    OnFileUploadRequestedCallback callback) {
  base::PostTaskWithTraits(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&HandleFileUploadRequest, process_id, async, file_paths,
                     std::move(callback),
                     base::SequencedTaskRunnerHandle::Get()));
}

void NetworkServiceClient::OnLoadingStateUpdate(
    std::vector<network::mojom::LoadInfoPtr> infos,
    OnLoadingStateUpdateCallback callback) {
  auto rdh_infos = std::make_unique<ResourceDispatcherHostImpl::LoadInfoList>();

  // TODO(jam): once ResourceDispatcherHost is gone remove the translation
  // (other than adding the WebContents callback).
  for (auto& info : infos) {
    ResourceDispatcherHostImpl::LoadInfo load_info;
    load_info.host = std::move(info->host);
    load_info.load_state.state = static_cast<net::LoadState>(info->load_state);
    load_info.load_state.param = std::move(info->state_param);
    load_info.upload_position = info->upload_position;
    load_info.upload_size = info->upload_size;
    load_info.web_contents_getter =
        base::BindRepeating(GetWebContents, info->process_id, info->routing_id);
    rdh_infos->push_back(std::move(load_info));
  }

  auto* rdh = ResourceDispatcherHostImpl::Get();
  ResourceDispatcherHostImpl::UpdateLoadStateOnUI(rdh->loader_delegate_,
                                                  std::move(rdh_infos));

  std::move(callback).Run();
}

void NetworkServiceClient::OnCertDBChanged() {
  GetNetworkService()->OnCertDBChanged();
}

void NetworkServiceClient::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  GetNetworkService()->OnMemoryPressure(memory_pressure_level);
}

void NetworkServiceClient::OnPeerToPeerConnectionsCountChange(uint32_t count) {
  GetNetworkService()->OnPeerToPeerConnectionsCountChange(count);
}

#if defined(OS_ANDROID)
void NetworkServiceClient::OnApplicationStateChange(
    base::android::ApplicationState state) {
  GetNetworkService()->OnApplicationStateChange(state);
}

void NetworkServiceClient::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  network_change_manager_->OnNetworkChanged(
      false /* dns_changed */, false /* ip_address_changed */,
      true /* connection_type_changed */, network::mojom::ConnectionType(type),
      false /* connection_subtype_changed */,
      network::mojom::ConnectionSubtype(
          net::NetworkChangeNotifier::GetConnectionSubtype()));
}

void NetworkServiceClient::OnMaxBandwidthChanged(
    double max_bandwidth_mbps,
    net::NetworkChangeNotifier::ConnectionType type) {
  // The connection subtype change will trigger a max bandwidth change in the
  // network service notifier.
  network_change_manager_->OnNetworkChanged(
      false /* dns_changed */, false /* ip_address_changed */,
      false /* connection_type_changed */, network::mojom::ConnectionType(type),
      true /* connection_subtype_changed */,
      network::mojom::ConnectionSubtype(
          net::NetworkChangeNotifier::GetConnectionSubtype()));
}

void NetworkServiceClient::OnIPAddressChanged() {
  network_change_manager_->OnNetworkChanged(
      false /* dns_changed */, true /* ip_address_changed */,
      false /* connection_type_changed */,
      network::mojom::ConnectionType(
          net::NetworkChangeNotifier::GetConnectionType()),
      false /* connection_subtype_changed */,
      network::mojom::ConnectionSubtype(
          net::NetworkChangeNotifier::GetConnectionSubtype()));
}

void NetworkServiceClient::OnDNSChanged() {
  network_change_manager_->OnNetworkChanged(
      true /* dns_changed */, false /* ip_address_changed */,
      false /* connection_type_changed */,
      network::mojom::ConnectionType(
          net::NetworkChangeNotifier::GetConnectionType()),
      false /* connection_subtype_changed */,
      network::mojom::ConnectionSubtype(
          net::NetworkChangeNotifier::GetConnectionSubtype()));
}

void NetworkServiceClient::OnInitialDNSConfigRead() {
  network_change_manager_->OnNetworkChanged(
      true /* dns_changed */, false /* ip_address_changed */,
      false /* connection_type_changed */,
      network::mojom::ConnectionType(
          net::NetworkChangeNotifier::GetConnectionType()),
      false /* connection_subtype_changed */,
      network::mojom::ConnectionSubtype(
          net::NetworkChangeNotifier::GetConnectionSubtype()));
}
#endif

void NetworkServiceClient::OnDataUseUpdate(
    int32_t network_traffic_annotation_id_hash,
    int64_t recv_bytes,
    int64_t sent_bytes) {
  GetContentClient()->browser()->OnNetworkServiceDataUseUpdate(
      network_traffic_annotation_id_hash, recv_bytes, sent_bytes);
}

#if defined(OS_ANDROID)
void NetworkServiceClient::OnGenerateHttpNegotiateAuthToken(
    const std::string& server_auth_token,
    bool can_delegate,
    const std::string& auth_negotiate_android_account_type,
    const std::string& spn,
    OnGenerateHttpNegotiateAuthTokenCallback callback) {
  // The callback takes ownership of these unique_ptrs and destroys them when
  // run.
  auto prefs = std::make_unique<net::HttpAuthPreferences>();
  prefs->set_auth_android_negotiate_account_type(
      auth_negotiate_android_account_type);

  auto auth_negotiate =
      std::make_unique<net::android::HttpAuthNegotiateAndroid>(prefs.get());
  net::android::HttpAuthNegotiateAndroid* auth_negotiate_raw =
      auth_negotiate.get();
  auth_negotiate->set_server_auth_token(server_auth_token);
  auth_negotiate->set_can_delegate(can_delegate);

  auto auth_token = std::make_unique<std::string>();
  auth_negotiate_raw->GenerateAuthTokenAndroid(
      nullptr, spn, std::string(), auth_token.get(),
      base::BindOnce(&FinishGenerateNegotiateAuthToken,
                     std::move(auth_negotiate), std::move(auth_token),
                     std::move(prefs), std::move(callback)));
}
#endif

void NetworkServiceClient::OnRawRequest(
    int32_t process_id,
    int32_t routing_id,
    const std::string& devtools_request_id,
    const net::CookieStatusList& cookies_with_status,
    std::vector<network::mojom::HttpRawHeaderPairPtr> headers) {
  devtools_instrumentation::OnRequestWillBeSentExtraInfo(
      process_id, routing_id, devtools_request_id, cookies_with_status,
      headers);
}

void NetworkServiceClient::OnRawResponse(
    int32_t process_id,
    int32_t routing_id,
    const std::string& devtools_request_id,
    const net::CookieAndLineStatusList& cookies_with_status,
    std::vector<network::mojom::HttpRawHeaderPairPtr> headers,
    const base::Optional<std::string>& raw_response_headers) {
  devtools_instrumentation::OnResponseReceivedExtraInfo(
      process_id, routing_id, devtools_request_id, cookies_with_status, headers,
      raw_response_headers);
}
}  // namespace content
