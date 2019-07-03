// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_IPC_PARAM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NETWORK_IPC_PARAM_TRAITS_H_

#include <string>

#include "base/component_export.h"
#include "base/pickle.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/param_traits_macros.h"
#include "net/base/auth.h"
#include "net/base/ip_endpoint.h"
#include "net/base/proxy_server.h"
#include "net/base/request_priority.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_version.h"
#include "net/nqe/effective_connection_type.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_info.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/net_ipc_param_traits.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/resource_response_info.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/cors.mojom-shared.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "url/ipc/url_param_traits.h"
#include "url/origin.h"

// This file defines IPC::ParamTraits for network:: classes / structs.
// For IPC::ParamTraits for net:: class / structs, see net_ipc_param_traits.h.

#ifndef INTERNAL_SERVICES_NETWORK_PUBLIC_CPP_NETWORK_IPC_PARAM_TRAITS_H_
#define INTERNAL_SERVICES_NETWORK_PUBLIC_CPP_NETWORK_IPC_PARAM_TRAITS_H_

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT COMPONENT_EXPORT(NETWORK_CPP_BASE)

namespace network {
struct HttpRawRequestResponseInfo;
}

namespace IPC {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    ParamTraits<scoped_refptr<network::HttpRawRequestResponseInfo>> {
  typedef scoped_refptr<network::HttpRawRequestResponseInfo> param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

// TODO(Richard): Remove this traits after usage of FrameHostMsg_OpenURL_Params
// disappears.
template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE) ParamTraits<network::DataElement> {
  typedef network::DataElement param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

// TODO(Richard): Remove this traits after usage of FrameHostMsg_OpenURL_Params
// disappears.
template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    ParamTraits<scoped_refptr<network::ResourceRequestBody>> {
  typedef scoped_refptr<network::ResourceRequestBody> param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // INTERNAL_SERVICES_NETWORK_PUBLIC_CPP_NETWORK_IPC_PARAM_TRAITS_H_

IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::CorsError,
                          network::mojom::CorsError::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::CredentialsMode,
                          network::mojom::CredentialsMode::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::RedirectMode,
                          network::mojom::RedirectMode::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::RequestMode,
                          network::mojom::RequestMode::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::CorsPreflightPolicy,
                          network::mojom::CorsPreflightPolicy::kMaxValue)

IPC_STRUCT_TRAITS_BEGIN(network::CorsErrorStatus)
  IPC_STRUCT_TRAITS_MEMBER(cors_error)
  IPC_STRUCT_TRAITS_MEMBER(failed_parameter)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(network::cors::PreflightTimingInfo)
  IPC_STRUCT_TRAITS_MEMBER(start_time)
  IPC_STRUCT_TRAITS_MEMBER(response_end)
  IPC_STRUCT_TRAITS_MEMBER(alpn_negotiated_protocol)
  IPC_STRUCT_TRAITS_MEMBER(connection_info)
  IPC_STRUCT_TRAITS_MEMBER(timing_allow_origin)
  IPC_STRUCT_TRAITS_MEMBER(transfer_size)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(network::URLLoaderCompletionStatus)
  IPC_STRUCT_TRAITS_MEMBER(error_code)
  IPC_STRUCT_TRAITS_MEMBER(extended_error_code)
  IPC_STRUCT_TRAITS_MEMBER(exists_in_cache)
  IPC_STRUCT_TRAITS_MEMBER(completion_time)
  IPC_STRUCT_TRAITS_MEMBER(cors_preflight_timing_info)
  IPC_STRUCT_TRAITS_MEMBER(encoded_data_length)
  IPC_STRUCT_TRAITS_MEMBER(encoded_body_length)
  IPC_STRUCT_TRAITS_MEMBER(decoded_body_length)
  IPC_STRUCT_TRAITS_MEMBER(cors_error_status)
  IPC_STRUCT_TRAITS_MEMBER(ssl_info)
  IPC_STRUCT_TRAITS_MEMBER(should_report_corb_blocking)
  IPC_STRUCT_TRAITS_MEMBER(proxy_server)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(network::ResourceResponseInfo)
  IPC_STRUCT_TRAITS_MEMBER(request_time)
  IPC_STRUCT_TRAITS_MEMBER(response_time)
  IPC_STRUCT_TRAITS_MEMBER(headers)
  IPC_STRUCT_TRAITS_MEMBER(mime_type)
  IPC_STRUCT_TRAITS_MEMBER(charset)
  IPC_STRUCT_TRAITS_MEMBER(ct_policy_compliance)
  IPC_STRUCT_TRAITS_MEMBER(content_length)
  IPC_STRUCT_TRAITS_MEMBER(network_accessed)
  IPC_STRUCT_TRAITS_MEMBER(encoded_data_length)
  IPC_STRUCT_TRAITS_MEMBER(encoded_body_length)
  IPC_STRUCT_TRAITS_MEMBER(appcache_id)
  IPC_STRUCT_TRAITS_MEMBER(appcache_manifest_url)
  IPC_STRUCT_TRAITS_MEMBER(load_timing)
  IPC_STRUCT_TRAITS_MEMBER(raw_request_response_info)
  IPC_STRUCT_TRAITS_MEMBER(was_fetched_via_spdy)
  IPC_STRUCT_TRAITS_MEMBER(was_alpn_negotiated)
  IPC_STRUCT_TRAITS_MEMBER(was_alternate_protocol_available)
  IPC_STRUCT_TRAITS_MEMBER(connection_info)
  IPC_STRUCT_TRAITS_MEMBER(alpn_negotiated_protocol)
  IPC_STRUCT_TRAITS_MEMBER(remote_endpoint)
  IPC_STRUCT_TRAITS_MEMBER(was_fetched_via_cache)
  IPC_STRUCT_TRAITS_MEMBER(proxy_server)
  IPC_STRUCT_TRAITS_MEMBER(was_fetched_via_service_worker)
  IPC_STRUCT_TRAITS_MEMBER(was_fallback_required_by_service_worker)
  IPC_STRUCT_TRAITS_MEMBER(url_list_via_service_worker)
  IPC_STRUCT_TRAITS_MEMBER(response_type)
  IPC_STRUCT_TRAITS_MEMBER(service_worker_start_time)
  IPC_STRUCT_TRAITS_MEMBER(service_worker_ready_time)
  IPC_STRUCT_TRAITS_MEMBER(is_in_cache_storage)
  IPC_STRUCT_TRAITS_MEMBER(cache_storage_cache_name)
  IPC_STRUCT_TRAITS_MEMBER(did_service_worker_navigation_preload)
  IPC_STRUCT_TRAITS_MEMBER(effective_connection_type)
  IPC_STRUCT_TRAITS_MEMBER(cert_status)
  IPC_STRUCT_TRAITS_MEMBER(ssl_info)
  IPC_STRUCT_TRAITS_MEMBER(cors_exposed_header_names)
  IPC_STRUCT_TRAITS_MEMBER(async_revalidation_requested)
  IPC_STRUCT_TRAITS_MEMBER(did_mime_sniff)
  IPC_STRUCT_TRAITS_MEMBER(is_signed_exchange_inner_response)
  IPC_STRUCT_TRAITS_MEMBER(was_in_prefetch_cache)
  IPC_STRUCT_TRAITS_MEMBER(is_legacy_tls_version)
  IPC_STRUCT_TRAITS_MEMBER(auth_challenge_info)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::FetchResponseType,
                          network::mojom::FetchResponseType::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(network::OriginPolicyState,
                          network::OriginPolicyState::kMaxValue)

IPC_STRUCT_TRAITS_BEGIN(network::OriginPolicyContents)
  IPC_STRUCT_TRAITS_MEMBER(features)
  IPC_STRUCT_TRAITS_MEMBER(content_security_policies)
  IPC_STRUCT_TRAITS_MEMBER(content_security_policies_report_only)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(network::OriginPolicy)
  IPC_STRUCT_TRAITS_MEMBER(state)
  IPC_STRUCT_TRAITS_MEMBER(policy_url)
  IPC_STRUCT_TRAITS_MEMBER(contents)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(network::ResourceResponseHead)
  IPC_STRUCT_TRAITS_PARENT(network::ResourceResponseInfo)
  IPC_STRUCT_TRAITS_MEMBER(request_start)
  IPC_STRUCT_TRAITS_MEMBER(response_start)
  IPC_STRUCT_TRAITS_MEMBER(origin_policy)
IPC_STRUCT_TRAITS_END()

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NETWORK_IPC_PARAM_TRAITS_H_
