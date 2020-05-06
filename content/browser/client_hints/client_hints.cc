// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/client_hints/client_hints.h"

#include <algorithm>
#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/optional.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "net/base/url_util.h"
#include "net/http/structured_headers.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/platform/web_client_hints_type.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace content {

namespace {
uint8_t randomization_salt = 0;

constexpr size_t kMaxRandomNumbers = 21;

// Returns the randomization salt (weak and insecure) that should be used when
// adding noise to the network quality metrics. This is known only to the
// device, and is generated only once. This makes it possible to add the same
// amount of noise for a given origin.
uint8_t RandomizationSalt() {
  if (randomization_salt == 0)
    randomization_salt = base::RandInt(1, kMaxRandomNumbers);
  DCHECK_LE(1, randomization_salt);
  DCHECK_GE(kMaxRandomNumbers, randomization_salt);
  return randomization_salt;
}

double GetRandomMultiplier(const std::string& host) {
  // The random number should be a function of the hostname to reduce
  // cross-origin fingerprinting. The random number should also be a function
  // of randomized salt which is known only to the device. This prevents
  // origin from removing noise from the estimates.
  unsigned hash = std::hash<std::string>{}(host) + RandomizationSalt();
  double random_multiplier =
      0.9 + static_cast<double>((hash % kMaxRandomNumbers)) * 0.01;
  DCHECK_LE(0.90, random_multiplier);
  DCHECK_GE(1.10, random_multiplier);
  return random_multiplier;
}

unsigned long RoundRtt(const std::string& host,
                       const base::Optional<base::TimeDelta>& rtt) {
  // Limit the size of the buckets and the maximum reported value to reduce
  // fingerprinting.
  static const size_t kGranularityMsec = 50;
  static const double kMaxRttMsec = 3.0 * 1000;

  if (!rtt.has_value()) {
    // RTT is unavailable. So, return the fastest value.
    return 0;
  }

  double rtt_msec = static_cast<double>(rtt.value().InMilliseconds());
  rtt_msec *= GetRandomMultiplier(host);
  rtt_msec = std::min(rtt_msec, kMaxRttMsec);

  DCHECK_LE(0, rtt_msec);
  DCHECK_GE(kMaxRttMsec, rtt_msec);

  // Round down to the nearest kBucketSize msec value.
  return std::round(rtt_msec / kGranularityMsec) * kGranularityMsec;
}

double RoundKbpsToMbps(const std::string& host,
                       const base::Optional<int32_t>& downlink_kbps) {
  // Limit the size of the buckets and the maximum reported value to reduce
  // fingerprinting.
  static const size_t kGranularityKbps = 50;
  static const double kMaxDownlinkKbps = 10.0 * 1000;

  // If downlink is unavailable, return the fastest value.
  double randomized_downlink_kbps = downlink_kbps.value_or(kMaxDownlinkKbps);
  randomized_downlink_kbps *= GetRandomMultiplier(host);

  randomized_downlink_kbps =
      std::min(randomized_downlink_kbps, kMaxDownlinkKbps);

  DCHECK_LE(0, randomized_downlink_kbps);
  DCHECK_GE(kMaxDownlinkKbps, randomized_downlink_kbps);
  // Round down to the nearest kGranularityKbps kbps value.
  double downlink_kbps_rounded =
      std::round(randomized_downlink_kbps / kGranularityKbps) *
      kGranularityKbps;

  // Convert from Kbps to Mbps.
  return downlink_kbps_rounded / 1000;
}

double GetDeviceScaleFactor() {
  double device_scale_factor = 1.0;
  if (display::Screen::GetScreen()) {
    device_scale_factor =
        display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor();
  }
  DCHECK_LT(0.0, device_scale_factor);
  return device_scale_factor;
}

// Returns the zoom factor for a given |url|.
double GetZoomFactor(BrowserContext* context, const GURL& url) {
// Android does not have the concept of zooming in like desktop.
#if defined(OS_ANDROID)
  return 1.0;
#else

  double zoom_level = HostZoomMap::GetDefaultForBrowserContext(context)
                          ->GetZoomLevelForHostAndScheme(
                              url.scheme(), net::GetHostOrSpecFromURL(url));

  if (zoom_level == 0.0) {
    // Get default zoom level.
    zoom_level = HostZoomMap::GetDefaultForBrowserContext(context)
                     ->GetDefaultZoomLevel();
  }

  return blink::PageZoomLevelToZoomFactor(zoom_level);
#endif
}

// Returns a string corresponding to |value|. The returned string satisfies
// ABNF: 1*DIGIT [ "." 1*DIGIT ]
std::string DoubleToSpecCompliantString(double value) {
  DCHECK_LE(0.0, value);
  std::string result = base::NumberToString(value);
  DCHECK(!result.empty());
  if (value >= 1.0)
    return result;

  DCHECK_LE(0.0, value);
  DCHECK_GT(1.0, value);

  // Check if there is at least one character before period.
  if (result.at(0) != '.')
    return result;

  // '.' is the first character in |result|. Prefix one digit before the
  // period to make it spec compliant.
  return "0" + result;
}

// Return the effective connection type value overridden for web APIs.
// If no override value has been set, a null value is returned.
base::Optional<net::EffectiveConnectionType>
GetWebHoldbackEffectiveConnectionType() {
  if (!base::FeatureList::IsEnabled(
          features::kNetworkQualityEstimatorWebHoldback)) {
    return base::nullopt;
  }
  std::string effective_connection_type_param =
      base::GetFieldTrialParamValueByFeature(
          features::kNetworkQualityEstimatorWebHoldback,
          "web_effective_connection_type_override");

  base::Optional<net::EffectiveConnectionType> effective_connection_type =
      net::GetEffectiveConnectionTypeForName(effective_connection_type_param);
  DCHECK(effective_connection_type_param.empty() || effective_connection_type);

  if (!effective_connection_type)
    return base::nullopt;
  DCHECK_NE(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            effective_connection_type.value());
  return effective_connection_type;
}

void SetHeaderToDouble(net::HttpRequestHeaders* headers,
                       network::mojom::WebClientHintsType client_hint_type,
                       double value) {
  headers->SetHeader(
      blink::kClientHintsHeaderMapping[static_cast<int>(client_hint_type)],
      DoubleToSpecCompliantString(value));
}

void SetHeaderToInt(net::HttpRequestHeaders* headers,
                    network::mojom::WebClientHintsType client_hint_type,
                    double value) {
  headers->SetHeader(
      blink::kClientHintsHeaderMapping[static_cast<int>(client_hint_type)],
      base::NumberToString(std::round(value)));
}

void SetHeaderToString(net::HttpRequestHeaders* headers,
                       network::mojom::WebClientHintsType client_hint_type,
                       const std::string& value) {
  headers->SetHeader(
      blink::kClientHintsHeaderMapping[static_cast<int>(client_hint_type)],
      value);
}

void RemoveClientHintHeader(network::mojom::WebClientHintsType client_hint_type,
                            net::HttpRequestHeaders* headers) {
  headers->RemoveHeader(
      blink::kClientHintsHeaderMapping[static_cast<int>(client_hint_type)]);
}

void AddDeviceMemoryHeader(net::HttpRequestHeaders* headers) {
  DCHECK(headers);
  blink::ApproximatedDeviceMemory::Initialize();
  const float device_memory =
      blink::ApproximatedDeviceMemory::GetApproximatedDeviceMemory();
  DCHECK_LT(0.0, device_memory);
  SetHeaderToDouble(headers, network::mojom::WebClientHintsType::kDeviceMemory,
                    device_memory);
}

void AddDPRHeader(net::HttpRequestHeaders* headers,
                  BrowserContext* context,
                  const GURL& url) {
  DCHECK(headers);
  DCHECK(context);
  double device_scale_factor = GetDeviceScaleFactor();
  double zoom_factor = GetZoomFactor(context, url);
  SetHeaderToDouble(headers, network::mojom::WebClientHintsType::kDpr,
                    device_scale_factor * zoom_factor);
}

void AddViewportWidthHeader(net::HttpRequestHeaders* headers,
                            BrowserContext* context,
                            const GURL& url) {
  DCHECK(headers);
  DCHECK(context);
  // The default value on Android. See
  // https://cs.chromium.org/chromium/src/third_party/WebKit/Source/core/css/viewportAndroid.css.
  double viewport_width = 980;

#if !defined(OS_ANDROID)
  double device_scale_factor = GetDeviceScaleFactor();
  viewport_width = (display::Screen::GetScreen()
                        ->GetPrimaryDisplay()
                        .GetSizeInPixel()
                        .width()) /
                   GetZoomFactor(context, url) / device_scale_factor;
#endif  // !OS_ANDROID
  DCHECK_LT(0, viewport_width);
  // TODO(yoav): Find out why this 0 check is needed...
  if (viewport_width > 0) {
    SetHeaderToInt(headers, network::mojom::WebClientHintsType::kViewportWidth,
                   viewport_width);
  }
}

void AddRttHeader(net::HttpRequestHeaders* headers,
                  network::NetworkQualityTracker* network_quality_tracker,
                  const GURL& url) {
  DCHECK(headers);

  base::Optional<net::EffectiveConnectionType> web_holdback_ect =
      GetWebHoldbackEffectiveConnectionType();

  base::TimeDelta http_rtt;
  if (web_holdback_ect.has_value()) {
    http_rtt = net::NetworkQualityEstimatorParams::GetDefaultTypicalHttpRtt(
        web_holdback_ect.value());
  } else if (network_quality_tracker) {
    http_rtt = network_quality_tracker->GetHttpRTT();
  } else {
    http_rtt = net::NetworkQualityEstimatorParams::GetDefaultTypicalHttpRtt(
        net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
  }
  SetHeaderToInt(headers, network::mojom::WebClientHintsType::kRtt,
                 RoundRtt(url.host(), http_rtt));
}

void AddDownlinkHeader(net::HttpRequestHeaders* headers,
                       network::NetworkQualityTracker* network_quality_tracker,
                       const GURL& url) {
  DCHECK(headers);
  base::Optional<net::EffectiveConnectionType> web_holdback_ect =
      GetWebHoldbackEffectiveConnectionType();

  int32_t downlink_throughput_kbps;

  if (web_holdback_ect.has_value()) {
    downlink_throughput_kbps =
        net::NetworkQualityEstimatorParams::GetDefaultTypicalDownlinkKbps(
            web_holdback_ect.value());
  } else if (network_quality_tracker) {
    downlink_throughput_kbps =
        network_quality_tracker->GetDownstreamThroughputKbps();
  } else {
    downlink_throughput_kbps =
        net::NetworkQualityEstimatorParams::GetDefaultTypicalDownlinkKbps(
            net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
  }

  SetHeaderToDouble(headers, network::mojom::WebClientHintsType::kDownlink,
                    RoundKbpsToMbps(url.host(), downlink_throughput_kbps));
}

void AddEctHeader(net::HttpRequestHeaders* headers,
                  network::NetworkQualityTracker* network_quality_tracker,
                  const GURL& url) {
  DCHECK(headers);
  DCHECK_EQ(blink::kWebEffectiveConnectionTypeMappingCount,
            net::EFFECTIVE_CONNECTION_TYPE_4G + 1u);
  DCHECK_EQ(blink::kWebEffectiveConnectionTypeMappingCount,
            static_cast<size_t>(net::EFFECTIVE_CONNECTION_TYPE_LAST));

  base::Optional<net::EffectiveConnectionType> web_holdback_ect =
      GetWebHoldbackEffectiveConnectionType();

  int effective_connection_type;
  if (web_holdback_ect.has_value()) {
    effective_connection_type = web_holdback_ect.value();
  } else if (network_quality_tracker) {
    effective_connection_type =
        static_cast<int>(network_quality_tracker->GetEffectiveConnectionType());
  } else {
    effective_connection_type =
        static_cast<int>(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
  }

  SetHeaderToString(
      headers, network::mojom::WebClientHintsType::kEct,
      blink::kWebEffectiveConnectionTypeMapping[effective_connection_type]);
}

void AddLangHeader(net::HttpRequestHeaders* headers, BrowserContext* context) {
  SetHeaderToString(
      headers, network::mojom::WebClientHintsType::kLang,
      blink::SerializeLangClientHint(
          GetContentClient()->browser()->GetAcceptLangs(context)));
}

bool IsValidURLForClientHints(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS() ||
      (url.SchemeIs(url::kHttpScheme) && !net::IsLocalhost(url)))
    return false;

  DCHECK(url.SchemeIs(url::kHttpsScheme) ||
         (url.SchemeIs(url::kHttpScheme) && net::IsLocalhost(url)));
  return true;
}

bool UserAgentClientHintEnabled() {
  return base::FeatureList::IsEnabled(features::kUserAgentClientHint);
}

void AddUAHeader(net::HttpRequestHeaders* headers,
                 network::mojom::WebClientHintsType type,
                 const std::string& value) {
  SetHeaderToString(headers, type, value);
}

// Use structured headers to escape and quote headers
std::string SerializeHeaderString(std::string str) {
  return net::structured_headers::SerializeItem(
             net::structured_headers::Item(str))
      .value_or(std::string());
}

bool IsFeaturePolicyForClientHintsEnabled() {
  return base::FeatureList::IsEnabled(features::kFeaturePolicyForClientHints);
}

// Captures the state used in applying client hints.
struct ClientHintsExtendedData {
  ClientHintsExtendedData(const GURL& url,
                          FrameTreeNode* frame_tree_node,
                          ClientHintsControllerDelegate* delegate)
      : resource_origin(url::Origin::Create(url)) {
    // If the current frame is the main frame, the URL wasn't committed yet, so
    // in order to get the main frame URL, we should use the provided URL
    // instead. Otherwise, the current frame is an iframe and the main frame URL
    // was committed, so we can safely get it from it. Similarly, an
    // in-navigation main frame doesn't yet have a feature policy.
    RenderFrameHostImpl* main_frame =
        frame_tree_node->frame_tree()->GetMainFrame();
    is_main_frame = frame_tree_node->IsMainFrame();
    if (is_main_frame) {
      main_frame_url = url;
      is_1p_origin = true;
    } else {
      main_frame_url = main_frame->GetLastCommittedURL();
      feature_policy = main_frame->feature_policy();
      is_1p_origin = resource_origin.IsSameOriginWith(
          main_frame->GetLastCommittedOrigin());
    }

    delegate->GetAllowedClientHintsFromSource(main_frame_url, &hints);
  }

  blink::WebEnabledClientHints hints;
  url::Origin resource_origin;
  bool is_main_frame = false;
  GURL main_frame_url;
  blink::FeaturePolicy* feature_policy = nullptr;
  bool is_1p_origin = false;
};

bool ShouldAddClientHint(const ClientHintsExtendedData& data,
                         network::mojom::WebClientHintsType type,
                         blink::mojom::FeaturePolicyFeature feature) {
  if (!data.hints.IsEnabled(type))
    return false;
  if (!IsFeaturePolicyForClientHintsEnabled() || data.is_main_frame)
    return data.is_1p_origin;
  return data.feature_policy && data.feature_policy->IsFeatureEnabledForOrigin(
                                    feature, data.resource_origin);
}

bool IsJavascriptEnabled(FrameTreeNode* frame_tree_node) {
  RenderViewHost* render_view_host =
      frame_tree_node->current_frame_host()->GetRenderViewHost();
  return render_view_host->GetWebkitPreferences().javascript_enabled;
}

bool ShouldAddClientHints(const GURL& url,
                          bool javascript_enabled,
                          ClientHintsControllerDelegate* delegate) {
  // Client hints should only be enabled when JavaScript is enabled. Platforms
  // which enable/disable JavaScript on a per-origin basis should implement
  // IsJavaScriptAllowed to check a given origin. Other platforms (Android
  // WebView) enable/disable JavaScript on a per-View basis, using the
  // WebPreferences setting.
  return IsValidURLForClientHints(url) && delegate->IsJavaScriptAllowed(url) &&
         javascript_enabled;
}

// Captures when UpdateNavigationRequestClientUaHeadersImpl() is being called.
enum class ClientUaHeaderCallType {
  // The call is happening during creation of the NavigationRequest.
  kDuringCreation,

  // The call is happening after creation of the NavigationRequest.
  kAfterCreated,
};

// Implementation of UpdateNavigationRequestClientUaHeaders().
void UpdateNavigationRequestClientUaHeadersImpl(
    const GURL& url,
    ClientHintsControllerDelegate* delegate,
    bool override_ua,
    FrameTreeNode* frame_tree_node,
    ClientUaHeaderCallType call_type,
    net::HttpRequestHeaders* headers) {
  base::Optional<blink::UserAgentMetadata> ua_metadata;
  bool disable_due_to_custom_ua = false;
  if (override_ua) {
    NavigatorDelegate* nav_delegate =
        frame_tree_node->navigator()->GetDelegate();
    ua_metadata =
        nav_delegate ? nav_delegate->GetUserAgentOverride().ua_metadata_override
                     : base::nullopt;
    // If a custom UA override is set, but no value is provided for UA client
    // hints, disable them.
    disable_due_to_custom_ua = !ua_metadata.has_value();
  }

  if (devtools_instrumentation::ApplyUserAgentMetadataOverrides(frame_tree_node,
                                                                &ua_metadata)) {
    // Likewise, if devtools says to override client hints but provides no
    // value, disable them. This overwrites previous decision from UI.
    disable_due_to_custom_ua = !ua_metadata.has_value();
  }

  if (!disable_due_to_custom_ua) {
    if (!ua_metadata.has_value())
      ua_metadata = delegate->GetUserAgentMetadata();

    // The `Sec-CH-UA` client hint is attached to all outgoing requests. This is
    // (intentionally) different than other client hints.
    //
    // https://wicg.github.io/client-hints-infrastructure/#abstract-opdef-append-client-hints-to-request
    AddUAHeader(headers, network::mojom::WebClientHintsType::kUA,
                ua_metadata->SerializeBrandVersionList());
    // The `Sec-CH-UA-Mobile client hint was also deemed "low entropy" and can
    // safely be sent with every request.
    AddUAHeader(headers, network::mojom::WebClientHintsType::kUAMobile,
                ua_metadata->mobile ? "?1" : "?0");

    ClientHintsExtendedData data(url, frame_tree_node, delegate);

    if (ShouldAddClientHint(
            data, network::mojom::WebClientHintsType::kUAFullVersion,
            blink::mojom::FeaturePolicyFeature::kClientHintUAFullVersion)) {
      AddUAHeader(headers, network::mojom::WebClientHintsType::kUAFullVersion,
                  SerializeHeaderString(ua_metadata->full_version));
    }

    if (ShouldAddClientHint(
            data, network::mojom::WebClientHintsType::kUAArch,
            blink::mojom::FeaturePolicyFeature::kClientHintUAArch)) {
      AddUAHeader(headers, network::mojom::WebClientHintsType::kUAArch,
                  SerializeHeaderString(ua_metadata->architecture));
    }

    if (ShouldAddClientHint(
            data, network::mojom::WebClientHintsType::kUAPlatform,
            blink::mojom::FeaturePolicyFeature::kClientHintUAPlatform)) {
      AddUAHeader(headers, network::mojom::WebClientHintsType::kUAPlatform,
                  SerializeHeaderString(ua_metadata->platform));
    }

    if (ShouldAddClientHint(
            data, network::mojom::WebClientHintsType::kUAPlatformVersion,
            blink::mojom::FeaturePolicyFeature::kClientHintUAPlatform)) {
      AddUAHeader(headers,
                  network::mojom::WebClientHintsType::kUAPlatformVersion,
                  SerializeHeaderString(ua_metadata->platform_version));
    }

    if (ShouldAddClientHint(
            data, network::mojom::WebClientHintsType::kUAModel,
            blink::mojom::FeaturePolicyFeature::kClientHintUAModel)) {
      AddUAHeader(headers, network::mojom::WebClientHintsType::kUAModel,
                  SerializeHeaderString(ua_metadata->model));
    }
  } else if (call_type == ClientUaHeaderCallType::kAfterCreated) {
    RemoveClientHintHeader(network::mojom::WebClientHintsType::kUA, headers);
    RemoveClientHintHeader(network::mojom::WebClientHintsType::kUAMobile,
                           headers);
    RemoveClientHintHeader(network::mojom::WebClientHintsType::kUAFullVersion,
                           headers);
    RemoveClientHintHeader(network::mojom::WebClientHintsType::kUAArch,
                           headers);
    RemoveClientHintHeader(network::mojom::WebClientHintsType::kUAPlatform,
                           headers);
    RemoveClientHintHeader(
        network::mojom::WebClientHintsType::kUAPlatformVersion, headers);
    RemoveClientHintHeader(network::mojom::WebClientHintsType::kUAModel,
                           headers);
  }
}

}  // namespace

unsigned long RoundRttForTesting(const std::string& host,
                                 const base::Optional<base::TimeDelta>& rtt) {
  return RoundRtt(host, rtt);
}

double RoundKbpsToMbpsForTesting(const std::string& host,
                                 const base::Optional<int32_t>& downlink_kbps) {
  return RoundKbpsToMbps(host, downlink_kbps);
}

void UpdateNavigationRequestClientUaHeaders(
    const GURL& url,
    ClientHintsControllerDelegate* delegate,
    bool override_ua,
    FrameTreeNode* frame_tree_node,
    net::HttpRequestHeaders* headers) {
  if (!UserAgentClientHintEnabled() ||
      !ShouldAddClientHints(url, IsJavascriptEnabled(frame_tree_node),
                            delegate)) {
    return;
  }

  UpdateNavigationRequestClientUaHeadersImpl(
      url, delegate, override_ua, frame_tree_node,
      ClientUaHeaderCallType::kAfterCreated, headers);
}

void AddNavigationRequestClientHintsHeaders(
    const GURL& url,
    net::HttpRequestHeaders* headers,
    BrowserContext* context,
    ClientHintsControllerDelegate* delegate,
    bool is_ua_override_on,
    FrameTreeNode* frame_tree_node) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(blink::kWebEffectiveConnectionTypeMappingCount,
            net::EFFECTIVE_CONNECTION_TYPE_4G + 1u);
  DCHECK_EQ(blink::kWebEffectiveConnectionTypeMappingCount,
            static_cast<size_t>(net::EFFECTIVE_CONNECTION_TYPE_LAST));
  DCHECK(context);

  if (!ShouldAddClientHints(url, IsJavascriptEnabled(frame_tree_node),
                            delegate)) {
    return;
  }

  const ClientHintsExtendedData data(url, frame_tree_node, delegate);

  // Add Headers
  if (ShouldAddClientHint(
          data, network::mojom::WebClientHintsType::kDeviceMemory,
          blink::mojom::FeaturePolicyFeature::kClientHintDeviceMemory)) {
    AddDeviceMemoryHeader(headers);
  }
  if (ShouldAddClientHint(data, network::mojom::WebClientHintsType::kDpr,
                          blink::mojom::FeaturePolicyFeature::kClientHintDPR)) {
    AddDPRHeader(headers, context, url);
  }
  if (ShouldAddClientHint(
          data, network::mojom::WebClientHintsType::kViewportWidth,
          blink::mojom::FeaturePolicyFeature::kClientHintViewportWidth)) {
    AddViewportWidthHeader(headers, context, url);
  }
  network::NetworkQualityTracker* network_quality_tracker =
      delegate->GetNetworkQualityTracker();
  if (ShouldAddClientHint(data, network::mojom::WebClientHintsType::kRtt,
                          blink::mojom::FeaturePolicyFeature::kClientHintRTT)) {
    AddRttHeader(headers, network_quality_tracker, url);
  }
  if (ShouldAddClientHint(
          data, network::mojom::WebClientHintsType::kDownlink,
          blink::mojom::FeaturePolicyFeature::kClientHintDownlink)) {
    AddDownlinkHeader(headers, network_quality_tracker, url);
  }
  if (ShouldAddClientHint(data, network::mojom::WebClientHintsType::kEct,
                          blink::mojom::FeaturePolicyFeature::kClientHintECT)) {
    AddEctHeader(headers, network_quality_tracker, url);
  }
  if (ShouldAddClientHint(
          data, network::mojom::WebClientHintsType::kLang,
          blink::mojom::FeaturePolicyFeature::kClientHintLang)) {
    AddLangHeader(headers, context);
  }

  if (UserAgentClientHintEnabled()) {
    UpdateNavigationRequestClientUaHeadersImpl(
        url, delegate, is_ua_override_on, frame_tree_node,
        ClientUaHeaderCallType::kDuringCreation, headers);
  }

  // Static assert that triggers if a new client hint header is added. If a
  // new client hint header is added, the following assertion should be updated.
  // If possible, logic should be added above so that the request headers for
  // the newly added client hint can be added to the request.
  static_assert(
      network::mojom::WebClientHintsType::kUAPlatformVersion ==
          network::mojom::WebClientHintsType::kMaxValue,
      "Consider adding client hint request headers from the browser process");

  // TODO(crbug.com/735518): If the request is redirected, the client hint
  // headers stay attached to the redirected request. Consider removing/adding
  // the client hints headers if the request is redirected with a change in
  // scheme or a change in the origin.
}

void PersistAcceptCHAfterNagivationRequestRedirect(
    const GURL& url,
    const ::network::mojom::ParsedHeadersPtr& headers,
    BrowserContext* context,
    ClientHintsControllerDelegate* delegate,
    FrameTreeNode* frame_tree_node) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(context);
  DCHECK(headers);

