// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/url_response_head_mojom_traits.h"

#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "services/network/public/cpp/ip_endpoint_mojom_traits.h"
#include "services/network/public/cpp/load_timing_info_mojom_traits.h"
#include "services/network/public/cpp/network_ipc_param_traits.h"
#include "services/proxy_resolver/public/cpp/proxy_resolver_mojom_traits.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<network::mojom::URLResponseHead::DataView,
                  network::ResourceResponseHead>::
    Read(network::mojom::URLResponseHead::DataView input,
         network::ResourceResponseHead* output) {
  bool success = true;

  if (!input.ReadRequestTime(&output->request_time))
    success = false;
  if (!input.ReadResponseTime(&output->response_time))
    success = false;
  if (!input.ReadHeaders(&output->headers))
    success = false;
  if (!input.ReadMimeType(&output->mime_type))
    success = false;
  if (!input.ReadCharset(&output->charset))
    success = false;
  if (!input.ReadCtPolicyCompliance(&output->ct_policy_compliance))
    success = false;
  output->content_length = input.content_length();
  output->encoded_data_length = input.encoded_data_length();
  output->encoded_body_length = input.encoded_body_length();
  output->network_accessed = input.network_accessed();
  output->appcache_id = input.appcache_id();
  if (!input.ReadAppcacheManifestUrl(&output->appcache_manifest_url))
    success = false;
  if (!input.ReadLoadTiming(&output->load_timing))
    success = false;
  if (!input.ReadRawRequestResponseInfo(&output->raw_request_response_info))
    success = false;
  output->was_fetched_via_spdy = input.was_fetched_via_spdy();
  output->was_alpn_negotiated = input.was_alpn_negotiated();
  output->was_alternate_protocol_available =
      input.was_alternate_protocol_available();
  if (!input.ReadConnectionInfo(&output->connection_info))
    success = false;
  if (!input.ReadAlpnNegotiatedProtocol(&output->alpn_negotiated_protocol))
    success = false;
  if (!input.ReadRemoteEndpoint(&output->remote_endpoint))
    success = false;
  output->was_fetched_via_cache = input.was_fetched_via_cache();
  if (!input.ReadProxyServer(&output->proxy_server))
    success = false;
  output->was_fetched_via_service_worker =
      input.was_fetched_via_service_worker();
  output->was_fallback_required_by_service_worker =
      input.was_fallback_required_by_service_worker();
  if (!input.ReadUrlListViaServiceWorker(&output->url_list_via_service_worker))
    success = false;
  if (!input.ReadResponseType(&output->response_type))
    success = false;
  if (!input.ReadServiceWorkerStartTime(&output->service_worker_start_time))
    success = false;
  if (!input.ReadServiceWorkerReadyTime(&output->service_worker_ready_time))
    success = false;
  output->is_in_cache_storage = input.is_in_cache_storage();
  if (!input.ReadCacheStorageCacheName(&output->cache_storage_cache_name))
    success = false;
  if (!input.ReadEffectiveConnectionType(&output->effective_connection_type))
    success = false;
  output->cert_status = input.cert_status();
  if (!input.ReadSslInfo(&output->ssl_info))
    success = false;
  if (!input.ReadCorsExposedHeaderNames(&output->cors_exposed_header_names))
    success = false;
  output->did_service_worker_navigation_preload =
      input.did_service_worker_navigation_preload();
  output->should_report_corb_blocking = input.should_report_corb_blocking();
  output->async_revalidation_requested = input.async_revalidation_requested();
  output->did_mime_sniff = input.did_mime_sniff();
  output->is_signed_exchange_inner_response =
      input.is_signed_exchange_inner_response();
  output->was_in_prefetch_cache = input.was_in_prefetch_cache();
  output->intercepted_by_plugin = input.intercepted_by_plugin();
  output->is_legacy_tls_version = input.is_legacy_tls_version();
  if (!input.ReadAuthChallengeInfo(&output->auth_challenge_info))
    success = false;
  if (!input.ReadRequestStart(&output->request_start))
    success = false;
  if (!input.ReadResponseStart(&output->response_start))
    success = false;
  if (!input.ReadOriginPolicy(&output->origin_policy))
    success = false;
  return success;
}

}  // namespace mojo
