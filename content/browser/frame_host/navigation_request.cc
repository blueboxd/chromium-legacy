// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_request.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/client_hints/client_hints.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/navigator_impl.h"
#include "content/browser/frame_host/origin_policy_throttle.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/loader/browser_initiated_resource_request.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/common/appcache_interfaces.h"
#include "content/common/content_constants_internal.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/navigation_policy.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/common/web_preferences.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/filename_util.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/common/frame/sandbox_flags.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"
#include "third_party/blink/public/platform/web_mixed_content_context_type.h"
#include "url/url_constants.h"

namespace content {

namespace {

// crbug.com/954271: This feature is a part of an ablation study which makes
// history navigations slower.
// TODO(altimin): Clean this up after the study finishes.
constexpr base::Feature kHistoryNavigationDoNotUseCacheAblationStudy{
    "HistoryNavigationDoNotUseCacheAblationStudy",
    base::FEATURE_DISABLED_BY_DEFAULT};
constexpr base::FeatureParam<double> kDoNotUseCacheProbability{
    &kHistoryNavigationDoNotUseCacheAblationStudy, "probability", 0.0};

// Returns the net load flags to use based on the navigation type.
// TODO(clamy): Remove the blink code that sets the caching flags when
// PlzNavigate launches.
void UpdateLoadFlagsWithCacheFlags(
    int* load_flags,
    FrameMsg_Navigate_Type::Value navigation_type,
    bool is_post) {
  switch (navigation_type) {
    case FrameMsg_Navigate_Type::RELOAD:
    case FrameMsg_Navigate_Type::RELOAD_ORIGINAL_REQUEST_URL:
      *load_flags |= net::LOAD_VALIDATE_CACHE;
      break;
    case FrameMsg_Navigate_Type::RELOAD_BYPASSING_CACHE:
      *load_flags |= net::LOAD_BYPASS_CACHE;
      break;
    case FrameMsg_Navigate_Type::RESTORE:
      *load_flags |= net::LOAD_SKIP_CACHE_VALIDATION;
      break;
    case FrameMsg_Navigate_Type::RESTORE_WITH_POST:
      *load_flags |=
          net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION;
      break;
    case FrameMsg_Navigate_Type::SAME_DOCUMENT:
    case FrameMsg_Navigate_Type::DIFFERENT_DOCUMENT:
      if (is_post)
        *load_flags |= net::LOAD_VALIDATE_CACHE;
      break;
    case FrameMsg_Navigate_Type::HISTORY_SAME_DOCUMENT:
    case FrameMsg_Navigate_Type::HISTORY_DIFFERENT_DOCUMENT:
      if (is_post) {
        *load_flags |=
            net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION;
      } else if (base::FeatureList::IsEnabled(
                     kHistoryNavigationDoNotUseCacheAblationStudy) &&
                 base::RandDouble() < kDoNotUseCacheProbability.Get()) {
        *load_flags |= net::LOAD_BYPASS_CACHE;
      } else {
        *load_flags |= net::LOAD_SKIP_CACHE_VALIDATION;
      }
      break;
  }
}

// TODO(clamy): This should be function in FrameTreeNode.
bool IsSecureFrame(FrameTreeNode* frame) {
  while (frame) {
    if (!IsPotentiallyTrustworthyOrigin(frame->current_origin()))
      return false;
    frame = frame->parent();
  }
  return true;
}

// This should match blink::ResourceRequest::needsHTTPOrigin.
bool NeedsHTTPOrigin(net::HttpRequestHeaders* headers,
                     const std::string& method) {
  // Don't add an Origin header if it is already present.
  if (headers->HasHeader(net::HttpRequestHeaders::kOrigin))
    return false;

  // Don't send an Origin header for GET or HEAD to avoid privacy issues.
  // For example, if an intranet page has a hyperlink to an external web
  // site, we don't want to include the Origin of the request because it
  // will leak the internal host name. Similar privacy concerns have lead
  // to the widespread suppression of the Referer header at the network
  // layer.
  if (method == "GET" || method == "HEAD")
    return false;

  // For non-GET and non-HEAD methods, always send an Origin header so the
  // server knows we support this feature.
  return true;
}

// TODO(clamy): This should match what's happening in
// blink::FrameFetchContext::addAdditionalRequestHeaders.
void AddAdditionalRequestHeaders(net::HttpRequestHeaders* headers,
                                 const GURL& url,
                                 FrameMsg_Navigate_Type::Value navigation_type,
                                 ui::PageTransition transition,
                                 BrowserContext* browser_context,
                                 const std::string& method,
                                 const std::string user_agent_override,
                                 bool has_user_gesture,
                                 base::Optional<url::Origin> initiator_origin,
                                 FrameTreeNode* frame_tree_node) {
  if (!url.SchemeIsHTTPOrHTTPS())
    return;

  const bool is_reload =
      navigation_type == FrameMsg_Navigate_Type::RELOAD ||
      navigation_type == FrameMsg_Navigate_Type::RELOAD_BYPASSING_CACHE ||
      navigation_type == FrameMsg_Navigate_Type::RELOAD_ORIGINAL_REQUEST_URL;
  blink::mojom::RendererPreferences render_prefs =
      frame_tree_node->render_manager()
          ->current_host()
          ->GetDelegate()
          ->GetRendererPrefs(browser_context);
  UpdateAdditionalHeadersForBrowserInitiatedRequest(headers, browser_context,
                                                    is_reload, render_prefs);

  // Tack an 'Upgrade-Insecure-Requests' header to outgoing navigational
  // requests, as described in
  // https://w3c.github.io/webappsec/specs/upgrade/#feature-detect
  headers->SetHeaderIfMissing("Upgrade-Insecure-Requests", "1");

  headers->SetHeaderIfMissing(
      net::HttpRequestHeaders::kUserAgent,
      user_agent_override.empty()
          ? GetContentClient()->browser()->GetUserAgent()
          : user_agent_override);

  // TODO(mkwst): Extract this logic out somewhere that can be shared between
  // Blink and //content.
  if (IsFetchMetadataEnabled() && IsOriginSecure(url)) {
    // Navigations that aren't triggerable from the web (e.g. typing in the
    // address bar, or clicking a bookmark) are labeled as user-initiated.
    std::string user_value = has_user_gesture ? "?1" : std::string();
    if (!PageTransitionIsWebTriggerable(transition))
      user_value = "?1";

    std::string destination;
    std::string mode = "navigate";
    switch (frame_tree_node->frame_owner_element_type()) {
      case blink::FrameOwnerElementType::kNone:
        destination = "document";
        break;
      case blink::FrameOwnerElementType::kObject:
        destination = "object";
        mode = "no-cors";
        break;
      case blink::FrameOwnerElementType::kEmbed:
        destination = "embed";
        mode = "no-cors";
        break;
      case blink::FrameOwnerElementType::kIframe:
      case blink::FrameOwnerElementType::kFrame:
      case blink::FrameOwnerElementType::kPortal:
        // TODO(mkwst): "Portal"'s destination isn't actually defined at the
        // moment. Let's assume it'll be similar to a frame until we decide
        // otherwise.
        destination = "nested-document";
        mode = "nested-navigate";
    }

    if (IsFetchMetadataDestinationEnabled()) {
      headers->SetHeaderIfMissing("Sec-Fetch-Dest", destination.c_str());
    }
    headers->SetHeaderIfMissing("Sec-Fetch-Mode", mode.c_str());
    if (!user_value.empty())
      headers->SetHeaderIfMissing("Sec-Fetch-User", user_value.c_str());
    // Sec-Fetch-Site is covered by network::SetSecFetchSiteHeader function.
  }

  // Ask whether we should request a policy.
  if (OriginPolicyThrottle::ShouldRequestOriginPolicy(url)) {
    headers->SetHeader(net::HttpRequestHeaders::kSecOriginPolicy,
                       kDefaultOriginPolicyVersion);
  }

  // Next, set the HTTP Origin if needed.
  if (!NeedsHTTPOrigin(headers, method))
    return;

  // Create a unique origin.
  url::Origin origin;
  if (frame_tree_node->IsMainFrame()) {
    // For main frame, the origin is the url currently loading.
    origin = url::Origin::Create(url);
  } else if ((frame_tree_node->active_sandbox_flags() &
              blink::WebSandboxFlags::kOrigin) ==
             blink::WebSandboxFlags::kNone) {
    // The origin should be the origin of the root, except for sandboxed
    // frames which have a unique origin.
    origin = frame_tree_node->frame_tree()->root()->current_origin();
  }

  headers->SetHeader(net::HttpRequestHeaders::kOrigin, origin.Serialize());
}

// Should match the definition of
// blink::SchemeRegistry::ShouldTreatURLSchemeAsLegacy.
bool ShouldTreatURLSchemeAsLegacy(const GURL& url) {
  return url.SchemeIs(url::kFtpScheme) || url.SchemeIs(url::kGopherScheme);
}

bool ShouldPropagateUserActivation(const url::Origin& previous_origin,
                                   const url::Origin& new_origin) {
  if ((previous_origin.scheme() != "http" &&
       previous_origin.scheme() != "https") ||
      (new_origin.scheme() != "http" && new_origin.scheme() != "https")) {
    return false;
  }

  if (previous_origin.host() == new_origin.host())
    return true;

  std::string previous_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          previous_origin.host(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  std::string new_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          new_origin.host(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return !previous_domain.empty() && previous_domain == new_domain;
}

// LOG_NAVIGATION_TIMING_HISTOGRAM logs |value| for "Navigation.<histogram>" UMA
// as well as supplementary UMAs (depending on |transition| and |is_background|)
// for BackForward/Reload/NewNavigation variants.
//
// kMaxTime and kBuckets constants are consistent with
// UMA_HISTOGRAM_MEDIUM_TIMES, but a custom kMinTime is used for high fidelity
// near the low end of measured values.
//
// TODO(csharrison,nasko): This macro is incorrect for subframe navigations,
// which will only have subframe-specific transition types. This means that all
// subframes currently are tagged as NewNavigations.
#define LOG_NAVIGATION_TIMING_HISTOGRAM(histogram, transition, is_background, \
                                        duration)                             \
  do {                                                                        \
    const base::TimeDelta kMinTime = base::TimeDelta::FromMilliseconds(1);    \
    const base::TimeDelta kMaxTime = base::TimeDelta::FromMinutes(3);         \
    const int kBuckets = 50;                                                  \
    UMA_HISTOGRAM_CUSTOM_TIMES("Navigation." histogram, duration, kMinTime,   \
                               kMaxTime, kBuckets);                           \
    if (transition & ui::PAGE_TRANSITION_FORWARD_BACK) {                      \
      UMA_HISTOGRAM_CUSTOM_TIMES("Navigation." histogram ".BackForward",      \
                                 duration, kMinTime, kMaxTime, kBuckets);     \
    } else if (ui::PageTransitionCoreTypeIs(transition,                       \
                                            ui::PAGE_TRANSITION_RELOAD)) {    \
      UMA_HISTOGRAM_CUSTOM_TIMES("Navigation." histogram ".Reload", duration, \
                                 kMinTime, kMaxTime, kBuckets);               \
    } else if (ui::PageTransitionIsNewNavigation(transition)) {               \
      UMA_HISTOGRAM_CUSTOM_TIMES("Navigation." histogram ".NewNavigation",    \
                                 duration, kMinTime, kMaxTime, kBuckets);     \
    } else {                                                                  \
      NOTREACHED() << "Invalid page transition: " << transition;              \
    }                                                                         \
    if (is_background.has_value()) {                                          \
      if (is_background.value()) {                                            \
        UMA_HISTOGRAM_CUSTOM_TIMES("Navigation." histogram                    \
                                   ".BackgroundProcessPriority",              \
                                   duration, kMinTime, kMaxTime, kBuckets);   \
      } else {                                                                \
        UMA_HISTOGRAM_CUSTOM_TIMES("Navigation." histogram                    \
                                   ".ForegroundProcessPriority",              \
                                   duration, kMinTime, kMaxTime, kBuckets);   \
      }                                                                       \
    }                                                                         \
  } while (0)

void RecordStartToCommitMetrics(base::TimeTicks navigation_start_time,
                                ui::PageTransition transition,
                                const base::TimeTicks& ready_to_commit_time,
                                base::Optional<bool> is_background,
                                bool is_same_process,
                                bool is_main_frame) {
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta delta = now - navigation_start_time;
  LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit", transition, is_background,
                                  delta);
  if (is_main_frame) {
    LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.MainFrame", transition,
                                    is_background, delta);
  } else {
    LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.Subframe", transition,
                                    is_background, delta);
  }
  if (is_same_process) {
    LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.SameProcess", transition,
                                    is_background, delta);
    if (is_main_frame) {
      LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.SameProcess.MainFrame",
                                      transition, is_background, delta);
    } else {
      LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.SameProcess.Subframe",
                                      transition, is_background, delta);
    }
  } else {
    LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.CrossProcess", transition,
                                    is_background, delta);
    if (is_main_frame) {
      LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.CrossProcess.MainFrame",
                                      transition, is_background, delta);
    } else {
      LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.CrossProcess.Subframe",
                                      transition, is_background, delta);
    }
  }
  if (!ready_to_commit_time.is_null()) {
    LOG_NAVIGATION_TIMING_HISTOGRAM("ReadyToCommitUntilCommit2", transition,
                                    is_background, now - ready_to_commit_time);
  }
}

void RecordReadyToCommitMetrics(base::TimeTicks navigation_start_time,
                                ui::PageTransition transition,
                                const base::TimeTicks& ready_to_commit_time,
                                bool is_same_process,
                                bool is_main_frame) {
  constexpr base::Optional<bool> kIsBackground = base::nullopt;
  base::TimeDelta delta = ready_to_commit_time - navigation_start_time;
  LOG_NAVIGATION_TIMING_HISTOGRAM("TimeToReadyToCommit2", transition,
                                  kIsBackground, delta);
  if (is_main_frame) {
    LOG_NAVIGATION_TIMING_HISTOGRAM("TimeToReadyToCommit2.MainFrame",
                                    transition, kIsBackground, delta);
  } else {
    LOG_NAVIGATION_TIMING_HISTOGRAM("TimeToReadyToCommit2.Subframe", transition,
                                    kIsBackground, delta);
  }
  if (is_same_process) {
    LOG_NAVIGATION_TIMING_HISTOGRAM("TimeToReadyToCommit2.SameProcess",
                                    transition, kIsBackground, delta);
  } else {
    LOG_NAVIGATION_TIMING_HISTOGRAM("TimeToReadyToCommit2.CrossProcess",
                                    transition, kIsBackground, delta);
  }
}

