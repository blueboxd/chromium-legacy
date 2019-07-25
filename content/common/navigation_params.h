// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_NAVIGATION_PARAMS_H_
#define CONTENT_COMMON_NAVIGATION_PARAMS_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/common/content_security_policy/content_security_policy.h"
#include "content/common/content_security_policy/csp_disposition_enum.h"
#include "content/common/frame_message_enums.h"
#include "content/common/prefetched_signed_exchange_info.h"
#include "content/public/common/navigation_policy.h"
#include "content/public/common/page_state.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/referrer.h"
#include "content/public/common/was_activated_option.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/resource_response_info.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/platform/web_mixed_content_context_type.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// Struct keeping track of the Javascript SourceLocation that triggered the
// navigation. This is initialized based on information from Blink at the start
// of navigation, and passed back to Blink when the navigation commits.
struct CONTENT_EXPORT SourceLocation {
  SourceLocation();
  SourceLocation(const std::string& url,
                 unsigned int line_number,
                 unsigned int column_number);
  ~SourceLocation();
  std::string url;
  unsigned int line_number = 0;
  unsigned int column_number = 0;
};

// The following structures hold parameters used during a navigation. In
// particular they are used by FrameMsg_Navigate, FrameHostMsg_BeginNavigation,
// and mojom::FrameNavigationControl.

// Provided by the browser or the renderer -------------------------------------

// Represents the Content Security Policy of the initator of the navigation.
struct CONTENT_EXPORT InitiatorCSPInfo {
  InitiatorCSPInfo();
  InitiatorCSPInfo(CSPDisposition should_check_main_world_csp,
                   const std::vector<ContentSecurityPolicy>& initiator_csp,
                   const base::Optional<CSPSource>& initiator_self_source);
  InitiatorCSPInfo(const InitiatorCSPInfo& other);
  ~InitiatorCSPInfo();

  // Whether or not the CSP of the main world should apply. When the navigation
  // is initiated from a content script in an isolated world, the CSP defined
  // in the main world should not apply.
  // TODO(arthursonzogni): Instead of this boolean, the origin of the isolated
  // world which has initiated the navigation should be passed.
  // See https://crbug.com/702540
  CSPDisposition should_check_main_world_csp = CSPDisposition::CHECK;

  // The relevant CSP policies and the initiator 'self' source to be used.
  std::vector<ContentSecurityPolicy> initiator_csp;
  base::Optional<CSPSource> initiator_self_source;
};

// Used by all navigation IPCs.
struct CONTENT_EXPORT CommonNavigationParams {
  CommonNavigationParams();
  CommonNavigationParams(
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
      base::TimeTicks input_start = base::TimeTicks());
  CommonNavigationParams(const CommonNavigationParams& other);
  ~CommonNavigationParams();

  // The URL to navigate to.
  // May be modified when the navigation is ready to commit.
  GURL url;

  // When a frame navigates another frame, this is the origin of the document
  // which initiated the navigation. This parameter can be null for
  // browser-initiated navigations.
  base::Optional<url::Origin> initiator_origin;

  // The URL to send in the "Referer" header field. Can be empty if there is
  // no referrer.
  Referrer referrer;

  // The type of transition.
  ui::PageTransition transition = ui::PAGE_TRANSITION_LINK;

  // Type of navigation.
  FrameMsg_Navigate_Type::Value navigation_type =
      FrameMsg_Navigate_Type::DIFFERENT_DOCUMENT;

  // Governs how downloads are handled by this navigation.
  NavigationDownloadPolicy download_policy;

  // Informs the RenderView the pending navigation should replace the current
  // history entry when it commits. This is used for cross-process redirects so
  // the transferred navigation can recover the navigation state.
  // This is used by client-side redirects to indicate that when
  // the navigation commits, it should commit in the existing page.
  bool should_replace_current_entry = false;

  // Base URL for use in Blink's SubstituteData.
  // Is only used with data: URLs.
  GURL base_url_for_data_url;

