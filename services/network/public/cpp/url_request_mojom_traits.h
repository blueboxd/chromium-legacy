// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_URL_REQUEST_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_URL_REQUEST_MOJOM_TRAITS_H_

#include <string>
#include <utility>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/base/file_mojom_traits.h"
#include "mojo/public/cpp/base/file_path_mojom_traits.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/request_priority.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/network_isolation_key_mojom_traits.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/chunked_data_pipe_getter.mojom.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "services/network/public/mojom/url_loader.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    EnumTraits<network::mojom::RequestPriority, net::RequestPriority> {
  static network::mojom::RequestPriority ToMojom(net::RequestPriority priority);
  static bool FromMojom(network::mojom::RequestPriority in,
                        net::RequestPriority* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    EnumTraits<network::mojom::URLRequestReferrerPolicy,
               net::URLRequest::ReferrerPolicy> {
  static network::mojom::URLRequestReferrerPolicy ToMojom(
      net::URLRequest::ReferrerPolicy policy);
  static bool FromMojom(network::mojom::URLRequestReferrerPolicy in,
                        net::URLRequest::ReferrerPolicy* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::URLRequestDataView, network::ResourceRequest> {
  static const std::string& method(const network::ResourceRequest& request) {
    return request.method;
  }
  static const GURL& url(const network::ResourceRequest& request) {
    return request.url;
  }
  static const GURL& site_for_cookies(const network::ResourceRequest& request) {
    return request.site_for_cookies;
  }
  static const base::Optional<url::Origin>& top_frame_origin(
      const network::ResourceRequest& request) {
    return request.top_frame_origin;
  }
  static network::mojom::UpdateNetworkIsolationKeyOnRedirect
  update_network_isolation_key_on_redirect(
      const network::ResourceRequest& request) {
    return request.update_network_isolation_key_on_redirect;
  }
  static const net::NetworkIsolationKey& trusted_network_isolation_key(
      const network::ResourceRequest& request) {
    return request.trusted_network_isolation_key;
  }
  static bool attach_same_site_cookies(
      const network::ResourceRequest& request) {
    return request.attach_same_site_cookies;
  }
  static bool update_first_party_url_on_redirect(
      const network::ResourceRequest& request) {
    return request.update_first_party_url_on_redirect;
  }
  static const base::Optional<url::Origin>& request_initiator(
      const network::ResourceRequest& request) {
    return request.request_initiator;
  }
  static const GURL& referrer(const network::ResourceRequest& request) {
    return request.referrer;
  }
  static net::URLRequest::ReferrerPolicy referrer_policy(
      const network::ResourceRequest& request) {
    return request.referrer_policy;
  }
  static bool is_prerendering(const network::ResourceRequest& request) {
    return request.is_prerendering;
  }
  static const net::HttpRequestHeaders& headers(
      const network::ResourceRequest& request) {
    return request.headers;
  }
  static const net::HttpRequestHeaders& cors_exempt_headers(
      const network::ResourceRequest& request) {
    return request.cors_exempt_headers;
  }
  static int32_t load_flags(const network::ResourceRequest& request) {
    return request.load_flags;
  }
  static bool allow_credentials(const network::ResourceRequest& request) {
    return request.allow_credentials;
  }
  static int32_t plugin_child_id(const network::ResourceRequest& request) {
    return request.plugin_child_id;
  }
  static int32_t resource_type(const network::ResourceRequest& request) {
    return request.resource_type;
  }
  static net::RequestPriority priority(
      const network::ResourceRequest& request) {
    return request.priority;
  }
  static const base::Optional<base::UnguessableToken>& appcache_host_id(
      const network::ResourceRequest& request) {
    return request.appcache_host_id;
  }
  static bool should_reset_appcache(const network::ResourceRequest& request) {
    return request.should_reset_appcache;
  }
  static bool is_external_request(const network::ResourceRequest& request) {
    return request.is_external_request;
  }
  static network::mojom::CorsPreflightPolicy cors_preflight_policy(
      const network::ResourceRequest& request) {
    return request.cors_preflight_policy;
  }
  static bool originated_from_service_worker(
      const network::ResourceRequest& request) {
    return request.originated_from_service_worker;
  }
  static bool skip_service_worker(const network::ResourceRequest& request) {
    return request.skip_service_worker;
  }
  static bool corb_detachable(const network::ResourceRequest& request) {
    return request.corb_detachable;
  }
  static bool corb_excluded(const network::ResourceRequest& request) {
    return request.corb_excluded;
  }
  static network::mojom::RequestMode mode(
      const network::ResourceRequest& request) {
    return request.mode;
  }
  static network::mojom::CredentialsMode credentials_mode(
      const network::ResourceRequest& request) {
    return request.credentials_mode;
  }
  static network::mojom::RedirectMode redirect_mode(
      const network::ResourceRequest& request) {
    return request.redirect_mode;
  }
  static const std::string& fetch_integrity(
      const network::ResourceRequest& request) {
    return request.fetch_integrity;
  }
  static int32_t fetch_request_context_type(
      const network::ResourceRequest& request) {
    return request.fetch_request_context_type;
  }
  static const scoped_refptr<network::ResourceRequestBody>& request_body(
      const network::ResourceRequest& request) {
    return request.request_body;
  }
  static bool keepalive(const network::ResourceRequest& request) {
    return request.keepalive;
  }
  static bool has_user_gesture(const network::ResourceRequest& request) {
    return request.has_user_gesture;
  }
  static bool enable_load_timing(const network::ResourceRequest& request) {
    return request.enable_load_timing;
  }
  static bool enable_upload_progress(const network::ResourceRequest& request) {
    return request.enable_upload_progress;
  }
  static bool do_not_prompt_for_login(const network::ResourceRequest& request) {
    return request.do_not_prompt_for_login;
  }
  static int32_t render_frame_id(const network::ResourceRequest& request) {
    return request.render_frame_id;
  }
  static bool is_main_frame(const network::ResourceRequest& request) {
    return request.is_main_frame;
  }
  static int32_t transition_type(const network::ResourceRequest& request) {
    return request.transition_type;
  }
  static bool allow_download(const network::ResourceRequest& request) {
    return request.allow_download;
  }
  static bool report_raw_headers(const network::ResourceRequest& request) {
    return request.report_raw_headers;
  }
  static int32_t previews_state(const network::ResourceRequest& request) {
    return request.previews_state;
  }
  static bool initiated_in_secure_context(
      const network::ResourceRequest& request) {
    return request.initiated_in_secure_context;
  }
  static bool upgrade_if_insecure(const network::ResourceRequest& request) {
    return request.upgrade_if_insecure;
  }
  static bool is_revalidating(const network::ResourceRequest& request) {
    return request.is_revalidating;
  }
  static bool should_also_use_factory_bound_origin_for_cors(
      const network::ResourceRequest& request) {
    return request.should_also_use_factory_bound_origin_for_cors;
  }
  static const base::Optional<base::UnguessableToken>& throttling_profile_id(
      const network::ResourceRequest& request) {
    return request.throttling_profile_id;
  }
  static const net::HttpRequestHeaders& custom_proxy_pre_cache_headers(
      const network::ResourceRequest& request) {
    return request.custom_proxy_pre_cache_headers;
  }
  static const net::HttpRequestHeaders& custom_proxy_post_cache_headers(
      const network::ResourceRequest& request) {
    return request.custom_proxy_post_cache_headers;
  }
  static bool custom_proxy_use_alternate_proxy_list(
      const network::ResourceRequest& request) {
    return request.custom_proxy_use_alternate_proxy_list;
  }
  static const base::Optional<base::UnguessableToken>& fetch_window_id(
      const network::ResourceRequest& request) {
    return request.fetch_window_id;
  }
  static const base::Optional<std::string>& devtools_request_id(
      const network::ResourceRequest& request) {
    return request.devtools_request_id;
  }
  static bool is_signed_exchange_prefetch_cache_enabled(
      const network::ResourceRequest& request) {
    return request.is_signed_exchange_prefetch_cache_enabled;
  }

  static bool Read(network::mojom::URLRequestDataView data,
                   network::ResourceRequest* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::URLRequestBodyDataView,
                 scoped_refptr<network::ResourceRequestBody>> {
  static bool IsNull(const scoped_refptr<network::ResourceRequestBody>& r) {
    return !r;
  }

  static void SetToNull(scoped_refptr<network::ResourceRequestBody>* output) {
    output->reset();
  }

  static const std::vector<network::DataElement>& elements(
      const scoped_refptr<network::ResourceRequestBody>& r) {
    return *r->elements();
  }

  static uint64_t identifier(
      const scoped_refptr<network::ResourceRequestBody>& r) {
    return r->identifier_;
  }

  static bool contains_sensitive_info(
      const scoped_refptr<network::ResourceRequestBody>& r) {
    return r->contains_sensitive_info_;
  }

  static bool Read(network::mojom::URLRequestBodyDataView data,
                   scoped_refptr<network::ResourceRequestBody>* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::DataElementDataView, network::DataElement> {
  static const network::mojom::DataElementType& type(
      const network::DataElement& element) {
    return element.type_;
  }
  static std::vector<uint8_t> buf(const network::DataElement& element) {
    if (element.bytes_) {
      return std::vector<uint8_t>(element.bytes_,
                                  element.bytes_ + element.length_);
    }
    return std::move(element.buf_);
  }
  static const base::FilePath& path(const network::DataElement& element) {
    return element.path_;
  }
  static base::File file(const network::DataElement& element) {
    return std::move(const_cast<network::DataElement&>(element).file_);
  }
  static const std::string& blob_uuid(const network::DataElement& element) {
    return element.blob_uuid_;
  }
  static network::mojom::DataPipeGetterPtrInfo data_pipe_getter(
      const network::DataElement& element) {
    if (element.type_ != network::mojom::DataElementType::kDataPipe)
      return nullptr;
    return element.CloneDataPipeGetter().PassInterface();
  }
  static network::mojom::ChunkedDataPipeGetterPtrInfo chunked_data_pipe_getter(
      const network::DataElement& element) {
    if (element.type_ != network::mojom::DataElementType::kChunkedDataPipe)
      return nullptr;
    return const_cast<network::DataElement&>(element)
        .ReleaseChunkedDataPipeGetter();
  }
  static uint64_t offset(const network::DataElement& element) {
    return element.offset_;
  }
  static uint64_t length(const network::DataElement& element) {
    return element.length_;
  }
  static const base::Time& expected_modification_time(
      const network::DataElement& element) {
    return element.expected_modification_time_;
  }

  static bool Read(network::mojom::DataElementDataView data,
                   network::DataElement* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_URL_REQUEST_MOJOM_TRAITS_H_