void RecordIsSameProcessMetrics(ui::PageTransition transition,
                                bool is_same_process) {
  // Log overall value, then log specific value per type of navigation.
  UMA_HISTOGRAM_BOOLEAN("Navigation.IsSameProcess", is_same_process);

  if (transition & ui::PAGE_TRANSITION_FORWARD_BACK) {
    UMA_HISTOGRAM_BOOLEAN("Navigation.IsSameProcess.BackForward",
                          is_same_process);
    return;
  }
  if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD)) {
    UMA_HISTOGRAM_BOOLEAN("Navigation.IsSameProcess.Reload", is_same_process);
    return;
  }
  if (ui::PageTransitionIsNewNavigation(transition)) {
    UMA_HISTOGRAM_BOOLEAN("Navigation.IsSameProcess.NewNavigation",
                          is_same_process);
    return;
  }
  NOTREACHED() << "Invalid page transition: " << transition;
}

// Use this to get a new unique ID for a NavigationHandle during construction.
// The returned ID is guaranteed to be nonzero (zero is the "no ID" indicator).
int64_t CreateUniqueHandleID() {
  static int64_t unique_id_counter = 0;
  return ++unique_id_counter;
}

}  // namespace

// static
std::unique_ptr<NavigationRequest> NavigationRequest::CreateBrowserInitiated(
    FrameTreeNode* frame_tree_node,
    const CommonNavigationParams& common_params,
    const CommitNavigationParams& commit_params,
    bool browser_initiated,
    const std::string& extra_headers,
    const FrameNavigationEntry& frame_entry,
    NavigationEntryImpl* entry,
    const scoped_refptr<network::ResourceRequestBody>& post_body,
    std::unique_ptr<NavigationUIData> navigation_ui_data) {
  // TODO(arthursonzogni): Form submission with the "GET" method is possible.
  // This is not currently handled here.
  bool is_form_submission = !!post_body;

  auto navigation_params = mojom::BeginNavigationParams::New(
      extra_headers, net::LOAD_NORMAL, false /* skip_service_worker */,
      blink::mojom::RequestContextType::LOCATION,
      blink::WebMixedContentContextType::kBlockable, is_form_submission,
      false /* was_initiated_by_link_click */, GURL() /* searchable_form_url */,
      std::string() /* searchable_form_encoding */,
      GURL() /* client_side_redirect_url */,
      base::nullopt /* devtools_initiator_info */);

  // Shift-Reload forces bypassing caches and service workers.
  if (common_params.navigation_type ==
      FrameMsg_Navigate_Type::RELOAD_BYPASSING_CACHE) {
    navigation_params->load_flags |= net::LOAD_BYPASS_CACHE;
    navigation_params->skip_service_worker = true;
  }

  std::unique_ptr<NavigationRequest> navigation_request(new NavigationRequest(
      frame_tree_node, common_params, std::move(navigation_params),
      commit_params, browser_initiated, false /* from_begin_navigation */,
      false /* is_for_commit */, &frame_entry, entry,
      std::move(navigation_ui_data), nullptr, nullptr));
  navigation_request->blob_url_loader_factory_ =
      frame_entry.blob_url_loader_factory();

  if (blink::BlobUtils::MojoBlobURLsEnabled() &&
      common_params.url.SchemeIsBlob() &&
      !navigation_request->blob_url_loader_factory_) {
    // If this navigation entry came from session history then the blob factory
    // would have been cleared in NavigationEntryImpl::ResetForCommit(). This is
    // avoid keeping large blobs alive unnecessarily and the spec is unclear. So
    // create a new blob factory which will work if the blob happens to still be
    // alive.
    navigation_request->blob_url_loader_factory_ =
        ChromeBlobStorageContext::URLLoaderFactoryForUrl(
            frame_tree_node->navigator()->GetController()->GetBrowserContext(),
            common_params.url);
  }

  return navigation_request;
}

// static
std::unique_ptr<NavigationRequest> NavigationRequest::CreateRendererInitiated(
    FrameTreeNode* frame_tree_node,
    NavigationEntryImpl* entry,
    const CommonNavigationParams& common_params,
    mojom::BeginNavigationParamsPtr begin_params,
    int current_history_list_offset,
    int current_history_list_length,
    bool override_user_agent,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    mojom::NavigationClientAssociatedPtrInfo navigation_client,
    blink::mojom::NavigationInitiatorPtr navigation_initiator,
    scoped_refptr<PrefetchedSignedExchangeCache>
        prefetched_signed_exchange_cache) {
  // Only normal navigations to a different document or reloads are expected.
  // - Renderer-initiated fragment-navigations never take place in the browser,
  //   even with PlzNavigate.
  // - Restore-navigations are always browser-initiated.
  // - History-navigations use the browser-initiated path, event the ones that
  //   are initiated by a javascript script, please see the IPC message
  //   FrameHostMsg_GoToEntryAtOffset.
  DCHECK(FrameMsg_Navigate_Type::IsReload(common_params.navigation_type) ||
         common_params.navigation_type ==
             FrameMsg_Navigate_Type::DIFFERENT_DOCUMENT);

  // TODO(clamy): See if the navigation start time should be measured in the
  // renderer and sent to the browser instead of being measured here.
  CommitNavigationParams commit_params(
      base::nullopt, override_user_agent,
      std::vector<GURL>(),  // redirects
      common_params.url, common_params.method,
      false,                          // can_load_local_resources
      PageState(),                    // page_state
      0,                              // nav_entry_id
      std::map<std::string, bool>(),  // subframe_unique_names
      false,                          // intended_as_new_entry
      -1,  // |pending_history_list_offset| is set to -1 because
           // history-navigations do not use this path. See comments above.
      current_history_list_offset, current_history_list_length,
      false,  // is_view_source
      false /*should_clear_history_list*/);
  std::unique_ptr<NavigationRequest> navigation_request(new NavigationRequest(
      frame_tree_node, common_params, std::move(begin_params), commit_params,
      false,  // browser_initiated
      true,   // from_begin_navigation
      false,  // is_for_commit
      nullptr, entry,
      nullptr,  // navigation_ui_data
      std::move(navigation_client), std::move(navigation_initiator)));
  navigation_request->blob_url_loader_factory_ =
      std::move(blob_url_loader_factory);
  navigation_request->prefetched_signed_exchange_cache_ =
      std::move(prefetched_signed_exchange_cache);
  return navigation_request;
}

// static
std::unique_ptr<NavigationRequest> NavigationRequest::CreateForCommit(
    FrameTreeNode* frame_tree_node,
    RenderFrameHostImpl* render_frame_host,
    NavigationEntryImpl* entry,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    bool is_renderer_initiated,
    bool is_same_document) {
  // TODO(clamy): Improve the *NavigationParams and *CommitParams to avoid
  // copying so many parameters here.
  CommonNavigationParams common_params(
      params.url,
      // TODO(nasko): Investigate better value to pass for |initiator_origin|.
      params.origin, params.referrer, params.transition,
      is_same_document ? FrameMsg_Navigate_Type::SAME_DOCUMENT
                       : FrameMsg_Navigate_Type::DIFFERENT_DOCUMENT,
      NavigationDownloadPolicy(), params.should_replace_current_entry,
      params.base_url, params.base_url, PREVIEWS_UNSPECIFIED,
      base::TimeTicks::Now(), params.method, nullptr,
      base::Optional<SourceLocation>(), false /* started_from_context_menu */,
      params.gesture == NavigationGestureUser, InitiatorCSPInfo(),
      std::vector<int>() /* initiator_origin_trial_features */,
      std::string() /* href_translate */,
      false /* is_history_navigation_in_new_child_frame */,
      base::TimeTicks::Now());
  CommitNavigationParams commit_params(
      params.origin, params.is_overriding_user_agent, params.redirects,
      params.original_request_url, params.method,
      false /* can_load_local_resources */, params.page_state,
      params.nav_entry_id,
      std::map<std::string, bool>() /* subframe_unique_names */,
      params.intended_as_new_entry, -1 /* pending_history_list_offset */,
      -1 /* current_history_list_offset */,
      -1 /* current_history_list_length */, false /* is_view_source */,
      params.history_list_was_cleared);
  mojom::BeginNavigationParamsPtr begin_params =
      mojom::BeginNavigationParams::New();
  std::unique_ptr<NavigationRequest> navigation_request(new NavigationRequest(
      frame_tree_node, common_params, std::move(begin_params), commit_params,
      !is_renderer_initiated, false /* from_begin_navigation */,
      true /* is_for_commit */,
      entry ? entry->GetFrameEntry(frame_tree_node) : nullptr, entry,
      nullptr /* navigation_ui_data */,
      mojom::NavigationClientAssociatedPtrInfo(),
      blink::mojom::NavigationInitiatorPtr()));

  // Update the state of the NavigationRequest to match the fact that the
  // navigation just committed.
  navigation_request->state_ = RESPONSE_STARTED;
  navigation_request->render_frame_host_ = render_frame_host;
  navigation_request->CreateNavigationHandle(true);
  DCHECK(navigation_request->navigation_handle());
  return navigation_request;
}

NavigationRequest::NavigationRequest(
    FrameTreeNode* frame_tree_node,
    const CommonNavigationParams& common_params,
    mojom::BeginNavigationParamsPtr begin_params,
    const CommitNavigationParams& commit_params,
    bool browser_initiated,
    bool from_begin_navigation,
    bool is_for_commit,
    const FrameNavigationEntry* frame_entry,
    NavigationEntryImpl* entry,
    std::unique_ptr<NavigationUIData> navigation_ui_data,
    mojom::NavigationClientAssociatedPtrInfo navigation_client,
    blink::mojom::NavigationInitiatorPtr navigation_initiator)
    : frame_tree_node_(frame_tree_node),
      common_params_(common_params),
      begin_params_(std::move(begin_params)),
      commit_params_(commit_params),
      browser_initiated_(browser_initiated),
      navigation_ui_data_(std::move(navigation_ui_data)),
      state_(NOT_STARTED),
      restore_type_(RestoreType::NONE),
      is_view_source_(false),
      bindings_(NavigationEntryImpl::kInvalidBindings),
      response_should_be_rendered_(true),
      associated_site_instance_type_(AssociatedSiteInstanceType::NONE),
      from_begin_navigation_(from_begin_navigation),
      has_stale_copy_in_cache_(false),
      net_error_(net::OK),
      expected_render_process_host_id_(ChildProcessHost::kInvalidUniqueID),
      devtools_navigation_token_(base::UnguessableToken::Create()),
      request_navigation_client_(nullptr),
      commit_navigation_client_(nullptr) {
  DCHECK(browser_initiated || common_params.initiator_origin.has_value());
  DCHECK(!browser_initiated || (entry != nullptr && frame_entry != nullptr));
  DCHECK(!IsRendererDebugURL(common_params_.url));
  DCHECK(common_params_.method == "POST" || !common_params_.post_data);
  TRACE_EVENT_ASYNC_BEGIN2("navigation", "NavigationRequest", this,
                           "frame_tree_node",
                           frame_tree_node_->frame_tree_node_id(), "url",
                           common_params_.url.possibly_invalid_spec());

  // Setup for navigation to the BundledExchanges.
  if (GetContentClient()->browser()->CanAcceptUntrustedExchangesIfNeeded() &&
      common_params_.url.SchemeIsFile() &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTrustableBundledExchangesFile)) {
    // A user intends navigation to the BundledExchanges for testing.
    const base::FilePath specified_path =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            switches::kTrustableBundledExchangesFile);
    base::FilePath url_path;
    if (net::FileURLToFilePath(common_params_.url, &url_path) &&
        url_path == specified_path) {
      bundled_exchanges_factory_ = std::make_unique<BundledExchangesFactory>(
          BundledExchangesSource(specified_path));
    }
  }

  // Sanitize the referrer.
  common_params_.referrer =
      Referrer::SanitizeForRequest(common_params_.url, common_params_.referrer);

  if (from_begin_navigation_) {
    // This is needed to have data URLs commit in the same SiteInstance as the
    // initiating renderer.
    source_site_instance_ =
        frame_tree_node->current_frame_host()->GetSiteInstance();

    if (IsPerNavigationMojoInterfaceEnabled()) {
      DCHECK(navigation_client.is_valid());
      SetNavigationClient(std::move(navigation_client),
                          source_site_instance_->GetId());
    }
  } else if (entry) {
    DCHECK(!navigation_client.is_valid());
    FrameNavigationEntry* frame_navigation_entry =
        entry->GetFrameEntry(frame_tree_node);
    if (frame_navigation_entry) {
      source_site_instance_ = frame_navigation_entry->source_site_instance();
      dest_site_instance_ = frame_navigation_entry->site_instance();
    }
    restore_type_ = entry->restore_type();
    is_view_source_ = entry->IsViewSourceMode();
    bindings_ = entry->bindings();
  }

  // Update the load flags with cache information.
  UpdateLoadFlagsWithCacheFlags(&begin_params_->load_flags,
                                common_params_.navigation_type,
                                common_params_.method == "POST");

  // Add necessary headers that may not be present in the
  // mojom::BeginNavigationParams.
  if (entry) {
    nav_entry_id_ = entry->GetUniqueID();
    // TODO(altimin, crbug.com/933147): Remove this logic after we are done
    // with implementing back-forward cache.
    if (frame_tree_node->IsMainFrame() && entry->back_forward_cache_metrics()) {
      entry->back_forward_cache_metrics()
          ->MainFrameDidStartNavigationToDocument();
    }
  }

  std::string user_agent_override;
  if (commit_params.is_overriding_user_agent ||
      (entry && entry->GetIsOverridingUserAgent())) {
    user_agent_override =
        frame_tree_node_->navigator()->GetDelegate()->GetUserAgentOverride();
  }

  net::HttpRequestHeaders headers;
  // Only add specific headers when creating a NavigationRequest before the
  // network request is made, not at commit time.
  if (!is_for_commit) {
    BrowserContext* browser_context =
        frame_tree_node_->navigator()->GetController()->GetBrowserContext();
    ClientHintsControllerDelegate* client_hints_delegate =
        browser_context->GetClientHintsControllerDelegate();
    if (client_hints_delegate) {
      net::HttpRequestHeaders client_hints_headers;
      AddNavigationRequestClientHintsHeaders(
          common_params_.url, &client_hints_headers, browser_context,
          client_hints_delegate);
      headers.MergeFrom(client_hints_headers);
    }

    headers.AddHeadersFromString(begin_params_->headers);
    AddAdditionalRequestHeaders(
        &headers, common_params_.url, common_params_.navigation_type,
        common_params_.transition,
        frame_tree_node_->navigator()->GetController()->GetBrowserContext(),
        common_params.method, user_agent_override,
        common_params_.has_user_gesture, common_params.initiator_origin,
        frame_tree_node);

    if (begin_params_->is_form_submission) {
      if (browser_initiated && !commit_params.post_content_type.empty()) {
        // This is a form resubmit, so make sure to set the Content-Type header.
        headers.SetHeaderIfMissing(net::HttpRequestHeaders::kContentType,
                                   commit_params.post_content_type);
      } else if (!browser_initiated) {
        // Save the Content-Type in case the form is resubmitted. This will get
        // sent back to the renderer in the CommitNavigation IPC. The renderer
        // will then send it back with the post body so that we can access it
        // along with the body in FrameNavigationEntry::page_state_.
        headers.GetHeader(net::HttpRequestHeaders::kContentType,
                          &commit_params_.post_content_type);
      }
    }
  }

  begin_params_->headers = headers.ToString();

  initiator_csp_context_.reset(new InitiatorCSPContext(
      common_params_.initiator_csp_info.initiator_csp,
      common_params_.initiator_csp_info.initiator_self_source,
      std::move(navigation_initiator)));

  navigation_entry_offset_ = EstimateHistoryOffset();

  commit_params_.is_browser_initiated = browser_initiated_;
}

