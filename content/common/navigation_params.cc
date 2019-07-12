// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/navigation_params.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace content {

SourceLocation::SourceLocation() = default;

SourceLocation::SourceLocation(const std::string& url,
                               unsigned int line_number,
                               unsigned int column_number)
    : url(url), line_number(line_number), column_number(column_number) {}

SourceLocation::~SourceLocation() = default;

InitiatorCSPInfo::InitiatorCSPInfo() = default;
InitiatorCSPInfo::InitiatorCSPInfo(
    CSPDisposition should_check_main_world_csp,
    const std::vector<ContentSecurityPolicy>& initiator_csp,
    const base::Optional<CSPSource>& initiator_self_source)
    : should_check_main_world_csp(should_check_main_world_csp),
      initiator_csp(initiator_csp),
      initiator_self_source(initiator_self_source) {}
InitiatorCSPInfo::InitiatorCSPInfo(const InitiatorCSPInfo& other) = default;

InitiatorCSPInfo::~InitiatorCSPInfo() = default;

CommonNavigationParams::CommonNavigationParams() = default;

CommonNavigationParams::CommonNavigationParams(
    const GURL& url,
    const base::Optional<url::Origin>& initiator_origin,
    const Referrer& referrer,
    ui::PageTransition transition,
    FrameMsg_Navigate_Type::Value navigation_type,
    NavigationDownloadPolicy download_policy,
    bool should_replace_current_entry,
    const GURL& base_url_for_data_url,
    const GURL& history_url_for_data_url,
    PreviewsState previews_state,
    base::TimeTicks navigation_start,
    std::string method,
    const scoped_refptr<network::ResourceRequestBody>& post_data,
    base::Optional<SourceLocation> source_location,
    bool started_from_context_menu,
    bool has_user_gesture,
    const InitiatorCSPInfo& initiator_csp_info,
    const std::vector<int>& initiator_origin_trial_features,
    const std::string& href_translate,
    bool is_history_navigation_in_new_child_frame,
    base::TimeTicks input_start)
    : url(url),
      initiator_origin(initiator_origin),
      referrer(referrer),
      transition(transition),
      navigation_type(navigation_type),
      download_policy(download_policy),
      should_replace_current_entry(should_replace_current_entry),
      base_url_for_data_url(base_url_for_data_url),
      history_url_for_data_url(history_url_for_data_url),
      previews_state(previews_state),
      navigation_start(navigation_start),
      method(method),
      post_data(post_data),
      source_location(source_location),
      started_from_context_menu(started_from_context_menu),
      has_user_gesture(has_user_gesture),
      initiator_csp_info(initiator_csp_info),
      initiator_origin_trial_features(initiator_origin_trial_features),
      href_translate(href_translate),
      is_history_navigation_in_new_child_frame(
          is_history_navigation_in_new_child_frame),
      input_start(input_start) {
  // |method != "POST"| should imply absence of |post_data|.
  if (method != "POST" && post_data) {
    NOTREACHED();
    this->post_data = nullptr;
  }
}

CommonNavigationParams::CommonNavigationParams(
    const CommonNavigationParams& other) = default;

CommonNavigationParams::~CommonNavigationParams() = default;

CommitNavigationParams::CommitNavigationParams()
    : navigation_token(base::UnguessableToken::Create()) {}

CommitNavigationParams::CommitNavigationParams(
    const base::Optional<url::Origin>& origin_to_commit,
    bool is_overriding_user_agent,
    const std::vector<GURL>& redirects,
    const GURL& original_url,
    const std::string& original_method,
    bool can_load_local_resources,
    const PageState& page_state,
    int nav_entry_id,
    std::map<std::string, bool> subframe_unique_names,
    bool intended_as_new_entry,
    int pending_history_list_offset,
    int current_history_list_offset,
    int current_history_list_length,
    bool is_view_source,
    bool should_clear_history_list)
    : origin_to_commit(origin_to_commit),
      is_overriding_user_agent(is_overriding_user_agent),
      redirects(redirects),
      original_url(original_url),
      original_method(original_method),
      can_load_local_resources(can_load_local_resources),
      page_state(page_state),
      nav_entry_id(nav_entry_id),
      subframe_unique_names(subframe_unique_names),
      intended_as_new_entry(intended_as_new_entry),
      pending_history_list_offset(pending_history_list_offset),
      current_history_list_offset(current_history_list_offset),
      current_history_list_length(current_history_list_length),
      is_view_source(is_view_source),
      should_clear_history_list(should_clear_history_list),
      navigation_token(base::UnguessableToken::Create()) {}

CommitNavigationParams::CommitNavigationParams(
    const CommitNavigationParams& other) = default;

CommitNavigationParams::~CommitNavigationParams() = default;

}  // namespace content
