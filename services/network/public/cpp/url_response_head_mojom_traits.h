// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_URL_RESPONSE_HEAD_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_URL_RESPONSE_HEAD_MOJOM_TRAITS_H_

#include "services/network/public/cpp/http_raw_request_response_info.h"
#include "services/network/public/cpp/origin_policy.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace mojo {

template <>
struct StructTraits<network::mojom::URLResponseHeadDataView,
                    network::ResourceResponseHead> {
  static const base::Time& request_time(
      const network::ResourceResponseHead& input) {
    return input.request_time;
  }

  static const base::Time& response_time(
      const network::ResourceResponseHead& input) {
    return input.response_time;
  }

  static scoped_refptr<net::HttpResponseHeaders> headers(
      const network::ResourceResponseHead& input) {
    return input.headers;
  }

  static const std::string& mime_type(
      const network::ResourceResponseHead& input) {
    return input.mime_type;
  }

  static const std::string& charset(
      const network::ResourceResponseHead& input) {
    return input.charset;
  }

  static net::ct::CTPolicyCompliance ct_policy_compliance(
      const network::ResourceResponseHead& input) {
    return input.ct_policy_compliance;
  }

  static int64_t content_length(const network::ResourceResponseHead& input) {
    return input.content_length;
  }

  static int64_t encoded_data_length(
      const network::ResourceResponseHead& input) {
    return input.encoded_data_length;
  }

  static int64_t encoded_body_length(
      const network::ResourceResponseHead& input) {
    return input.encoded_body_length;
  }

  static bool network_accessed(const network::ResourceResponseHead& input) {
    return input.network_accessed;
  }

  static int64_t appcache_id(const network::ResourceResponseHead& input) {
    return input.appcache_id;
  }

  static const GURL& appcache_manifest_url(
      const network::ResourceResponseHead& input) {
    return input.appcache_manifest_url;
  }

  static const net::LoadTimingInfo& load_timing(
      const network::ResourceResponseHead& input) {
    return input.load_timing;
  }

  static scoped_refptr<network::HttpRawRequestResponseInfo>
  raw_request_response_info(const network::ResourceResponseHead& input) {
    return input.raw_request_response_info;
  }

  static bool was_fetched_via_spdy(const network::ResourceResponseHead& input) {
    return input.was_fetched_via_spdy;
  }

  static bool was_alpn_negotiated(const network::ResourceResponseHead& input) {
    return input.was_alpn_negotiated;
  }

  static bool was_alternate_protocol_available(
      const network::ResourceResponseHead& input) {
    return input.was_alternate_protocol_available;
  }

  static net::HttpResponseInfo::ConnectionInfo connection_info(
      const network::ResourceResponseHead& input) {
    return input.connection_info;
  }

  static const std::string& alpn_negotiated_protocol(
      const network::ResourceResponseHead& input) {
    return input.alpn_negotiated_protocol;
  }

  static const net::IPEndPoint& remote_endpoint(
      const network::ResourceResponseHead& input) {
    return input.remote_endpoint;
  }

  static bool was_fetched_via_cache(
      const network::ResourceResponseHead& input) {
    return input.was_fetched_via_cache;
  }

  static const net::ProxyServer& proxy_server(
      const network::ResourceResponseHead& input) {
    return input.proxy_server;
  }

  static bool was_fetched_via_service_worker(
      const network::ResourceResponseHead& input) {
    return input.was_fetched_via_service_worker;
  }

  static bool was_fallback_required_by_service_worker(
      const network::ResourceResponseHead& input) {
    return input.was_fallback_required_by_service_worker;
  }

  static const std::vector<GURL>& url_list_via_service_worker(
      const network::ResourceResponseHead& input) {
    return input.url_list_via_service_worker;
  }

  static network::mojom::FetchResponseType response_type(
      const network::ResourceResponseHead& input) {
    return input.response_type;
  }

  static const base::TimeTicks& service_worker_start_time(
      const network::ResourceResponseHead& input) {
    return input.service_worker_start_time;
  }

  static const base::TimeTicks& service_worker_ready_time(
      const network::ResourceResponseHead& input) {
    return input.service_worker_ready_time;
  }

  static bool is_in_cache_storage(const network::ResourceResponseHead& input) {
    return input.is_in_cache_storage;
  }

  static const std::string& cache_storage_cache_name(
      const network::ResourceResponseHead& input) {
    return input.cache_storage_cache_name;
  }

  static net::EffectiveConnectionType effective_connection_type(
      const network::ResourceResponseHead& input) {
    return input.effective_connection_type;
  }

  static net::CertStatus cert_status(
      const network::ResourceResponseHead& input) {
    return input.cert_status;
  }

  static const base::Optional<net::SSLInfo>& ssl_info(
      const network::ResourceResponseHead& input) {
    return input.ssl_info;
  }

  static const std::vector<std::string>& cors_exposed_header_names(
      const network::ResourceResponseHead& input) {
    return input.cors_exposed_header_names;
  }

  static bool did_service_worker_navigation_preload(
      const network::ResourceResponseHead& input) {
    return input.did_service_worker_navigation_preload;
  }

  static bool should_report_corb_blocking(
      const network::ResourceResponseHead& input) {
    return input.should_report_corb_blocking;
  }

  static bool async_revalidation_requested(
      const network::ResourceResponseHead& input) {
    return input.async_revalidation_requested;
  }

  static bool did_mime_sniff(const network::ResourceResponseHead& input) {
    return input.did_mime_sniff;
  }

  static bool is_signed_exchange_inner_response(
      const network::ResourceResponseHead& input) {
    return input.is_signed_exchange_inner_response;
  }

  static bool was_in_prefetch_cache(
      const network::ResourceResponseHead& input) {
    return input.was_in_prefetch_cache;
  }

  static bool intercepted_by_plugin(
      const network::ResourceResponseHead& input) {
    return input.intercepted_by_plugin;
  }

  static bool is_legacy_tls_version(
      const network::ResourceResponseHead& input) {
    return input.is_legacy_tls_version;
  }

  static const base::Optional<net::AuthChallengeInfo>& auth_challenge_info(
      const network::ResourceResponseHead& input) {
    return input.auth_challenge_info;
  }

  static const base::TimeTicks& request_start(
      const network::ResourceResponseHead& input) {
    return input.request_start;
  }

  static const base::TimeTicks& response_start(
      const network::ResourceResponseHead& input) {
    return input.response_start;
  }

  static const base::Optional<network::OriginPolicy>& origin_policy(
      const network::ResourceResponseHead& input) {
    return input.origin_policy;
  }

  static bool Read(network::mojom::URLResponseHead::DataView input,
                   network::ResourceResponseHead* output);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_URL_RESPONSE_HEAD_MOJOM_TRAITS_H_