NavigationRequest::~NavigationRequest() {
  TRACE_EVENT_ASYNC_END0("navigation", "NavigationRequest", this);
  ResetExpectedProcess();
  if (state_ == STARTED) {
    devtools_instrumentation::OnNavigationRequestFailed(
        *this, network::URLLoaderCompletionStatus(net::ERR_ABORTED));
  }

#if defined(OS_ANDROID)
  if (navigation_handle_proxy_)
    navigation_handle_proxy_->DidFinish();
#endif

  // This is done manually here because the NavigationHandle destructor
  // calls into WebContentsObserver::DidFinishNavigation, some of which need to
  // then access navigation_request(). This is only possible if the handle is
  // destroyed before the NavigationRequest.
  navigation_handle_.reset();
}

void NavigationRequest::BeginNavigation() {
  DCHECK(state_ == NOT_STARTED || state_ == WAITING_FOR_RENDERER_RESPONSE);
  TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationRequest", this,
                               "BeginNavigation");
  DCHECK(!loader_);
  DCHECK(!render_frame_host_);

  state_ = STARTED;

#if defined(OS_ANDROID)
  base::WeakPtr<NavigationRequest> this_ptr(weak_factory_.GetWeakPtr());
  bool should_override_url_loading = false;

  if (!GetContentClient()->browser()->ShouldOverrideUrlLoading(
          frame_tree_node_->frame_tree_node_id(), browser_initiated_,
          commit_params_.original_url, commit_params_.original_method,
          common_params_.has_user_gesture, false,
          frame_tree_node_->IsMainFrame(), common_params_.transition,
          &should_override_url_loading)) {
    // A Java exception was thrown by the embedding application; we
    // need to return from this task. Specifically, it's not safe from
    // this point on to make any JNI calls.
    return;
  }

  // The content/ embedder might cause |this| to be deleted while
  // |ShouldOverrideUrlLoading| is called.
  // See https://crbug.com/770157.
  if (!this_ptr)
    return;

  if (should_override_url_loading) {
    // Don't create a NavigationHandle here to simulate what happened with the
    // old navigation code path (i.e. doesn't fire onPageFinished notification
    // for aborted loads).
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_ABORTED),
        false /*skip_throttles*/, base::nullopt /*error_page_content*/,
        false /*collapse_frame*/);
    return;
  }
#endif

  // Check Content Security Policy before the NavigationThrottles run. This
  // gives CSP a chance to modify requests that NavigationThrottles would
  // otherwise block. Similarly, the NavigationHandle is created afterwards, so
  // that it gets the request URL after potentially being modified by CSP.
  net::Error net_error = CheckContentSecurityPolicy(
      false /* has_followed redirect */,
      false /* url_upgraded_after_redirect */, false /* is_response_check */);
  if (net_error != net::OK) {
    // Create a navigation handle so that the correct error code can be set on
    // it by OnRequestFailedInternal().
    CreateNavigationHandle(false);
    OnRequestFailedInternal(network::URLLoaderCompletionStatus(net_error),
                            false /* skip_throttles */,
                            base::nullopt /* error_page_content */,
                            false /* collapse_frame */);
    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  if (CheckCredentialedSubresource() ==
          CredentialedSubresourceCheckResult::BLOCK_REQUEST ||
      CheckLegacyProtocolInSubresource() ==
          LegacyProtocolInSubresourceCheckResult::BLOCK_REQUEST) {
    // Create a navigation handle so that the correct error code can be set on
    // it by OnRequestFailedInternal().
    CreateNavigationHandle(false);
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_ABORTED),
        false /* skip_throttles  */, base::nullopt /* error_page_content */,
        false /* collapse_frame */);

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  CreateNavigationHandle(false);

  if (CheckAboutSrcDoc() == AboutSrcDocCheckResult::BLOCK_REQUEST) {
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_INVALID_URL),
        true /* skip_throttles */, base::nullopt /* error_page_content*/,
        false /* collapse_frame */);
    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  if (!NeedsUrlLoader()) {
    // There is no need to make a network request for this navigation, so commit
    // it immediately.
    TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationRequest", this,
                                 "ResponseStarted");
    state_ = RESPONSE_STARTED;

    // Select an appropriate RenderFrameHost.
    render_frame_host_ =
        frame_tree_node_->render_manager()->GetFrameHostForNavigation(*this);
    NavigatorImpl::CheckWebUIRendererDoesNotDisplayNormalURL(
        render_frame_host_, common_params_.url);

    ReadyToCommitNavigation(false /* is_error */);
    CommitNavigation();
    return;
  }

  common_params_.previews_state =
      GetContentClient()->browser()->DetermineAllowedPreviews(
          common_params_.previews_state, navigation_handle_.get(),
          common_params_.url);

  // It's safe to use base::Unretained because this NavigationRequest owns
  // the NavigationHandle where the callback will be stored.
  // TODO(clamy): pass the method to the NavigationHandle instead of a
  // boolean.
  WillStartRequest(base::Bind(&NavigationRequest::OnStartChecksComplete,
                              base::Unretained(this)));
}

void NavigationRequest::SetWaitingForRendererResponse() {
  TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationRequest", this,
                               "WaitingForRendererResponse");
  DCHECK(state_ == NOT_STARTED);
  state_ = WAITING_FOR_RENDERER_RESPONSE;
}

void NavigationRequest::CreateNavigationHandle(bool is_for_commit) {
  DCHECK(frame_tree_node_->navigation_request() == this || is_for_commit);
  FrameTreeNode* frame_tree_node = frame_tree_node_;

  // This is needed to get site URLs and assign the expected RenderProcessHost.
  // This is not always the same as |source_site_instance_|, as it only depends
  // on the current frame host, and does not depend on |entry|.
  // The |starting_site_instance_| needs to be set here instead of the
  // constructor since a navigation can be started after the constructor and
  // before here, which can set a different RenderFrameHost and a different
  // starting SiteInstance.
  starting_site_instance_ =
      frame_tree_node->current_frame_host()->GetSiteInstance();

  site_url_ = GetSiteForCommonParamsURL();

  // Compute the redirect chain.
  // TODO(clamy): Try to simplify this and have the redirects be part of
  // CommonNavigationParams.
  redirect_chain_.clear();
  if (!begin_params_->client_side_redirect_url.is_empty()) {
    // |begin_params_->client_side_redirect_url| will be set when the navigation
    // was triggered by a client-side redirect.
    redirect_chain_.push_back(begin_params_->client_side_redirect_url);
  } else if (!commit_params_.redirects.empty()) {
    // Redirects that were specified at NavigationRequest creation time should
    // be added to the list of redirects. In particular, if the
    // NavigationRequest was created at commit time, redirects that happened
    // during the navigation have been added to |commit_params_.redirects| and
    // should be passed to the NavigationHandle.
    for (const auto& url : commit_params_.redirects)
      redirect_chain_.push_back(url);
  }

  // Finally, add the current URL to the vector of redirects.
  // Note: for NavigationRequests created at commit time, the current URL has
  // been added to |commit_params_.redirects|, so don't add it a second time.
  if (!is_for_commit)
    redirect_chain_.push_back(common_params_.url);

  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(begin_params_->headers);

  // Mirrors the logic in RenderFrameImpl::SendDidCommitProvisionalLoad.
  if (common_params_.transition & ui::PAGE_TRANSITION_CLIENT_REDIRECT) {
    // If the page contained a client redirect (meta refresh,
    // document.location), set the referrer appropriately.
    sanitized_referrer_ = Referrer(
        redirect_chain_[0], Referrer::SanitizeForRequest(
                                common_params_.url, common_params_.referrer)
                                .policy);
  } else {
    sanitized_referrer_ = Referrer::SanitizeForRequest(common_params_.url,
                                                       common_params_.referrer);
  }

  handle_state_ = NavigationRequest::INITIAL;
  navigation_handle_id_ = CreateUniqueHandleID();

  std::unique_ptr<NavigationHandleImpl> navigation_handle = base::WrapUnique(
      new NavigationHandleImpl(this, nav_entry_id_, std::move(headers)));

  if (!frame_tree_node->navigation_request() && !is_for_commit) {
    // A callback could have cancelled this request synchronously in which case
    // |this| is deleted.
    return;
  }

  navigation_handle_ = std::move(navigation_handle);

  throttle_runner_ = base::WrapUnique(
      new NavigationThrottleRunner(this, navigation_handle_.get()));

#if defined(OS_ANDROID)
  navigation_handle_proxy_ =
      std::make_unique<NavigationHandleProxy>(navigation_handle_.get());
#endif

  GetDelegate()->DidStartNavigation(navigation_handle_.get());
}

void NavigationRequest::ResetForCrossDocumentRestart() {
  DCHECK(IsSameDocument());

  // Reset the NavigationHandle, which is now incorrectly marked as
  // same-document. Ensure |loader_| does not exist as it can hold raw pointers
  // to objects owned by the handle (see the comment in the header).
  DCHECK(!loader_);

#if defined(OS_ANDROID)
  if (navigation_handle_proxy_)
    navigation_handle_proxy_->DidFinish();
#endif

  // The below order of resets is necessary to avoid accessing null pointers.
  // See https://crbug.com/958396.
  navigation_handle_.reset();

#if defined(OS_ANDROID)
  if (navigation_handle_proxy_)
    navigation_handle_proxy_.reset();
#endif

  // Reset the previously selected RenderFrameHost. This is expected to be null
  // at the beginning of a new navigation. See https://crbug.com/936962.
  DCHECK(render_frame_host_);
  render_frame_host_ = nullptr;

  // Convert the navigation type to the appropriate cross-document one.
  if (common_params_.navigation_type ==
      FrameMsg_Navigate_Type::HISTORY_SAME_DOCUMENT) {
    common_params_.navigation_type =
        FrameMsg_Navigate_Type::HISTORY_DIFFERENT_DOCUMENT;
  } else {
    DCHECK(common_params_.navigation_type ==
           FrameMsg_Navigate_Type::SAME_DOCUMENT);
    common_params_.navigation_type = FrameMsg_Navigate_Type::DIFFERENT_DOCUMENT;
  }

  // Reset the state of the NavigationRequest.
  state_ = NOT_STARTED;
}

void NavigationRequest::RegisterSubresourceOverride(
    mojom::TransferrableURLLoaderPtr transferrable_loader) {
  if (!subresource_overrides_)
    subresource_overrides_.emplace();

  subresource_overrides_->push_back(std::move(transferrable_loader));
}

mojom::NavigationClient* NavigationRequest::GetCommitNavigationClient() {
  if (commit_navigation_client_ && commit_navigation_client_.is_bound())
    return commit_navigation_client_.get();

  // Instantiate a new NavigationClient interface.
  commit_navigation_client_ =
      render_frame_host_->GetNavigationClientFromInterfaceProvider();
  HandleInterfaceDisconnection(
      &commit_navigation_client_,
      base::BindOnce(&NavigationRequest::OnRendererAbortedNavigation,
                     base::Unretained(this)));
  return commit_navigation_client_.get();
}

void NavigationRequest::SetOriginPolicy(const network::OriginPolicy& policy) {
  response_head_->head.origin_policy = policy;
}