  if (!headers->accept_ch)
    return;

  if (!IsValidURLForClientHints(url))
    return;

  // Client hints should only be enabled when JavaScript is enabled. Platforms
  // which enable/disable JavaScript on a per-origin basis should implement
  // IsJavaScriptAllowed to check a given origin. Other platforms (Android
  // WebView) enable/disable JavaScript on a per-View basis, using the
  // WebPreferences setting.
  if (!delegate->IsJavaScriptAllowed(url) ||
      !IsJavascriptEnabled(frame_tree_node)) {
    return;
  }

  // Only the main frame should parse accept-CH.
  if (!frame_tree_node->IsMainFrame())
    return;

  // TODO(morlovich): No browser-side knowledge on what permit_lang_hints should
  // be, so this is failing shut for now.
  base::Optional<std::vector<network::mojom::WebClientHintsType>> parsed =
      blink::FilterAcceptCH(headers->accept_ch.value(),
                            false /* permit_lang_hints */,
                            UserAgentClientHintEnabled());
  if (!parsed.has_value())
    return;

  // JSON cannot store "non-finite" values (i.e. NaN or infinite) so
  // base::TimeDelta::Max cannot be used. As accept-ch-lifetime will be removed
  // once the FeaturePolicyForClientHints feature is shipped, a reasonably large
  // value was chosen instead.
  delegate->PersistClientHints(url::Origin::Create(url), parsed.value(),
                               base::TimeDelta::FromDays(1000000));
}

}  // namespace content
