// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/resource_request.h"

#include "net/base/load_flags.h"

namespace network {

ResourceRequest::ResourceRequest() {}
ResourceRequest::ResourceRequest(const ResourceRequest& request) = default;
ResourceRequest::~ResourceRequest() {}

bool ResourceRequest::EqualsForTesting(const ResourceRequest& request) const {
  return method == request.method && url == request.url &&
         site_for_cookies == request.site_for_cookies &&
         top_frame_origin == request.top_frame_origin &&
         trusted_network_isolation_key ==
             request.trusted_network_isolation_key &&
         update_network_isolation_key_on_redirect ==
             request.update_network_isolation_key_on_redirect &&
         attach_same_site_cookies == request.attach_same_site_cookies &&
         update_first_party_url_on_redirect ==
             request.update_first_party_url_on_redirect &&
         request_initiator == request.request_initiator &&
         referrer == request.referrer &&
         referrer_policy == request.referrer_policy &&
         is_prerendering == request.is_prerendering &&
         headers.ToString() == request.headers.ToString() &&
         cors_exempt_headers.ToString() ==
             request.cors_exempt_headers.ToString() &&
         load_flags == request.load_flags &&
         allow_credentials == request.allow_credentials &&
         plugin_child_id == request.plugin_child_id &&
         resource_type == request.resource_type &&
         priority == request.priority &&
         appcache_host_id == request.appcache_host_id &&
         should_reset_appcache == request.should_reset_appcache &&
         is_external_request == request.is_external_request &&
         cors_preflight_policy == request.cors_preflight_policy &&
         originated_from_service_worker ==
             request.originated_from_service_worker &&
         skip_service_worker == request.skip_service_worker &&
         corb_detachable == request.corb_detachable &&
         corb_excluded == request.corb_excluded && mode == request.mode &&
         credentials_mode == request.credentials_mode &&
         redirect_mode == request.redirect_mode &&
         fetch_integrity == request.fetch_integrity &&
         fetch_request_context_type == request.fetch_request_context_type &&
         request_body == request.request_body &&
         keepalive == request.keepalive &&
         has_user_gesture == request.has_user_gesture &&
         enable_load_timing == request.enable_load_timing &&
         enable_upload_progress == request.enable_upload_progress &&
         do_not_prompt_for_login == request.do_not_prompt_for_login &&
         render_frame_id == request.render_frame_id &&
         is_main_frame == request.is_main_frame &&
         transition_type == request.transition_type &&
         allow_download == request.allow_download &&
         report_raw_headers == request.report_raw_headers &&
         previews_state == request.previews_state &&
         initiated_in_secure_context == request.initiated_in_secure_context &&
         upgrade_if_insecure == request.upgrade_if_insecure &&
         is_revalidating == request.is_revalidating &&
         should_also_use_factory_bound_origin_for_cors ==
             request.should_also_use_factory_bound_origin_for_cors &&
         throttling_profile_id == request.throttling_profile_id &&
         custom_proxy_pre_cache_headers.ToString() ==
             request.custom_proxy_pre_cache_headers.ToString() &&
         custom_proxy_post_cache_headers.ToString() ==
             request.custom_proxy_post_cache_headers.ToString() &&
         custom_proxy_use_alternate_proxy_list ==
             request.custom_proxy_use_alternate_proxy_list &&
         fetch_window_id == request.fetch_window_id &&
         devtools_request_id == request.devtools_request_id &&
         is_signed_exchange_prefetch_cache_enabled ==
             request.is_signed_exchange_prefetch_cache_enabled;
}

bool ResourceRequest::SendsCookies() const {
  return allow_credentials && !(load_flags & net::LOAD_DO_NOT_SEND_COOKIES);
}

bool ResourceRequest::SavesCookies() const {
  return allow_credentials && !(load_flags & net::LOAD_DO_NOT_SAVE_COOKIES);
}

}  // namespace network