void NavigationRequest::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    const scoped_refptr<network::ResourceResponse>& response_head) {
  response_head_ = response_head;
  ssl_info_ = response_head->head.ssl_info;
  auth_challenge_info_ = response_head->head.auth_challenge_info;
#if defined(OS_ANDROID)
  base::WeakPtr<NavigationRequest> this_ptr(weak_factory_.GetWeakPtr());

  bool should_override_url_loading = false;
  if (!GetContentClient()->browser()->ShouldOverrideUrlLoading(
          frame_tree_node_->frame_tree_node_id(), browser_initiated_,
          redirect_info.new_url, redirect_info.new_method,
          // Redirects are always not counted as from user gesture.
          false, true, frame_tree_node_->IsMainFrame(),
          common_params_.transition, &should_override_url_loading)) {
    // A Java exception was thrown by the embedding application; we
    // need to return from this task. Specifically, it's not safe from
    // this point on to make any JNI calls.
    return;
  }

  // The content/ embedder might cause |this| to be deleted while
  // |ShouldOverrideUrlLoading| is called.
  // See https://crbug.com/770157.
  if (!this_ptr)
    return;

  if (should_override_url_loading) {
    net_error_ = net::ERR_ABORTED;
    common_params_.url = redirect_info.new_url;
    common_params_.method = redirect_info.new_method;
    // Update the navigation handle to point to the new url to ensure
    // AwWebContents sees the new URL and thus passes that URL to onPageFinished
    // (rather than passing the old URL).
    UpdateStateFollowingRedirect(
        GURL(redirect_info.new_referrer),
        base::Bind(&NavigationRequest::OnRedirectChecksComplete,
                   base::Unretained(this)));
    frame_tree_node_->ResetNavigationRequest(false, true);
    return;
  }
#endif
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanRedirectToURL(
          redirect_info.new_url)) {
    DVLOG(1) << "Denied redirect for "
             << redirect_info.new_url.possibly_invalid_spec();
    // TODO(arthursonzogni): Redirect to a javascript URL should display an
    // error page with the net::ERR_UNSAFE_REDIRECT error code. Instead, the
    // browser simply ignores the navigation, because some extensions use this
    // edge case to silently cancel navigations. See https://crbug.com/941653.
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_ABORTED),
        false /* skip_throttles */, base::nullopt /* error_page_content */,
        false /* collapse_frame */);
    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  // For renderer-initiated navigations we need to check if the source has
  // access to the URL. Browser-initiated navigations only rely on the
  // |CanRedirectToURL| test above.
  if (!browser_initiated_ && source_site_instance() &&
      !ChildProcessSecurityPolicyImpl::GetInstance()->CanRequestURL(
          source_site_instance()->GetProcess()->GetID(),
          redirect_info.new_url)) {
    DVLOG(1) << "Denied unauthorized redirect for "
             << redirect_info.new_url.possibly_invalid_spec();
    // TODO(arthursonzogni): This case uses ERR_ABORTED to be consistent with
    // the javascript URL redirect case above, though ideally it would use
    // net::ERR_UNSAFE_REDIRECT and an error page. See https://crbug.com/941653.
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_ABORTED),
        false /* skip_throttles */, base::nullopt /* error_page_content */,
        false /* collapse_frame */);
    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  // For now, DevTools needs the POST data sent to the renderer process even if
  // it is no longer a POST after the redirect.
  if (redirect_info.new_method != "POST")
    common_params_.post_data.reset();

  // Mark time for the Navigation Timing API.
  if (commit_params_.navigation_timing.redirect_start.is_null()) {
    commit_params_.navigation_timing.redirect_start =
        commit_params_.navigation_timing.fetch_start;
  }
  commit_params_.navigation_timing.redirect_end = base::TimeTicks::Now();
  commit_params_.navigation_timing.fetch_start = base::TimeTicks::Now();

  commit_params_.redirect_response.push_back(response_head->head);
  commit_params_.redirect_infos.push_back(redirect_info);

  // On redirects, the initial origin_to_commit is no longer correct, so it
  // must be cleared to avoid sending incorrect value to the renderer process.
  if (commit_params_.origin_to_commit)
    commit_params_.origin_to_commit.reset();

  commit_params_.redirects.push_back(common_params_.url);
  common_params_.url = redirect_info.new_url;
  common_params_.method = redirect_info.new_method;
  common_params_.referrer.url = GURL(redirect_info.new_referrer);
  common_params_.referrer =
      Referrer::SanitizeForRequest(common_params_.url, common_params_.referrer);

  // Check Content Security Policy before the NavigationThrottles run. This
  // gives CSP a chance to modify requests that NavigationThrottles would
  // otherwise block.
  net::Error net_error =
      CheckContentSecurityPolicy(true /* has_followed_redirect */,
                                 redirect_info.insecure_scheme_was_upgraded,
                                 false /* is_response_check */);
  if (net_error != net::OK) {
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net_error), false /*skip_throttles*/,
        base::nullopt /*error_page_content*/, false /*collapse_frame*/);

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  if (CheckCredentialedSubresource() ==
          CredentialedSubresourceCheckResult::BLOCK_REQUEST ||
      CheckLegacyProtocolInSubresource() ==
          LegacyProtocolInSubresourceCheckResult::BLOCK_REQUEST) {
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_ABORTED),
        false /*skip_throttles*/, base::nullopt /*error_page_content*/,
        false /*collapse_frame*/);

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  // Compute the SiteInstance to use for the redirect and pass its
  // RenderProcessHost if it has a process. Keep a reference if it has a
  // process, so that the SiteInstance and its associated process aren't deleted
  // before the navigation is ready to commit.
  scoped_refptr<SiteInstance> site_instance =
      frame_tree_node_->render_manager()->GetSiteInstanceForNavigationRequest(
          *this);
  speculative_site_instance_ =
      site_instance->HasProcess() ? site_instance : nullptr;

  // If the new site instance doesn't yet have a process, then tell the
  // SpareRenderProcessHostManager so it can decide whether to start warming up
  // the spare at this time (note that the actual behavior depends on
  // RenderProcessHostImpl::IsSpareProcessKeptAtAllTimes).
  if (!site_instance->HasProcess()) {
    RenderProcessHostImpl::NotifySpareManagerAboutRecentlyUsedBrowserContext(
        site_instance->GetBrowserContext());
  }

  // Re-evaluate the PreviewsState, but do not update the URLLoader. The
  // URLLoader PreviewsState is considered immutable after the URLLoader is
  // created.
  common_params_.previews_state =
      GetContentClient()->browser()->DetermineAllowedPreviews(
          common_params_.previews_state, navigation_handle_.get(),
          common_params_.url);

  // Check what the process of the SiteInstance is. It will be passed to the
  // NavigationHandle, and informed to expect a navigation to the redirected
  // URL.
  // Note: calling GetProcess on the SiteInstance can lead to the creation of a
  // new process if it doesn't have one. In this case, it should only be called
  // on a SiteInstance that already has a process.
  RenderProcessHost* expected_process =
      site_instance->HasProcess() ? site_instance->GetProcess() : nullptr;

  // It's safe to use base::Unretained because this NavigationRequest owns the
  // NavigationHandle where the callback will be stored.
  WillRedirectRequest(common_params_.referrer.url, expected_process,
                      base::Bind(&NavigationRequest::OnRedirectChecksComplete,
                                 base::Unretained(this)));
}

void NavigationRequest::OnResponseStarted(
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    const scoped_refptr<network::ResourceResponse>& response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    const GlobalRequestID& request_id,
    bool is_download,
    NavigationDownloadPolicy download_policy,
    base::Optional<SubresourceLoaderParams> subresource_loader_params) {
  // The |loader_|'s job is finished. It must not call the NavigationRequest
  // anymore from now.
  loader_.reset();
  if (is_download)
    RecordDownloadUseCountersPrePolicyCheck(download_policy);
  is_download_ = is_download && download_policy.IsDownloadAllowed();
  if (is_download_)
    RecordDownloadUseCountersPostPolicyCheck();
  request_id_ = request_id;

  DCHECK_EQ(state_, STARTED);
  DCHECK(response_head);
  TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationRequest", this,
                               "OnResponseStarted");
  state_ = RESPONSE_STARTED;
  response_head_ = response_head;
  response_body_ = std::move(response_body);
  ssl_info_ = response_head->head.ssl_info;
  auth_challenge_info_ = response_head->head.auth_challenge_info;

  // Check if the response should be sent to a renderer.
  response_should_be_rendered_ =
      !is_download && (!response_head->head.headers.get() ||
                       (response_head->head.headers->response_code() != 204 &&
                        response_head->head.headers->response_code() != 205));

  // Response that will not commit should be marked as aborted in the
  // NavigationHandle.
  if (!response_should_be_rendered_)
    net_error_ = net::ERR_ABORTED;

  // Update the AppCache params of the commit params.
  commit_params_.appcache_host_id =
      appcache_handle_
          ? base::make_optional(appcache_handle_->appcache_host_id())
          : base::nullopt;

  // Update fetch start timing. While NavigationRequest updates fetch start
  // timing for redirects, it's not aware of service worker interception so
  // fetch start timing could happen earlier than worker start timing. Use
  // worker ready time if it is greater than the current value to make sure
  // fetch start timing always comes after worker start timing (if a service
  // worker intercepted the navigation).
  commit_params_.navigation_timing.fetch_start =
      std::max(commit_params_.navigation_timing.fetch_start,
               response_head->head.service_worker_ready_time);

  // A navigation is user activated if it contains a user gesture or the frame
  // received a gesture and the navigation is renderer initiated. If the
  // navigation is browser initiated, it has to come from the context menu.
  // In all cases, the previous and new URLs have to match the
  // `ShouldPropagateUserActivation` requirements (same eTLD+1).
  // There are two different checks:
  // 1. if the `frame_tree_node_` has an origin and is following the rules above
  //    with the target URL, it is used and the bit is set if the navigation is
  //    renderer initiated and the `frame_tree_node_` had a gesture. This should
  //    apply to same page navigations and is preferred over using the referrer
  //    as it can be changed.
  // 2. if referrer and the target url are following the rules above, two
  //    conditions will set the bit: navigation comes from a gesture and is
  //    renderer initiated (middle click/ctrl+click) or it is coming from a
  //    context menu. This should apply to pages that open in a new tab and we
  //    have to follow the referrer. It means that the activation might not be
  //    transmitted if it should have.
  if (commit_params_.was_activated == WasActivatedOption::kUnknown) {
    commit_params_.was_activated = WasActivatedOption::kNo;

    if (navigation_handle_->IsRendererInitiated() &&
        (frame_tree_node_->has_received_user_gesture() ||
         frame_tree_node_->has_received_user_gesture_before_nav()) &&
        ShouldPropagateUserActivation(
            frame_tree_node_->current_origin(),
            url::Origin::Create(navigation_handle_->GetURL()))) {
      commit_params_.was_activated = WasActivatedOption::kYes;
      // TODO(805871): the next check is relying on
      // navigation_handle_->GetReferrer() but should ideally use a more
      // reliable source for the originating URL when the navigation is renderer
      // initiated.
    } else if (((navigation_handle_->HasUserGesture() &&
                 navigation_handle_->IsRendererInitiated()) ||
                navigation_handle_->WasStartedFromContextMenu()) &&
               ShouldPropagateUserActivation(
                   url::Origin::Create(navigation_handle_->GetReferrer().url),
                   url::Origin::Create(navigation_handle_->GetURL()))) {
      commit_params_.was_activated = WasActivatedOption::kYes;
    }
  }

  // Select an appropriate renderer to commit the navigation.
  if (response_should_be_rendered_) {
    render_frame_host_ =
        frame_tree_node_->render_manager()->GetFrameHostForNavigation(*this);
    NavigatorImpl::CheckWebUIRendererDoesNotDisplayNormalURL(
        render_frame_host_, common_params_.url);
  } else {
    render_frame_host_ = nullptr;
  }
  DCHECK(render_frame_host_ || !response_should_be_rendered_);

  if (!browser_initiated_ && render_frame_host_ &&
      render_frame_host_ != frame_tree_node_->current_frame_host()) {
    // Reset the source location information if the navigation will not commit
    // in the current renderer process. This information originated in another
    // process (the current one), it should not be transferred to the new one.
    common_params_.source_location.reset();

    // Allow the embedder to cancel the cross-process commit if needed.
    // TODO(clamy): Rename ShouldTransferNavigation once PlzNavigate ships.
    if (!frame_tree_node_->navigator()->GetDelegate()->ShouldTransferNavigation(
            frame_tree_node_->IsMainFrame())) {
      net_error_ = net::ERR_ABORTED;
      frame_tree_node_->ResetNavigationRequest(false, true);
      return;
    }
  }

  // This must be set before DetermineCommittedPreviews is called.
  proxy_server_ = response_head->head.proxy_server;

  // Update the previews state of the request.
  common_params_.previews_state =
      GetContentClient()->browser()->DetermineCommittedPreviews(
          common_params_.previews_state, navigation_handle_.get(),
          response_head->head.headers.get());

  // Store the URLLoaderClient endpoints until checks have been processed.
  url_loader_client_endpoints_ = std::move(url_loader_client_endpoints);

  subresource_loader_params_ = std::move(subresource_loader_params);

  // Since we've made the final pick for the RenderFrameHost above, the picked
  // RenderFrameHost's process should be considered "tainted" for future
  // process reuse decisions. That is, a site requiring a dedicated process
  // should not reuse this process, unless it's same-site with the URL we're
  // committing.  An exception is for URLs that do not "use up" the
  // SiteInstance, such as about:blank or chrome-native://.
  //
  // Note that although NavigationThrottles could still cancel the navigation
  // as part of WillProcessResponse below, we must update the process here,
  // since otherwise there could be a race if a NavigationThrottle defers the
  // navigation, and in the meantime another navigation reads the incorrect
  // IsUnused() value from the same process when making a process reuse
  // decision.
  if (render_frame_host_ &&
      SiteInstanceImpl::ShouldAssignSiteForURL(common_params_.url)) {
    render_frame_host_->GetProcess()->SetIsUsed();

    // For sites that require a dedicated process, set the site URL now if it
    // hasn't been set already. This will lock the process to that site, which
    // will prevent other sites from incorrectly reusing this process. See
    // https://crbug.com/738634.
    SiteInstanceImpl* instance = render_frame_host_->GetSiteInstance();
    if (!instance->HasSite() &&
        SiteInstanceImpl::DoesSiteRequireDedicatedProcess(
            instance->GetIsolationContext(), common_params_.url)) {
      instance->ConvertToDefaultOrSetSite(common_params_.url);
    }
  }

  devtools_instrumentation::OnNavigationResponseReceived(*this, *response_head);

  // The response code indicates that this is an error page, but we don't
  // know how to display the content.  We follow Firefox here and show our
  // own error page instead of intercepting the request as a stream or a
  // download.
  if (is_download &&
      (response_head->head.headers.get() &&
       (response_head->head.headers->response_code() / 100 != 2))) {
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_INVALID_RESPONSE),
        false /* skip_throttles */, base::nullopt /* error_page_content */,
        false /* collapse_frame */);

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  // The CSP 'navigate-to' directive needs to know whether the response is a
  // redirect or not in order to perform its checks. This is the reason why we
  // need to check the CSP both on request and response.
  net::Error net_error = CheckContentSecurityPolicy(
      navigation_handle_->WasServerRedirect() /* has_followed_redirect */,
      false /* url_upgraded_after_redirect */, true /* is_response_check */);
  if (net_error != net::OK) {
    OnRequestFailedInternal(network::URLLoaderCompletionStatus(net_error),
                            false /* skip_throttles */,
                            base::nullopt /* error_page_content */,
                            false /* collapse_frame */);

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  // Check if the navigation should be allowed to proceed.
  WillProcessResponse(
      base::Bind(&NavigationRequest::OnWillProcessResponseChecksComplete,
                 base::Unretained(this)));
}