  // History URL for use in Blink's SubstituteData.
  // Is only used with data: URLs.
  GURL history_url_for_data_url;

  // Bitmask that has whether or not to request a Preview version of the
  // document for various preview types or let the browser decide.
  PreviewsState previews_state = PREVIEWS_UNSPECIFIED;

  // The navigationStart time exposed through the Navigation Timing API to JS.
  // If this is for a browser-initiated navigation, this can override the
  // navigation_start value in Blink.
  // For renderer initiated navigations, this will be set on the
  // renderer side and sent with FrameHostMsg_BeginNavigation.
  base::TimeTicks navigation_start = base::TimeTicks::Now();

  // The request method: GET, POST, etc.
  std::string method = "GET";

  // Body of HTTP POST request.
  scoped_refptr<network::ResourceRequestBody> post_data;

  // Information about the Javascript source for this navigation. Used for
  // providing information in console error messages triggered by the
  // navigation. If the navigation was not caused by Javascript, this should
  // not be set.
  base::Optional<SourceLocation> source_location;

  // Whether or not this navigation was started from a context menu.
  bool started_from_context_menu = false;

  // True if the request was user initiated.
  bool has_user_gesture = false;

  // We require a copy of the relevant CSP to perform navigation checks.
  InitiatorCSPInfo initiator_csp_info;

  // The origin trial features activated in the initiator that should be applied
  // in the document being navigated to. The int values are blink
  // OriginTrialFeature enum values. OriginTrialFeature enum is not visible
  // outside of blink (and doesn't need to be) so these values are casted to int
  // as they are passed through content across navigations.
  std::vector<int> initiator_origin_trial_features;

  // The value of the hrefTranslate attribute if this navigation was initiated
  // from a link that had that attribute set.
  std::string href_translate;

  // Whether this is a history navigation in a newly created child frame, in
  // which case the browser process is instructing the renderer process to load
  // a URL from a session history item.  Defaults to false.
  // TODO(ahemery): Move this to BeginNavigationParams once we default to
  // IsPerNavigationMojoInterface().
  bool is_history_navigation_in_new_child_frame = false;

  // The time the input event leading to the navigation occurred. This will
  // not always be set; it depends on the creator of the CommonNavigationParams
  // setting it.
  base::TimeTicks input_start;
};

// Provided by the browser -----------------------------------------------------

// Timings collected in the browser during navigation for the
// Navigation Timing API. Sent to Blink in CommitNavigationParams when
// the navigation is ready to be committed.
struct CONTENT_EXPORT NavigationTiming {
  base::TimeTicks redirect_start;
  base::TimeTicks redirect_end;
  base::TimeTicks fetch_start;
};

// Used by commit IPC messages. Holds the parameters needed by the renderer to
// commit a navigation besides those in CommonNavigationParams.
struct CONTENT_EXPORT CommitNavigationParams {
  CommitNavigationParams();
  CommitNavigationParams(const base::Optional<url::Origin>& origin_to_commit,
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
                         bool should_clear_history_list);
  CommitNavigationParams(const CommitNavigationParams& other);
  ~CommitNavigationParams();

  // The origin to be used for committing the navigation, if specified.
  // This will be an origin that's compatible with the |url| in the
  // CommonNavigationParams; if |url| is data: or about:blank, or the frame has
  // sandbox attributes, this determines the origin of the resulting document.
  // It is specified for session history navigations, for which the origin is
  // known and saved in the FrameNavigationEntry.
  base::Optional<url::Origin> origin_to_commit;

  // Whether or not the user agent override string should be used.
  bool is_overriding_user_agent = false;

  // Any redirect URLs that occurred before |url|. Useful for cross-process
  // navigations; defaults to empty.
  std::vector<GURL> redirects;

  // The ResourceResponseInfos received during redirects.
  std::vector<network::ResourceResponseHead> redirect_response;

  // The RedirectInfos received during redirects.
  std::vector<net::RedirectInfo> redirect_infos;

