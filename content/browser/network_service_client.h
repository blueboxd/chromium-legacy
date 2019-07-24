// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NETWORK_SERVICE_IMPL_H_
#define CONTENT_BROWSER_NETWORK_SERVICE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/cert/cert_database.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace content {

class WebRtcConnectionsObserver;

class CONTENT_EXPORT NetworkServiceClient
    : public network::mojom::NetworkServiceClient,
#if defined(OS_ANDROID)
      public net::NetworkChangeNotifier::ConnectionTypeObserver,
      public net::NetworkChangeNotifier::MaxBandwidthObserver,
      public net::NetworkChangeNotifier::IPAddressObserver,
      public net::NetworkChangeNotifier::DNSObserver,
#endif
      public net::CertDatabase::Observer {
 public:
  explicit NetworkServiceClient(network::mojom::NetworkServiceClientRequest
                                    network_service_client_request);
  ~NetworkServiceClient() override;

  // network::mojom::NetworkServiceClient implementation:
  void OnAuthRequired(const base::Optional<base::UnguessableToken>& window_id,
                      uint32_t process_id,
                      uint32_t routing_id,
                      uint32_t request_id,
                      const GURL& url,
                      bool first_auth_attempt,
                      const net::AuthChallengeInfo& auth_info,
                      const base::Optional<network::ResourceResponseHead>& head,
                      network::mojom::AuthChallengeResponderPtr
                          auth_challenge_responder) override;
  void OnCertificateRequested(
      const base::Optional<base::UnguessableToken>& window_id,
      uint32_t process_id,
      uint32_t routing_id,
      uint32_t request_id,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
      network::mojom::ClientCertificateResponderPtr cert_responder) override;
  void OnSSLCertificateError(uint32_t process_id,
                             uint32_t routing_id,
                             const GURL& url,
                             int net_error,
                             const net::SSLInfo& ssl_info,
                             bool fatal,
                             OnSSLCertificateErrorCallback response) override;
#if defined(OS_CHROMEOS)
  void OnTrustAnchorUsed(const std::string& username_hash) override;
#endif
  void OnFileUploadRequested(uint32_t process_id,
                             bool async,
                             const std::vector<base::FilePath>& file_paths,
                             OnFileUploadRequestedCallback callback) override;
  void OnLoadingStateUpdate(std::vector<network::mojom::LoadInfoPtr> infos,
                            OnLoadingStateUpdateCallback callback) override;
  void OnDataUseUpdate(int32_t network_traffic_annotation_id_hash,
                       int64_t recv_bytes,
                       int64_t sent_bytes) override;
#if defined(OS_ANDROID)
  void OnGenerateHttpNegotiateAuthToken(
      const std::string& server_auth_token,
      bool can_delegate,
      const std::string& auth_negotiate_android_account_type,
      const std::string& spn,
      OnGenerateHttpNegotiateAuthTokenCallback callback) override;
#endif
  void OnRawRequest(
      int32_t process_id,
      int32_t routing_id,
      const std::string& devtools_request_id,
      const net::CookieStatusList& cookies_with_status,
      std::vector<network::mojom::HttpRawHeaderPairPtr> headers) override;
  void OnRawResponse(
      int32_t process_id,
      int32_t routing_id,
      const std::string& devtools_request_id,
      const net::CookieAndLineStatusList& cookies_with_status,
      std::vector<network::mojom::HttpRawHeaderPairPtr> headers,
      const base::Optional<std::string>& raw_response_headers) override;
  // net::CertDatabase::Observer implementation:
  void OnCertDBChanged() override;

  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_presure_level);

  // Called when there is a change in the count of media connections that
  // require low network latency.
  void OnPeerToPeerConnectionsCountChange(uint32_t count);

#if defined(OS_ANDROID)
  void OnApplicationStateChange(base::android::ApplicationState state);

  // net::NetworkChangeNotifier::ConnectionTypeObserver implementation:
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // net::NetworkChangeNotifier::MaxBandwidthObserver implementation:
  void OnMaxBandwidthChanged(
      double max_bandwidth_mbps,
      net::NetworkChangeNotifier::ConnectionType type) override;

  // net::NetworkChangeNotifier::IPAddressObserver implementation:
  void OnIPAddressChanged() override;

  // net::NetworkChangeNotifier::DNSObserver implementation:
  void OnDNSChanged() override;
  void OnInitialDNSConfigRead() override;
#endif

 private:
  mojo::Binding<network::mojom::NetworkServiceClient> binding_;

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  std::unique_ptr<WebRtcConnectionsObserver> webrtc_connections_observer_;

#if defined(OS_ANDROID)
  std::unique_ptr<base::android::ApplicationStatusListener>
      app_status_listener_;
  network::mojom::NetworkChangeManagerPtr network_change_manager_;
#endif

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NETWORK_SERVICE_IMPL_H_