void NavigationRequest::OnRequestFailed(
    const network::URLLoaderCompletionStatus& status) {
  DCHECK_NE(status.error_code, net::OK);

  bool collapse_frame =
      status.extended_error_code ==
      static_cast<int>(blink::ResourceRequestBlockedReason::kCollapsedByClient);
  OnRequestFailedInternal(status, false /* skip_throttles */,
                          base::nullopt /* error_page_content */,
                          collapse_frame);
}

void NavigationRequest::OnRequestFailedInternal(
    const network::URLLoaderCompletionStatus& status,
    bool skip_throttles,
    const base::Optional<std::string>& error_page_content,
    bool collapse_frame) {
  DCHECK(state_ == STARTED || state_ == RESPONSE_STARTED);
  DCHECK(!(status.error_code == net::ERR_ABORTED &&
           error_page_content.has_value()));

  // The request failed, the |loader_| must not call the NavigationRequest
  // anymore from now while the error page is being loaded.
  loader_.reset();

  common_params_.previews_state = content::PREVIEWS_OFF;
  if (status.ssl_info.has_value())
    ssl_info_ = status.ssl_info;

  devtools_instrumentation::OnNavigationRequestFailed(*this, status);

  // TODO(https://crbug.com/757633): Check that ssl_info.has_value() if
  // net_error is a certificate error.
  TRACE_EVENT_ASYNC_STEP_INTO1("navigation", "NavigationRequest", this,
                               "OnRequestFailed", "error", status.error_code);
  state_ = FAILED;

  int expected_pending_entry_id =
      navigation_handle_.get() ? navigation_handle_->pending_nav_entry_id()
                               : nav_entry_id_;
  frame_tree_node_->navigator()->DiscardPendingEntryIfNeeded(
      expected_pending_entry_id);

  net_error_ = static_cast<net::Error>(status.error_code);

  // If the request was canceled by the user do not show an error page.
  if (status.error_code == net::ERR_ABORTED) {
    frame_tree_node_->ResetNavigationRequest(false, true);
    return;
  }

  if (collapse_frame) {
    DCHECK(!frame_tree_node_->IsMainFrame());
    DCHECK_EQ(net::ERR_BLOCKED_BY_CLIENT, status.error_code);
    frame_tree_node_->SetCollapsed(true);
  }

  RenderFrameHostImpl* render_frame_host = nullptr;
  if (SiteIsolationPolicy::IsErrorPageIsolationEnabled(
          frame_tree_node_->IsMainFrame())) {
    // Main frame error pages must be isolated from the source or destination
    // process.
    //
    // Note: Since this navigation resulted in an error, clear the expected
    // process for the original navigation since for main frames the error page
    // will go into a new process.
    // TODO(nasko): Investigate whether GetFrameHostForNavigation can properly
    // account for clearing the expected process if it clears the speculative
    // RenderFrameHost. See https://crbug.com/793127.
    ResetExpectedProcess();
    render_frame_host =
        frame_tree_node_->render_manager()->GetFrameHostForNavigation(*this);
  } else {
    if (ShouldKeepErrorPageInCurrentProcess(status.error_code)) {
      render_frame_host = frame_tree_node_->current_frame_host();
    } else {
      render_frame_host =
          frame_tree_node_->render_manager()->GetFrameHostForNavigation(*this);
    }
  }

  // Sanity check that we haven't changed the RenderFrameHost picked for the
  // error page in OnRequestFailedInternal when running the WillFailRequest
  // checks.
  CHECK(!render_frame_host_ || render_frame_host_ == render_frame_host);
  render_frame_host_ = render_frame_host;

  // The check for WebUI should be performed only if error page isolation is
  // enabled for this failed navigation. It is possible for subframe error page
  // to be committed in a WebUI process as shown in https://crbug.com/944086.
  if (SiteIsolationPolicy::IsErrorPageIsolationEnabled(
          frame_tree_node_->IsMainFrame())) {
    NavigatorImpl::CheckWebUIRendererDoesNotDisplayNormalURL(
        render_frame_host_, common_params_.url);
  }

  has_stale_copy_in_cache_ = status.exists_in_cache;

  if (skip_throttles) {
    // The NavigationHandle shouldn't be notified about renderer-debug URLs.
    // They will be handled by the renderer process.
    CommitErrorPage(error_page_content);
  } else {
    // Check if the navigation should be allowed to proceed.
    WillFailRequest(base::BindOnce(&NavigationRequest::OnFailureChecksComplete,
                                   base::Unretained(this)));
  }
}

bool NavigationRequest::ShouldKeepErrorPageInCurrentProcess(int net_error) {
  // Decide whether to leave the error page in the original process.
  // * If this was a renderer-initiated navigation, and the request is blocked
  //   because the initiating document wasn't allowed to make the request,
  //   commit the error in the existing process. This is a strategy to to
  //   avoid creating a process for the destination, which may belong to an
  //   origin with a higher privilege level.
  // * Error pages resulting from errors like network outage, no network, or
  //   DNS error can reasonably expect that a reload at a later point in time
  //   would work. These should be allowed to transfer away from the current
  //   process: they do belong to whichever process that will host the
  //   destination URL, as a reload will end up committing in that process
  //   anyway.
  // * Error pages that arise during browser-initiated navigations to blocked
  //   URLs should be allowed to transfer away from the current process, which
  //   didn't request the navigation and may have a higher privilege level
  //   than the blocked destination.
  return net_error == net::ERR_BLOCKED_BY_CLIENT && !browser_initiated();
}

void NavigationRequest::OnRequestStarted(base::TimeTicks timestamp) {
  frame_tree_node_->navigator()->LogResourceRequestTime(timestamp,
                                                        common_params_.url);
}

void NavigationRequest::OnStartChecksComplete(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK(result.action() != NavigationThrottle::DEFER);
  DCHECK(result.action() != NavigationThrottle::BLOCK_RESPONSE);

  if (on_start_checks_complete_closure_)
    on_start_checks_complete_closure_.Run();
  // Abort the request if needed. This will destroy the NavigationRequest.
  if (result.action() == NavigationThrottle::CANCEL_AND_IGNORE ||
      result.action() == NavigationThrottle::CANCEL ||
      result.action() == NavigationThrottle::BLOCK_REQUEST ||
      result.action() == NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE) {
#if DCHECK_IS_ON()
    if (result.action() == NavigationThrottle::BLOCK_REQUEST) {
      DCHECK(result.net_error_code() == net::ERR_BLOCKED_BY_CLIENT ||
             result.net_error_code() == net::ERR_BLOCKED_BY_ADMINISTRATOR);
    }
    // TODO(clamy): distinguish between CANCEL and CANCEL_AND_IGNORE.
    else if (result.action() == NavigationThrottle::CANCEL_AND_IGNORE) {
      DCHECK_EQ(result.net_error_code(), net::ERR_ABORTED);
    }
#endif

    bool collapse_frame =
        result.action() == NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE;

    // If the start checks completed synchronously, which could happen if there
    // is no onbeforeunload handler or if a NavigationThrottle cancelled it,
    // then this could cause reentrancy into NavigationController. So use a
    // PostTask to avoid that.
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            &NavigationRequest::OnRequestFailedInternal,
            weak_factory_.GetWeakPtr(),
            network::URLLoaderCompletionStatus(result.net_error_code()),
            true /* skip_throttles */, result.error_page_content(),
            collapse_frame));

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  // Use the SiteInstance of the navigating RenderFrameHost to get access to
  // the StoragePartition. Using the url of the navigation will result in a
  // wrong StoragePartition being picked when a WebView is navigating.
  DCHECK_NE(AssociatedSiteInstanceType::NONE, associated_site_instance_type_);
  RenderFrameHostImpl* navigating_frame_host =
      associated_site_instance_type_ == AssociatedSiteInstanceType::SPECULATIVE
          ? frame_tree_node_->render_manager()->speculative_frame_host()
          : frame_tree_node_->current_frame_host();
  DCHECK(navigating_frame_host);

  SetExpectedProcess(navigating_frame_host->GetProcess());

  BrowserContext* browser_context =
      frame_tree_node_->navigator()->GetController()->GetBrowserContext();
  StoragePartition* partition = BrowserContext::GetStoragePartition(
      browser_context, navigating_frame_host->GetSiteInstance());
  DCHECK(partition);

  // |loader_| should not exist if the service worker handle and app cache
  // handles will be destroyed, since it holds raw pointers to them. See the
  // comment in the header for |loader_|.
  DCHECK(!loader_);

  // Only initialize the ServiceWorkerNavigationHandle if it can be created for
  // this frame.
  bool can_create_service_worker =
      (frame_tree_node_->pending_frame_policy().sandbox_flags &
       blink::WebSandboxFlags::kOrigin) != blink::WebSandboxFlags::kOrigin;
  if (can_create_service_worker) {
    ServiceWorkerContextWrapper* service_worker_context =
        static_cast<ServiceWorkerContextWrapper*>(
            partition->GetServiceWorkerContext());
    navigation_handle_->InitServiceWorkerHandle(service_worker_context);
  }

  if (IsSchemeSupportedForAppCache(common_params_.url)) {
    if (navigating_frame_host->GetRenderViewHost()
            ->GetWebkitPreferences()
            .application_cache_enabled) {
      // The final process id won't be available until
      // NavigationRequest::ReadyToCommitNavigation.
      appcache_handle_.reset(new AppCacheNavigationHandle(
          static_cast<ChromeAppCacheService*>(partition->GetAppCacheService()),
          ChildProcessHost::kInvalidUniqueID));
    }
  }

  // Mark the fetch_start (Navigation Timing API).
  commit_params_.navigation_timing.fetch_start = base::TimeTicks::Now();

  GURL site_for_cookies =
      frame_tree_node_->current_frame_host()
          ->ComputeSiteForCookiesForNavigation(common_params_.url);
  bool parent_is_main_frame = !frame_tree_node_->parent()
                                  ? false
                                  : frame_tree_node_->parent()->IsMainFrame();

  std::unique_ptr<NavigationUIData> navigation_ui_data;
  if (navigation_handle_->GetNavigationUIData())
    navigation_ui_data = navigation_handle_->GetNavigationUIData()->Clone();

  bool is_for_guests_only =
      navigation_handle_->GetStartingSiteInstance()->GetSiteURL().
          SchemeIs(kGuestScheme);

  // Give DevTools a chance to override begin params (headers, skip SW)
  // before actually loading resource.
  bool report_raw_headers = false;
  devtools_instrumentation::ApplyNetworkRequestOverrides(
      frame_tree_node_, begin_params_.get(), &report_raw_headers);
  devtools_instrumentation::OnNavigationRequestWillBeSent(*this);

  // If this is a top-frame navigation, then use the origin of the url (and
  // update it as redirects happen). If this is a sub-frame navigation, get the
  // URL from the top frame.
  // TODO(crbug.com/979296): Consider changing this code to copy an origin
  // instead of creating one from a URL which lacks opacity information.
  url::Origin frame_origin = url::Origin::Create(common_params_.url);
  url::Origin top_frame_origin =
      frame_tree_node_->IsMainFrame()
          ? frame_origin
          : frame_tree_node_->frame_tree()->root()->current_origin();

  // Merge headers with embedder's headers.
  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(begin_params_->headers);
  headers.MergeFrom(navigation_handle_->TakeModifiedRequestHeaders());
  begin_params_->headers = headers.ToString();

  std::vector<std::unique_ptr<NavigationLoaderInterceptor>> interceptor;
  if (bundled_exchanges_factory_)
    interceptor.push_back(bundled_exchanges_factory_->CreateInterceptor());
  loader_ = NavigationURLLoader::Create(
      browser_context, browser_context->GetResourceContext(), partition,
      std::make_unique<NavigationRequestInfo>(
          common_params_, begin_params_.Clone(), site_for_cookies,
          net::NetworkIsolationKey(top_frame_origin, frame_origin),
          frame_tree_node_->IsMainFrame(), parent_is_main_frame,
          IsSecureFrame(frame_tree_node_->parent()),
          frame_tree_node_->frame_tree_node_id(), is_for_guests_only,
          report_raw_headers,
          navigating_frame_host->GetVisibilityState() ==
              PageVisibilityState::kPrerender,
          upgrade_if_insecure_,
          blob_url_loader_factory_ ? blob_url_loader_factory_->Clone()
                                   : nullptr,
          devtools_navigation_token(),
          frame_tree_node_->devtools_frame_token()),
      std::move(navigation_ui_data),
      navigation_handle_->service_worker_handle(), appcache_handle_.get(),
      std::move(prefetched_signed_exchange_cache_), this,
      std::move(interceptor));
  DCHECK(!render_frame_host_);
}

void NavigationRequest::OnRedirectChecksComplete(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK(result.action() != NavigationThrottle::DEFER);
  DCHECK(result.action() != NavigationThrottle::BLOCK_RESPONSE);

  bool collapse_frame =
      result.action() == NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE;

  // Abort the request if needed. This will destroy the NavigationRequest.
  if (result.action() == NavigationThrottle::CANCEL_AND_IGNORE ||
      result.action() == NavigationThrottle::CANCEL) {
    // TODO(clamy): distinguish between CANCEL and CANCEL_AND_IGNORE if needed.
    DCHECK(result.action() == NavigationThrottle::CANCEL ||
           result.net_error_code() == net::ERR_ABORTED);
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(result.net_error_code()),
        true /* skip_throttles */, result.error_page_content(), collapse_frame);

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  if (result.action() == NavigationThrottle::BLOCK_REQUEST ||
      result.action() == NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE) {
    DCHECK(result.net_error_code() == net::ERR_BLOCKED_BY_CLIENT ||
           result.net_error_code() == net::ERR_BLOCKED_BY_ADMINISTRATOR);
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(result.net_error_code()),
        true /* skip_throttles */, result.error_page_content(), collapse_frame);
    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  devtools_instrumentation::OnNavigationRequestWillBeSent(*this);

  net::HttpRequestHeaders modified_headers =
      navigation_handle_->TakeModifiedRequestHeaders();
  std::vector<std::string> removed_headers =
      navigation_handle_->TakeRemovedRequestHeaders();

  BrowserContext* browser_context =
      frame_tree_node_->navigator()->GetController()->GetBrowserContext();
  ClientHintsControllerDelegate* client_hints_delegate =
      browser_context->GetClientHintsControllerDelegate();
  if (client_hints_delegate) {
    net::HttpRequestHeaders client_hints_extra_headers;
    AddNavigationRequestClientHintsHeaders(
        common_params_.url, &client_hints_extra_headers, browser_context,
        client_hints_delegate);
    modified_headers.MergeFrom(client_hints_extra_headers);
  }

  loader_->FollowRedirect(std::move(removed_headers),
                          std::move(modified_headers),
                          common_params_.previews_state);
}