  // The content type from the request headers for POST requests.
  std::string post_content_type;

  // The original URL & method for this navigation.
  GURL original_url;
  std::string original_method;

  // Whether or not this url should be allowed to access local file://
  // resources.
  bool can_load_local_resources = false;

  // Opaque history state (received by ViewHostMsg_UpdateState).
  PageState page_state;

  // For browser-initiated navigations, this is the unique id of the
  // NavigationEntry being navigated to. (For renderer-initiated navigations it
  // is 0.) If the load succeeds, then this nav_entry_id will be reflected in
  // the resulting FrameHostMsg_DidCommitProvisionalLoad_Params.
  int nav_entry_id = 0;

  // If this is a history navigation, this contains a map of frame unique names
  // to |is_about_blank| for immediate children of the frame being navigated for
  // which there are history items.  The renderer process only needs to check
  // with the browser process for newly created subframes that have these unique
  // names (and only when not staying on about:blank).
  // TODO(creis): Expand this to a data structure including corresponding
  // same-process PageStates for the whole subtree in https://crbug.com/639842.
  std::map<std::string, bool> subframe_unique_names;

  // For browser-initiated navigations, this is true if this is a new entry
  // being navigated to. This is false otherwise. TODO(avi): Remove this when
  // the pending entry situation is made sane and the browser keeps them around
  // long enough to match them via nav_entry_id, above.
  bool intended_as_new_entry = false;

  // For history navigations, this is the offset in the history list of the
  // pending load. For non-history navigations, this will be ignored.
  int pending_history_list_offset = -1;

  // Where its current page contents reside in session history and the total
  // size of the session history list.
  int current_history_list_offset = -1;
  int current_history_list_length = 0;

  // Indicates that the tab was previously discarded.
  // wasDiscarded is exposed on Document after discard, see:
  // https://github.com/WICG/web-lifecycle
  bool was_discarded = false;

  // Indicates whether the navigation is to a view-source:// scheme or not.
  // It is a separate boolean as the view-source scheme is stripped from the
  // URL before it is sent to the renderer process and the RenderFrame needs
  // to be put in special view source mode.
  bool is_view_source = false;

  // Whether session history should be cleared. In that case, the RenderView
  // needs to notify the browser that the clearing was succesful when the
  // navigation commits.
  bool should_clear_history_list = false;

  // Timing of navigation events.
  NavigationTiming navigation_timing;

  // The AppCache host id to be used to identify this navigation.
  base::Optional<base::UnguessableToken> appcache_host_id;

  // Set to |kYes| if a navigation is following the rules of user activation
  // propagation. This is different from |has_user_gesture|
  // (in CommonNavigationParams) as the activation may have happened before
  // the navigation was triggered, for example.
  // In other words, the distinction isn't regarding user activation and user
  // gesture but whether there was an activation prior to the navigation or to
  // start it. `was_activated` will answer the former question while
  // `user_gesture` will answer the latter.
  WasActivatedOption was_activated = WasActivatedOption::kUnknown;

  // A token that should be passed to the browser process in
  // DidCommitProvisionalLoadParams.
  // TODO(clamy): Remove this once NavigationClient has shipped and
  // same-document browser-initiated navigations are properly handled as well.
  base::UnguessableToken navigation_token;

  // Prefetched signed exchanges. Used when SignedExchangeSubresourcePrefetch
  // feature is enabled.
  std::vector<PrefetchedSignedExchangeInfo> prefetched_signed_exchanges;

#if defined(OS_ANDROID)
  // The real content of the data: URL. Only used in Android WebView for
  // implementing LoadDataWithBaseUrl API method to circumvent the restriction
  // on the GURL max length in the IPC layer. Short data: URLs can still be
  // passed in the |CommonNavigationParams::url| field.
  std::string data_url_as_string;
#endif

  // Whether this navigation was browser initiated.
  bool is_browser_initiated = false;
};

}  // namespace content

#endif  // CONTENT_COMMON_NAVIGATION_PARAMS_H_