void NavigationRequest::OnFailureChecksComplete(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK(result.action() != NavigationThrottle::DEFER);

  net::Error old_net_error = net_error_;
  net_error_ = result.net_error_code();

  // TODO(crbug.com/774663): We may want to take result.action() into account.
  if (net::ERR_ABORTED == result.net_error_code()) {
    frame_tree_node_->ResetNavigationRequest(false, true);
    return;
  }

  // Ensure that WillFailRequest() isn't changing the error code in a way that
  // switches the destination process for the error page - see
  // https://crbug.com/817881.  This is not a concern with error page
  // isolation, where all errors will go into one process.
  if (!SiteIsolationPolicy::IsErrorPageIsolationEnabled(
          frame_tree_node_->IsMainFrame())) {
    CHECK_EQ(ShouldKeepErrorPageInCurrentProcess(old_net_error),
             ShouldKeepErrorPageInCurrentProcess(net_error_))
        << " Unsupported error code change in WillFailRequest(): from "
        << net_error_ << " to " << result.net_error_code();
  }

  CommitErrorPage(result.error_page_content());
  // DO NOT ADD CODE after this. The previous call to CommitErrorPage caused
  // the destruction of the NavigationRequest.
}

void NavigationRequest::OnWillProcessResponseChecksComplete(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK(result.action() != NavigationThrottle::DEFER);

  // If the NavigationThrottles allowed the navigation to continue, have the
  // processing of the response resume in the network stack.
  if (result.action() == NavigationThrottle::PROCEED) {
    // ResourceDispatcherHost is not used when the network service is on or when
    // a service worker serves the response.
    bool served_via_resource_dispatcher_host =
        !base::FeatureList::IsEnabled(network::features::kNetworkService) &&
        !response_head_->head.was_fetched_via_service_worker;

    // If this is a download without ResourceDispatcherHost, intercept the
    // navigation response and pass it to DownloadManager, and cancel the
    // navigation.
    if (is_download_ && !served_via_resource_dispatcher_host) {
      // TODO(arthursonzogni): Pass the real ResourceRequest. For the moment
      // only these 4 parameters will be used, but it may evolve quickly.
      auto resource_request = std::make_unique<network::ResourceRequest>();
      resource_request->url = common_params_.url;
      resource_request->method = common_params_.method;
      resource_request->request_initiator = common_params_.initiator_origin;
      resource_request->referrer = common_params_.referrer.url;
      resource_request->has_user_gesture = common_params_.has_user_gesture;
      resource_request->mode = network::mojom::RequestMode::kNavigate;

      BrowserContext* browser_context =
          frame_tree_node_->navigator()->GetController()->GetBrowserContext();
      DownloadManagerImpl* download_manager = static_cast<DownloadManagerImpl*>(
          BrowserContext::GetDownloadManager(browser_context));
      download_manager->InterceptNavigation(
          std::move(resource_request), navigation_handle_->GetRedirectChain(),
          response_head_, std::move(response_body_),
          std::move(url_loader_client_endpoints_),
          ssl_info_.has_value() ? ssl_info_->cert_status : 0,
          frame_tree_node_->frame_tree_node_id());

      OnRequestFailedInternal(
          network::URLLoaderCompletionStatus(net::ERR_ABORTED),
          false /*skip_throttles*/, base::nullopt /*error_page_content*/,
          false /*collapse_frame*/);
      // DO NOT ADD CODE after this. The previous call to OnRequestFailed has
      // destroyed the NavigationRequest.
      return;
    }

    // Call ProceedWithResponse()
    // Note: There is no need to call ProceedWithResponse() when the Network
    // Service is enabled. See https://crbug.com/791049.
    if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
      // |url_loader_client_endpoints_| is always valid, except in some tests
      // where the TestNavigationLoader is used.
      if (url_loader_client_endpoints_) {
        network::mojom::URLLoaderPtr url_loader(
            std::move(url_loader_client_endpoints_->url_loader));
        url_loader->ProceedWithResponse();
        url_loader_client_endpoints_->url_loader = url_loader.PassInterface();
      } else {
        loader_->ProceedWithResponse();
      }
    }
  }

  // Abort the request if needed. This includes requests that were blocked by
  // NavigationThrottles and requests that should not commit (e.g. downloads,
  // 204/205s). This will destroy the NavigationRequest.
  if (result.action() == NavigationThrottle::CANCEL_AND_IGNORE ||
      result.action() == NavigationThrottle::CANCEL ||
      !response_should_be_rendered_) {
    // Reset the RenderFrameHost that had been computed for the commit of the
    // navigation.
    render_frame_host_ = nullptr;

    // TODO(clamy): distinguish between CANCEL and CANCEL_AND_IGNORE.
    if (!response_should_be_rendered_) {
      // DO NOT ADD CODE after this. The previous call to OnRequestFailed has
      // destroyed the NavigationRequest.
      OnRequestFailedInternal(
          network::URLLoaderCompletionStatus(net::ERR_ABORTED),
          true /* skip_throttles */, base::nullopt /* error_page_content */,
          false /* collapse_frame */);
      return;
    }

    DCHECK(result.action() == NavigationThrottle::CANCEL ||
           result.net_error_code() == net::ERR_ABORTED);
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(result.net_error_code()),
        true /* skip_throttles */, result.error_page_content(),
        false /* collapse_frame */);

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  if (result.action() == NavigationThrottle::BLOCK_RESPONSE) {
    DCHECK_EQ(net::ERR_BLOCKED_BY_RESPONSE, result.net_error_code());
    // Reset the RenderFrameHost that had been computed for the commit of the
    // navigation.
    render_frame_host_ = nullptr;
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(result.net_error_code()),
        true /* skip_throttles */, result.error_page_content(),
        false /* collapse_frame */);
    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  CommitNavigation();

  // DO NOT ADD CODE after this. The previous call to CommitNavigation caused
  // the destruction of the NavigationRequest.
}

void NavigationRequest::CommitErrorPage(
    const base::Optional<std::string>& error_page_content) {
  UpdateCommitNavigationParamsHistory();
  frame_tree_node_->TransferNavigationRequestOwnership(render_frame_host_);
  // Error pages commit in an opaque origin in the renderer process. If this
  // NavigationRequest resulted in committing an error page, just clear the
  // |origin_to_commit| and let the renderer process calculate the origin.
  // TODO(nasko): Create an opque origin here and pass it for the renderer
  // to commit into it. Potentially also make it an opaque origin derived from
  // the error page URL, so it can be checked at DidCommit processing.
  commit_params_.origin_to_commit.reset();
  if (IsPerNavigationMojoInterfaceEnabled() && request_navigation_client_ &&
      request_navigation_client_.is_bound()) {
    if (associated_site_instance_id_ ==
        render_frame_host_->GetSiteInstance()->GetId()) {
      // Reuse the request NavigationClient for commit.
      commit_navigation_client_ = std::move(request_navigation_client_);
    } else {
      IgnoreInterfaceDisconnection();
      // This navigation is cross-site: the original document should no longer
      // be able to cancel it.
    }
    associated_site_instance_id_.reset();
  }

  ReadyToCommitNavigation(true);
  render_frame_host_->FailedNavigation(this, common_params_, commit_params_,
                                       has_stale_copy_in_cache_, net_error_,
                                       error_page_content);
}

void NavigationRequest::CommitNavigation() {
  UpdateCommitNavigationParamsHistory();
  DCHECK(NeedsUrlLoader() == !!response_head_ ||
         (navigation_handle_->WasServerRedirect() &&
          common_params_.url.IsAboutBlank()));
  DCHECK(!common_params_.url.SchemeIs(url::kJavaScriptScheme));
  DCHECK(!IsRendererDebugURL(common_params_.url));
  DCHECK(render_frame_host_ ==
             frame_tree_node_->render_manager()->current_frame_host() ||
         render_frame_host_ ==
             frame_tree_node_->render_manager()->speculative_frame_host());


  frame_tree_node_->TransferNavigationRequestOwnership(render_frame_host_);
  if (IsPerNavigationMojoInterfaceEnabled() && request_navigation_client_ &&
      request_navigation_client_.is_bound()) {
    if (associated_site_instance_id_ ==
        render_frame_host_->GetSiteInstance()->GetId()) {
      // Reuse the request NavigationClient for commit.
      commit_navigation_client_ = std::move(request_navigation_client_);
    } else {
      // This navigation is cross-site: the original document should no longer
      // be able to cancel it.
      IgnoreInterfaceDisconnection();
    }
    associated_site_instance_id_.reset();
  }

  blink::mojom::ServiceWorkerProviderInfoForClientPtr
      service_worker_provider_info;
  if (navigation_handle_->service_worker_handle()) {
    // Notify the service worker navigation handle that navigation commit is
    // about to go.
    navigation_handle_->service_worker_handle()->OnBeginNavigationCommit(
        render_frame_host_->GetProcess()->GetID(),
        render_frame_host_->GetRoutingID(), &service_worker_provider_info);
  }
  if (subresource_loader_params_ &&
      !subresource_loader_params_->prefetched_signed_exchanges.empty()) {
    commit_params_.prefetched_signed_exchanges =
        std::move(subresource_loader_params_->prefetched_signed_exchanges);
  }
  render_frame_host_->CommitNavigation(
      this, common_params_, commit_params_, response_head_.get(),
      std::move(response_body_), std::move(url_loader_client_endpoints_),
      is_view_source_, std::move(subresource_loader_params_),
      std::move(subresource_overrides_),
      std::move(service_worker_provider_info), devtools_navigation_token_,
      std::move(bundled_exchanges_factory_));

  // Give SpareRenderProcessHostManager a heads-up about the most recently used
  // BrowserContext.  This is mostly needed to make sure the spare is warmed-up
  // if it wasn't done in RenderProcessHostImpl::GetProcessHostForSiteInstance.
  RenderProcessHostImpl::NotifySpareManagerAboutRecentlyUsedBrowserContext(
      render_frame_host_->GetSiteInstance()->GetBrowserContext());
}

void NavigationRequest::ResetExpectedProcess() {
  if (expected_render_process_host_id_ == ChildProcessHost::kInvalidUniqueID) {
    // No expected process is set, nothing to update.
    return;
  }
  RenderProcessHost* process =
      RenderProcessHost::FromID(expected_render_process_host_id_);
  if (process) {
    RenderProcessHostImpl::RemoveExpectedNavigationToSite(
        frame_tree_node()->navigator()->GetController()->GetBrowserContext(),
        process, site_url_);
    process->RemoveObserver(this);
  }
  expected_render_process_host_id_ = ChildProcessHost::kInvalidUniqueID;
}

void NavigationRequest::SetExpectedProcess(
    RenderProcessHost* expected_process) {
  if (expected_process &&
      expected_process->GetID() == expected_render_process_host_id_) {
    // This |expected_process| has already been informed of the navigation,
    // no need to update it again.
    return;
  }

  ResetExpectedProcess();

  if (expected_process == nullptr)
    return;

  // Keep track of the speculative RenderProcessHost and tell it to expect a
  // navigation to |site_url_|.
  expected_render_process_host_id_ = expected_process->GetID();
  expected_process->AddObserver(this);
  RenderProcessHostImpl::AddExpectedNavigationToSite(
      frame_tree_node()->navigator()->GetController()->GetBrowserContext(),
      expected_process, site_url_);
}

void NavigationRequest::RenderProcessHostDestroyed(RenderProcessHost* host) {
  DCHECK_EQ(host->GetID(), expected_render_process_host_id_);
  ResetExpectedProcess();
}

void NavigationRequest::UpdateSiteURL(
    RenderProcessHost* post_redirect_process) {
  GURL new_site_url = GetSiteForCommonParamsURL();
  int post_redirect_process_id = post_redirect_process
                                     ? post_redirect_process->GetID()
                                     : ChildProcessHost::kInvalidUniqueID;
  if (new_site_url == site_url_ &&
      post_redirect_process_id == expected_render_process_host_id_) {
    return;
  }

  // Stop expecting a navigation to the current site URL in the current expected
  // process.
  ResetExpectedProcess();

  // Update the site URL and the expected process.
  site_url_ = new_site_url;
  SetExpectedProcess(post_redirect_process);
}

bool NavigationRequest::IsAllowedByCSPDirective(
    CSPContext* context,
    CSPDirective::Name directive,
    bool has_followed_redirect,
    bool url_upgraded_after_redirect,
    bool is_response_check,
    CSPContext::CheckCSPDisposition disposition) {
  GURL url;
  // If this request was upgraded in the net stack, downgrade the URL back to
  // HTTP before checking report only policies.
  if (url_upgraded_after_redirect &&
      disposition == CSPContext::CheckCSPDisposition::CHECK_REPORT_ONLY_CSP &&
      common_params_.url.SchemeIs(url::kHttpsScheme)) {
    GURL::Replacements replacements;
    replacements.SetSchemeStr(url::kHttpScheme);
    url = common_params_.url.ReplaceComponents(replacements);
  } else {
    url = common_params_.url;
  }
  return context->IsAllowedByCsp(
      directive, url, has_followed_redirect, is_response_check,
      common_params_.source_location.value_or(SourceLocation()), disposition,
      begin_params_->is_form_submission);
}

net::Error NavigationRequest::CheckCSPDirectives(
    RenderFrameHostImpl* parent,
    bool has_followed_redirect,
    bool url_upgraded_after_redirect,
    bool is_response_check,
    CSPContext::CheckCSPDisposition disposition) {
  bool navigate_to_allowed = IsAllowedByCSPDirective(
      initiator_csp_context_.get(), CSPDirective::NavigateTo,
      has_followed_redirect, url_upgraded_after_redirect, is_response_check,
      disposition);

  bool frame_src_allowed = true;
  if (parent) {
    frame_src_allowed = IsAllowedByCSPDirective(
        parent, CSPDirective::FrameSrc, has_followed_redirect,
        url_upgraded_after_redirect, is_response_check, disposition);
  }

  if (navigate_to_allowed && frame_src_allowed)
    return net::OK;

  // If 'frame-src' fails, ERR_BLOCKED_BY_CLIENT is used instead.
  // If both checks fail, ERR_BLOCKED_BY_CLIENT is used to keep the existing
  // behaviour before 'navigate-to' was introduced.
  if (!frame_src_allowed)
    return net::ERR_BLOCKED_BY_CLIENT;

  // net::ERR_ABORTED is used to ensure that the navigation is cancelled
  // when the 'navigate-to' directive check is failed. This is a better user
  // experience as the user is not presented with an error page.
  return net::ERR_ABORTED;
}

net::Error NavigationRequest::CheckContentSecurityPolicy(
    bool has_followed_redirect,
    bool url_upgraded_after_redirect,
    bool is_response_check) {
  if (common_params_.url.SchemeIs(url::kAboutScheme))
    return net::OK;

  if (common_params_.initiator_csp_info.should_check_main_world_csp ==
      CSPDisposition::DO_NOT_CHECK) {
    return net::OK;
  }

  FrameTreeNode* parent_ftn = frame_tree_node()->parent();
  RenderFrameHostImpl* parent =
      parent_ftn ? parent_ftn->current_frame_host() : nullptr;
  if (!parent &&
      frame_tree_node()
          ->current_frame_host()
          ->GetRenderViewHost()
          ->GetDelegate()
          ->IsPortal() &&
      frame_tree_node()->render_manager()->GetOuterDelegateNode()) {
    parent = frame_tree_node()
                 ->render_manager()
                 ->GetOuterDelegateNode()
                 ->current_frame_host()
                 ->GetParent();
  }

  // TODO(andypaicu,https://crbug.com/837627): the current_frame_host is the
  // wrong RenderFrameHost. We should be using the navigation initiator
  // RenderFrameHost.
  initiator_csp_context_->SetReportingRenderFrameHost(
      frame_tree_node()->current_frame_host());

  // CSP checking happens in three phases, per steps 3-5 of
  // https://fetch.spec.whatwg.org/#main-fetch:
  //
  // (1) Check report-only policies and trigger reports for any violations.
  // (2) Upgrade the request to HTTPS if necessary.
  // (3) Check enforced policies (triggering reports for any violations of those
  //     policies) and block the request if necessary.
  //
  // This sequence of events allows site owners to learn about (via step 1) any
  // requests that are upgraded in step 2.

  net::Error report_only_csp_status = CheckCSPDirectives(
      parent, has_followed_redirect, url_upgraded_after_redirect,
      is_response_check, CSPContext::CHECK_REPORT_ONLY_CSP);

  // upgrade-insecure-requests is handled in the network code for redirects,
  // only do the upgrade here if this is not a redirect.
  if (!has_followed_redirect && !frame_tree_node()->IsMainFrame()) {
    if (parent &&
        parent->ShouldModifyRequestUrlForCsp(true /* is subresource */)) {
      upgrade_if_insecure_ = true;
      parent->ModifyRequestUrlForCsp(&common_params_.url);
      commit_params_.original_url = common_params_.url;
    }
  }

  net::Error enforced_csp_status = CheckCSPDirectives(
      parent, has_followed_redirect, url_upgraded_after_redirect,
      is_response_check, CSPContext::CHECK_ENFORCED_CSP);
  if (enforced_csp_status != net::OK)
    return enforced_csp_status;
  return report_only_csp_status;
}

NavigationRequest::CredentialedSubresourceCheckResult
NavigationRequest::CheckCredentialedSubresource() const {
  // It only applies to subframes.
  if (frame_tree_node_->IsMainFrame())
    return CredentialedSubresourceCheckResult::ALLOW_REQUEST;

  // URLs with no embedded credentials should load correctly.
  if (!common_params_.url.has_username() && !common_params_.url.has_password())
    return CredentialedSubresourceCheckResult::ALLOW_REQUEST;

  // Relative URLs on top-level pages that were loaded with embedded credentials
  // should load correctly.
  FrameTreeNode* parent_ftn = frame_tree_node_->parent();
  DCHECK(parent_ftn);
  const GURL& parent_url = parent_ftn->current_url();
  if (url::Origin::Create(parent_url)
          .IsSameOriginWith(url::Origin::Create(common_params_.url)) &&
      parent_url.username() == common_params_.url.username() &&
      parent_url.password() == common_params_.url.password()) {
    return CredentialedSubresourceCheckResult::ALLOW_REQUEST;
  }

  // Warn the user about the request being blocked.
  RenderFrameHostImpl* parent = parent_ftn->current_frame_host();
  DCHECK(parent);
  const char* console_message =
      "Subresource requests whose URLs contain embedded credentials (e.g. "
      "`https://user:pass@host/`) are blocked. See "
      "https://www.chromestatus.com/feature/5669008342777856 for more "
      "details.";
  parent->AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kWarning,
                              console_message);
  if (!base::FeatureList::IsEnabled(features::kBlockCredentialedSubresources))
    return CredentialedSubresourceCheckResult::ALLOW_REQUEST;
  return CredentialedSubresourceCheckResult::BLOCK_REQUEST;
}

NavigationRequest::LegacyProtocolInSubresourceCheckResult
NavigationRequest::CheckLegacyProtocolInSubresource() const {
  // It only applies to subframes.
  if (frame_tree_node_->IsMainFrame())
    return LegacyProtocolInSubresourceCheckResult::ALLOW_REQUEST;

  if (!ShouldTreatURLSchemeAsLegacy(common_params_.url))
    return LegacyProtocolInSubresourceCheckResult::ALLOW_REQUEST;

  FrameTreeNode* parent_ftn = frame_tree_node_->parent();
  DCHECK(parent_ftn);
  const GURL& parent_url = parent_ftn->current_url();
  if (ShouldTreatURLSchemeAsLegacy(parent_url))
    return LegacyProtocolInSubresourceCheckResult::ALLOW_REQUEST;

  // Warn the user about the request being blocked.
  RenderFrameHostImpl* parent = parent_ftn->current_frame_host();
  DCHECK(parent);
  const char* console_message =
      "Subresource requests using legacy protocols (like `ftp:`) are blocked. "
      "Please deliver web-accessible resources over modern protocols like "
      "HTTPS. See https://www.chromestatus.com/feature/5709390967472128 for "
      "details.";
  parent->AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kWarning,
                              console_message);

  return LegacyProtocolInSubresourceCheckResult::BLOCK_REQUEST;
}

NavigationRequest::AboutSrcDocCheckResult NavigationRequest::CheckAboutSrcDoc()
    const {
  if (!common_params_.url.IsAboutSrcdoc())
    return AboutSrcDocCheckResult::ALLOW_REQUEST;

  // Loading about:srcdoc in the main frame can't have any reasonable meaning.
  // There might be a malicious website trying to exploit a bug from this. As a
  // defensive measure, do not proceed. They would have failed anyway later.
  if (frame_tree_node_->IsMainFrame())
    return AboutSrcDocCheckResult::BLOCK_REQUEST;

  // Navigations to about:srcdoc?foo or about:srcdoc#foo are never caused by
  // using the iframe srcdoc attribute. Only about:srcdoc is expected.
  if (common_params_.url != GURL(url::kAboutSrcdocURL))
    return AboutSrcDocCheckResult::BLOCK_REQUEST;

  // TODO(arthursonzogni): Disallow navigations to about:srcdoc initiated from a
  // different frame or from a different window.

  // TODO(arthursonzogni): Disallow browser initiated navigations to
  // about:srcdoc, except session history navigations.

  return AboutSrcDocCheckResult::ALLOW_REQUEST;
}

void NavigationRequest::UpdateCommitNavigationParamsHistory() {
  NavigationController* navigation_controller =
      frame_tree_node_->navigator()->GetController();
  commit_params_.current_history_list_offset =
      navigation_controller->GetCurrentEntryIndex();
  commit_params_.current_history_list_length =
      navigation_controller->GetEntryCount();
}

void NavigationRequest::OnRendererAbortedNavigation() {
  if (IsWaitingToCommit()) {
    render_frame_host_->NavigationRequestCancelled(this);
  } else {
    frame_tree_node_->navigator()->CancelNavigation(frame_tree_node_, false);
  }

  // Do not add code after this, NavigationRequest has been destroyed.
}

void NavigationRequest::HandleInterfaceDisconnection(
    mojom::NavigationClientAssociatedPtr* navigation_client,
    base::OnceClosure error_handler) {
  navigation_client->set_connection_error_handler(std::move(error_handler));
}

void NavigationRequest::IgnoreInterfaceDisconnection() {
  return request_navigation_client_.set_connection_error_handler(
      base::DoNothing());
}

void NavigationRequest::IgnoreCommitInterfaceDisconnection() {
  return commit_navigation_client_.set_connection_error_handler(
      base::DoNothing());
}

bool NavigationRequest::IsSameDocument() const {
  return FrameMsg_Navigate_Type::IsSameDocument(common_params_.navigation_type);
}

int NavigationRequest::EstimateHistoryOffset() {
  if (common_params_.should_replace_current_entry)
    return 0;

  NavigationController* controller =
      frame_tree_node_->navigator()->GetController();
  if (!controller)  // Interstitial page.
    return 1;

  int current_index = controller->GetLastCommittedEntryIndex();
  int pending_index = controller->GetPendingEntryIndex();

  // +1 for non history navigation.
  if (current_index == -1 || pending_index == -1)
    return 1;

  return pending_index - current_index;
}

void NavigationRequest::RecordDownloadUseCountersPrePolicyCheck(
    NavigationDownloadPolicy download_policy) {
  content::RenderFrameHost* rfh = frame_tree_node_->current_frame_host();
  GetContentClient()->browser()->LogWebFeatureForCurrentPage(
      rfh, blink::mojom::WebFeature::kDownloadPrePolicyCheck);

  // Log UseCounters for opener navigations.
  if (download_policy.IsType(NavigationDownloadType::kOpenerCrossOrigin)) {
    rfh->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        base::StringPrintf(
            "Navigating a cross-origin opener to a download (%s) is "
            "deprecated, see "
            "https://www.chromestatus.com/feature/5742188281462784.",
            navigation_handle_->GetURL().spec().c_str()));
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kOpenerNavigationDownloadCrossOrigin);
  }

  // Log UseCounters for download in sandbox.
  if (download_policy.IsType(NavigationDownloadType::kSandbox)) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kDownloadInSandbox);
  }

  // Log UseCounters for download without user activation.
  if (download_policy.IsType(NavigationDownloadType::kNoGesture)) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kDownloadWithoutUserGesture);
  }

  // Log UseCounters for download in sandbox without user activation.
  if (download_policy.IsType(NavigationDownloadType::kSandboxNoGesture)) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kDownloadInSandboxWithoutUserGesture);
  }

  // Log UseCounters for download in ad frame without user activation.
  if (download_policy.IsType(NavigationDownloadType::kAdFrameNoGesture)) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kDownloadInAdFrameWithoutUserGesture);
  }

  // Log UseCounters for download in ad frame.
  if (download_policy.IsType(NavigationDownloadType::kAdFrame)) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kDownloadInAdFrame);
  }
}

void NavigationRequest::RecordDownloadUseCountersPostPolicyCheck() {
  DCHECK(is_download_);
  content::RenderFrameHost* rfh = frame_tree_node_->current_frame_host();
  GetContentClient()->browser()->LogWebFeatureForCurrentPage(
      rfh, blink::mojom::WebFeature::kDownloadPostPolicyCheck);
}

void NavigationRequest::OnNavigationEventProcessed(
    NavigationThrottleRunner::Event event,
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK_NE(NavigationThrottle::DEFER, result.action());
  switch (event) {
    case NavigationThrottleRunner::Event::WillStartRequest:
      OnWillStartRequestProcessed(result);
      return;
    case NavigationThrottleRunner::Event::WillRedirectRequest:
      OnWillRedirectRequestProcessed(result);
      return;
    case NavigationThrottleRunner::Event::WillFailRequest:
      OnWillFailRequestProcessed(result);
      return;
    case NavigationThrottleRunner::Event::WillProcessResponse:
      OnWillProcessResponseProcessed(result);
      return;
    default:
      NOTREACHED();
  }
  NOTREACHED();
}

void NavigationRequest::OnWillStartRequestProcessed(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK_EQ(PROCESSING_WILL_START_REQUEST, handle_state_);
  DCHECK_NE(NavigationThrottle::BLOCK_RESPONSE, result.action());
  if (result.action() == NavigationThrottle::PROCEED)
    handle_state_ = WILL_START_REQUEST;
  else
    handle_state_ = CANCELING;

  // TODO(zetamoo): Remove CompleteCallback, and call NavigationRequest methods
  // directly.
  RunCompleteCallback(result);
}

void NavigationRequest::OnWillRedirectRequestProcessed(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK_EQ(PROCESSING_WILL_REDIRECT_REQUEST, handle_state_);
  DCHECK_NE(NavigationThrottle::BLOCK_RESPONSE, result.action());
  if (result.action() == NavigationThrottle::PROCEED) {
    handle_state_ = WILL_REDIRECT_REQUEST;

    // Notify the delegate that a redirect was encountered and will be followed.
    if (GetDelegate())
      GetDelegate()->DidRedirectNavigation(navigation_handle_.get());
  } else {
    handle_state_ = NavigationRequest::CANCELING;
  }
  RunCompleteCallback(result);
}

void NavigationRequest::OnWillFailRequestProcessed(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK_EQ(NavigationRequest::PROCESSING_WILL_FAIL_REQUEST, handle_state_);
  DCHECK_NE(NavigationThrottle::BLOCK_RESPONSE, result.action());
  if (result.action() == NavigationThrottle::PROCEED) {
    handle_state_ = WILL_FAIL_REQUEST;
    result = NavigationThrottle::ThrottleCheckResult(
        NavigationThrottle::PROCEED, navigation_handle_->GetNetErrorCode());
  } else {
    handle_state_ = CANCELING;
  }
  RunCompleteCallback(result);
}

void NavigationRequest::OnWillProcessResponseProcessed(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK_EQ(NavigationRequest::PROCESSING_WILL_PROCESS_RESPONSE, handle_state_);
  DCHECK_NE(NavigationThrottle::BLOCK_REQUEST, result.action());
  DCHECK_NE(NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE, result.action());
  if (result.action() == NavigationThrottle::PROCEED) {
    handle_state_ = WILL_PROCESS_RESPONSE;
    // If the navigation is done processing the response, then it's ready to
    // commit. Inform observers that the navigation is now ready to commit,
    // unless it is not set to commit (204/205s/downloads).
    if (render_frame_host_)
      ReadyToCommitNavigation(false);
  } else {
    handle_state_ = NavigationRequest::CANCELING;
  }
  RunCompleteCallback(result);
}

NavigatorDelegate* NavigationRequest::GetDelegate() const {
  return frame_tree_node()->navigator()->GetDelegate();
}

void NavigationRequest::Resume(NavigationThrottle* resuming_throttle) {
  DCHECK(resuming_throttle);
  TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationRequest", this,
                               "Resume");
  throttle_runner_->ResumeProcessingNavigationEvent(resuming_throttle);
  // DO NOT ADD CODE AFTER THIS, as the NavigationHandle might have been deleted
  // by the previous call.
}

void NavigationRequest::CancelDeferredNavigation(
    NavigationThrottle* cancelling_throttle,
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK(cancelling_throttle);
  DCHECK_EQ(cancelling_throttle, throttle_runner_->GetDeferringThrottle());
  CancelDeferredNavigationInternal(result);
}

void NavigationRequest::CallResumeForTesting() {
  throttle_runner_->CallResumeForTesting();
}

void NavigationRequest::RegisterThrottleForTesting(
    std::unique_ptr<NavigationThrottle> navigation_throttle) {
  throttle_runner_->AddThrottle(std::move(navigation_throttle));
}
bool NavigationRequest::IsDeferredForTesting() {
  return throttle_runner_->GetDeferringThrottle() != nullptr;
}

bool NavigationRequest::IsForMhtmlSubframe() const {
  return frame_tree_node_->parent() &&
         frame_tree_node_->frame_tree()
             ->root()
             ->current_frame_host()
             ->is_mhtml_document() &&
         // Unlike every other MHTML subframe URLs, data-url are loaded via the
         // URL, not from the MHTML archive. See https://crbug.com/969696.
         !common_params_.url.SchemeIs(url::kDataScheme);
}

void NavigationRequest::CancelDeferredNavigationInternal(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK(handle_state_ == PROCESSING_WILL_START_REQUEST ||
         handle_state_ == PROCESSING_WILL_REDIRECT_REQUEST ||
         handle_state_ == PROCESSING_WILL_FAIL_REQUEST ||
         handle_state_ == PROCESSING_WILL_PROCESS_RESPONSE);
  DCHECK(result.action() == NavigationThrottle::CANCEL_AND_IGNORE ||
         result.action() == NavigationThrottle::CANCEL ||
         result.action() == NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE);
  DCHECK(result.action() != NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE ||
         handle_state_ == PROCESSING_WILL_START_REQUEST ||
         handle_state_ == PROCESSING_WILL_REDIRECT_REQUEST);

  TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationRequest", this,
                               "CancelDeferredNavigation");
  handle_state_ = NavigationRequest::CANCELING;
  RunCompleteCallback(result);
}

void NavigationRequest::WillStartRequest(
    ThrottleChecksFinishedCallback callback) {
  TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationRequest", this,
                               "WillStartRequest");
  // WillStartRequest should only be called once.
  if (handle_state_ != INITIAL) {
    handle_state_ = CANCELING;
    RunCompleteCallback(NavigationThrottle::CANCEL);
    return;
  }

  handle_state_ = PROCESSING_WILL_START_REQUEST;
  complete_callback_ = std::move(callback);

  if (IsSelfReferentialURL()) {
    handle_state_ = CANCELING;
    RunCompleteCallback(NavigationThrottle::CANCEL);
    return;
  }

  throttle_runner_->RegisterNavigationThrottles();

  // If the content/ embedder did not pass the NavigationUIData at the beginning
  // of the navigation, ask for it now.
  if (!navigation_ui_data_) {
    navigation_ui_data_ =
        GetDelegate()->GetNavigationUIData(navigation_handle_.get());
  }

  // Notify each throttle of the request.
  throttle_runner_->ProcessNavigationEvent(
      NavigationThrottleRunner::Event::WillStartRequest);
  // DO NOT ADD CODE AFTER THIS, as the NavigationHandle might have been deleted
  // by the previous call.
}

void NavigationRequest::WillRedirectRequest(
    const GURL& new_referrer_url,
    RenderProcessHost* post_redirect_process,
    ThrottleChecksFinishedCallback callback) {
  TRACE_EVENT_ASYNC_STEP_INTO1("navigation", "NavigationRequest", this,
                               "WillRedirectRequest", "url",
                               common_params_.url.possibly_invalid_spec());
  UpdateStateFollowingRedirect(new_referrer_url, std::move(callback));
  UpdateSiteURL(post_redirect_process);

  if (IsSelfReferentialURL()) {
    handle_state_ = CANCELING;
    RunCompleteCallback(NavigationThrottle::CANCEL);
    return;
  }

  // Notify each throttle of the request.
  throttle_runner_->ProcessNavigationEvent(
      NavigationThrottleRunner::Event::WillRedirectRequest);
  // DO NOT ADD CODE AFTER THIS, as the NavigationHandle might have been deleted
  // by the previous call.
}

void NavigationRequest::WillFailRequest(
    ThrottleChecksFinishedCallback callback) {
  TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationRequest", this,
                               "WillFailRequest");

  complete_callback_ = std::move(callback);
  handle_state_ = PROCESSING_WILL_FAIL_REQUEST;

  // Notify each throttle of the request.
  throttle_runner_->ProcessNavigationEvent(
      NavigationThrottleRunner::Event::WillFailRequest);
  // DO NOT ADD CODE AFTER THIS, as the NavigationHandle might have been deleted
  // by the previous call.
}

void NavigationRequest::WillProcessResponse(
    ThrottleChecksFinishedCallback callback) {
  TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationRequest", this,
                               "WillProcessResponse");

  handle_state_ = PROCESSING_WILL_PROCESS_RESPONSE;
  complete_callback_ = std::move(callback);

  // Notify each throttle of the response.
  throttle_runner_->ProcessNavigationEvent(
      NavigationThrottleRunner::Event::WillProcessResponse);
  // DO NOT ADD CODE AFTER THIS, as the NavigationHandle might have been deleted
  // by the previous call.
}

bool NavigationRequest::IsSelfReferentialURL() {
  // about: URLs should be exempted since they are reserved for other purposes
  // and cannot be the source of infinite recursion.
  // See https://crbug.com/341858 .
  if (common_params_.url.SchemeIs(url::kAboutScheme))
    return false;

  // Browser-triggered navigations should be exempted.
  if (browser_initiated())
    return false;

  // Some sites rely on constructing frame hierarchies where frames are loaded
  // via POSTs with the same URLs, so exempt POST requests.
  // See https://crbug.com/710008.
  if (common_params_.method == "POST")
    return false;

  // We allow one level of self-reference because some sites depend on that,
  // but we don't allow more than one.
  bool found_self_reference = false;
  for (const FrameTreeNode* node = frame_tree_node()->parent(); node;
       node = node->parent()) {
    if (node->current_url().EqualsIgnoringRef(common_params_.url)) {
      if (found_self_reference)
        return true;
      found_self_reference = true;
    }
  }
  return false;
}

void NavigationRequest::DidCommitNavigation(
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    bool navigation_entry_committed,
    bool did_replace_entry,
    const GURL& previous_url,
    NavigationType navigation_type) {
  common_params_.url = params.url;
  did_replace_entry_ = did_replace_entry;
  should_update_history_ = params.should_update_history;
  previous_url_ = previous_url;
  navigation_type_ = navigation_type;

  // If an error page reloads, net_error_code might be 200 but we still want to
  // count it as an error page.
  if (params.base_url.spec() == kUnreachableWebDataURL ||
      navigation_handle_->GetNetErrorCode() != net::OK) {
    TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationHandle",
                                 navigation_handle_.get(),
                                 "DidCommitNavigation: error page");
    handle_state_ = DID_COMMIT_ERROR_PAGE;
  } else {
    TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationHandle",
                                 navigation_handle_.get(),
                                 "DidCommitNavigation");
    handle_state_ = DID_COMMIT;
  }

  navigation_handle_->StopCommitTimeout();

  // Record metrics for the time it took to commit the navigation if it was to
  // another document without error.
  if (!IsSameDocument() && handle_state_ != DID_COMMIT_ERROR_PAGE) {
    ui::PageTransition transition = common_params_.transition;
    base::Optional<bool> is_background =
        render_frame_host_->GetProcess()->IsProcessBackgrounded();

    RecordStartToCommitMetrics(
        common_params_.navigation_start, transition, ready_to_commit_time_,
        is_background, is_same_process_, frame_tree_node_->IsMainFrame());
  }

  DCHECK(!frame_tree_node_->IsMainFrame() || navigation_entry_committed)
      << "Only subframe navigations can get here without changing the "
      << "NavigationEntry";
  subframe_entry_committed_ = navigation_entry_committed;

  // For successful navigations, ensure the frame owner element is no longer
  // collapsed as a result of a prior navigation.
  if (handle_state_ != DID_COMMIT_ERROR_PAGE &&
      !frame_tree_node()->IsMainFrame()) {
    // The last committed load in collapsed frames will be an error page with
    // |kUnreachableWebDataURL|. Same-document navigation should not be
    // possible.
    DCHECK(!IsSameDocument() || !frame_tree_node()->is_collapsed());
    frame_tree_node()->SetCollapsed(false);
  }
}

GURL NavigationRequest::GetSiteForCommonParamsURL() const {
  // TODO(alexmos): Using |starting_site_instance_|'s IsolationContext may not
  // be correct for cross-BrowsingInstance redirects.
  return SiteInstanceImpl::GetSiteForURL(
      starting_site_instance_->GetIsolationContext(), common_params_.url);
}

// TODO(zetamoo): Try to merge this function inside its callers.
void NavigationRequest::UpdateStateFollowingRedirect(
    const GURL& new_referrer_url,
    ThrottleChecksFinishedCallback callback) {
  // The navigation should not redirect to a "renderer debug" url. It should be
  // blocked in NavigationRequest::OnRequestRedirected or in
  // ResourceLoader::OnReceivedRedirect.
  // Note: the |common_params_.url| below is the post-redirect URL.
  // See https://crbug.com/728398.
  CHECK(!IsRendererDebugURL(common_params_.url));

  // Update the navigation parameters.
  if (!(common_params_.transition & ui::PAGE_TRANSITION_CLIENT_REDIRECT)) {
    sanitized_referrer_.url = new_referrer_url;
    sanitized_referrer_ =
        Referrer::SanitizeForRequest(common_params_.url, sanitized_referrer_);
  }

  was_redirected_ = true;
  redirect_chain_.push_back(common_params_.url);

  handle_state_ = PROCESSING_WILL_REDIRECT_REQUEST;

#if defined(OS_ANDROID)
  navigation_handle_proxy_->DidRedirect();
#endif

  complete_callback_ = std::move(callback);
}

void NavigationRequest::SetNavigationClient(
    mojom::NavigationClientAssociatedPtrInfo navigation_client,
    int32_t associated_site_instance_id) {
  DCHECK(from_begin_navigation_ ||
         common_params_.is_history_navigation_in_new_child_frame);
  DCHECK(!request_navigation_client_);
  if (!navigation_client.is_valid())
    return;

  request_navigation_client_ = mojom::NavigationClientAssociatedPtr();
  request_navigation_client_.Bind(std::move(navigation_client));

  // Binds the OnAbort callback
  HandleInterfaceDisconnection(
      &request_navigation_client_,
      base::BindOnce(&NavigationRequest::OnRendererAbortedNavigation,
                     base::Unretained(this)));
  associated_site_instance_id_ = associated_site_instance_id;
}

bool NavigationRequest::NeedsUrlLoader() const {
  return IsURLHandledByNetworkStack(common_params_.url) && !IsSameDocument() &&
         !IsForMhtmlSubframe();
}

void NavigationRequest::ReadyToCommitNavigation(bool is_error) {
  TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationHandle", this,
                               "ReadyToCommitNavigation");

  handle_state_ = READY_TO_COMMIT;
  ready_to_commit_time_ = base::TimeTicks::Now();
  navigation_handle_->RestartCommitTimeout();

  if (appcache_handle_)
    appcache_handle_->SetProcessId(render_frame_host_->GetProcess()->GetID());

  // Record metrics for the time it takes to get to this state from the
  // beginning of the navigation.
  if (!IsSameDocument() && !is_error) {
    is_same_process_ =
        render_frame_host_->GetProcess()->GetID() ==
        frame_tree_node_->current_frame_host()->GetProcess()->GetID();

    RecordIsSameProcessMetrics(common_params_.transition, is_same_process_);

    RecordReadyToCommitMetrics(common_params_.navigation_start,
                               common_params_.transition, ready_to_commit_time_,
                               is_same_process_,
                               frame_tree_node_->IsMainFrame());
  }

  SetExpectedProcess(render_frame_host_->GetProcess());

  if (!IsSameDocument())
    GetDelegate()->ReadyToCommitNavigation(navigation_handle_.get());
}

std::unique_ptr<AppCacheNavigationHandle>
NavigationRequest::TakeAppCacheHandle() {
  return std::move(appcache_handle_);
}

bool NavigationRequest::IsWaitingToCommit() {
  return handle_state_ == NavigationRequest::READY_TO_COMMIT;
}

void NavigationRequest::RunCompleteCallback(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK(result.action() != NavigationThrottle::DEFER);

  ThrottleChecksFinishedCallback callback = std::move(complete_callback_);

  if (!complete_callback_for_testing_.is_null())
    std::move(complete_callback_for_testing_).Run(result);

  if (!callback.is_null())
    std::move(callback).Run(result);

  // No code after running the callback, as it might have resulted in our
  // destruction.
}

}  // namespace content
