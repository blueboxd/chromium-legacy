// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/lookalike_url_navigation_throttle.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/i18n/char_iterator.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/lookalikes/lookalike_url_blocking_page.h"
#include "chrome/browser/lookalikes/lookalike_url_controller_client.h"
#include "chrome/browser/lookalikes/lookalike_url_service.h"
#include "chrome/browser/lookalikes/lookalike_url_tab_storage.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reputation/safety_tips_config.h"
#include "chrome/common/chrome_features.h"
#include "components/lookalikes/core/features.h"
#include "components/lookalikes/core/lookalike_url_ui_util.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/url_formatter/spoof_checks/top_domains/top500_domains.h"
#include "components/url_formatter/spoof_checks/top_domains/top_domain_util.h"
#include "content/public/browser/navigation_handle.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"

namespace {

typedef content::NavigationThrottle::ThrottleCheckResult ThrottleCheckResult;

// Returns true if |current_url| is at the end of the redirect chain
// stored in |stored_redirect_chain|.
bool IsInterstitialReload(const GURL& current_url,
                          const std::vector<GURL>& stored_redirect_chain) {
  return stored_redirect_chain.size() > 1 &&
         stored_redirect_chain[stored_redirect_chain.size() - 1] == current_url;
}

// Returns the index of the first URL in the redirect chain which has a
// different eTLD+1 than the initial URL. If all URLs have the same eTLD+1,
// returns 0.
size_t FindFirstCrossSiteURL(const std::vector<GURL>& redirect_chain) {
  DCHECK_GE(redirect_chain.size(), 2u);
  const GURL initial_url = redirect_chain[0];
  const std::string initial_etld_plus_one = GetETLDPlusOne(initial_url.host());
  for (size_t i = 1; i < redirect_chain.size(); i++) {
    if (initial_etld_plus_one != GetETLDPlusOne(redirect_chain[i].host())) {
      return i;
    }
  }
  return 0;
}

bool IsASCII(UChar32 codepoint) {
  return !(codepoint & ~0x7F);
}

// Returns true if |codepoint| has emoji related properties.
bool IsEmojiRelatedCodepoint(UChar32 codepoint) {
  return u_hasBinaryProperty(codepoint, UCHAR_EMOJI) ||
         // Characters that have emoji presentation by default (e.g. hourglass)
         u_hasBinaryProperty(codepoint, UCHAR_EMOJI_PRESENTATION) ||
         // Characters displayed as country flags when used as a valid pair.
         // E.g. Regional Indicator Symbol Letter B used once in a string
         // is rendered as 🇧, used twice is rendered as the flag of Barbados
         // (with country code BB). It's therefore possible to come up with
         // a spoof using regional indicator characters as text, but these
         // domain names will be readily punycoded and detecting pairs isn't
         // easy so we keep the code simple here.
         u_hasBinaryProperty(codepoint, UCHAR_REGIONAL_INDICATOR) ||
         // Pictographs such as Black Cross On Shield (U+26E8).
         u_hasBinaryProperty(codepoint, UCHAR_EXTENDED_PICTOGRAPHIC);
}

// Returns true if |text| contains only ASCII characters, pictographs
// or emojis. This check is only used to determine if a domain that already
// failed spoof checks should be blocked by an interstitial. Ideally, we would
// check this for non-ASCII scripts as well (e.g. Cyrillic + emoji), but such
// usage isn't common.
bool IsASCIIAndEmojiOnly(const base::StringPiece16& text) {
  base::i18n::UTF16CharIterator iter(text.data(), text.length());
  while (!iter.end()) {
    const UChar32 codepoint = iter.get();
    if (!IsASCII(codepoint) && !IsEmojiRelatedCodepoint(codepoint)) {
      return false;
    }
    iter.Advance();
  }
  return true;
}

}  // namespace

bool IsSafeRedirect(const std::string& matching_domain,
                    const std::vector<GURL>& redirect_chain) {
  if (redirect_chain.size() < 2) {
    return false;
  }
  const size_t first_cross_site_redirect =
      FindFirstCrossSiteURL(redirect_chain);
  DCHECK_GE(first_cross_site_redirect, 0u);
  DCHECK_LE(first_cross_site_redirect, redirect_chain.size() - 1);
  if (first_cross_site_redirect == 0) {
    // All URLs in the redirect chain belong to the same eTLD+1.
    return false;
  }
  // There is a redirect from the initial eTLD+1 to another site. In order to be
  // a safe redirect, it should be to the root of |matching_domain|. This
  // ignores any further redirects after |matching_domain|.
  const GURL redirect_target = redirect_chain[first_cross_site_redirect];
  return matching_domain == GetETLDPlusOne(redirect_target.host()) &&
         redirect_target == redirect_target.GetWithEmptyPath();
}

LookalikeUrlNavigationThrottle::LookalikeUrlNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle),
      profile_(Profile::FromBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext())) {}

LookalikeUrlNavigationThrottle::~LookalikeUrlNavigationThrottle() {}

ThrottleCheckResult LookalikeUrlNavigationThrottle::HandleThrottleRequest(
    const GURL& url,
    bool check_safe_redirect) {
  // Ignore if running unit tests. Some tests use
  // TestMockTimeTaskRunner::ScopedContext and call CreateTestWebContents()
  // which navigates and waits for throttles to complete using a RunLoop.
  // However, TestMockTimeTaskRunner::ScopedContext disallows RunLoop so those
  // tests crash. We should only do this with a real profile anyways.
  // use_test_profile is set by unit tests to true so that the rest of the
  // throttle is exercised.
  // In other words, this condition is false in production code, browser tests
  // and only lookalike unit tests. It's true in all non-lookalike unit tests.
  if (!use_test_profile_ && profile_->AsTestingProfile()) {
    return content::NavigationThrottle::PROCEED;
  }

  content::NavigationHandle* handle = navigation_handle();

  // Ignore subframe and same document navigations.
  if (!handle->IsInMainFrame() || handle->IsSameDocument()) {
    return content::NavigationThrottle::PROCEED;
  }

  // Get stored interstitial parameters early. By doing so, we ensure that a
  // navigation to an irrelevant (for this interstitial's purposes) URL such as
  // chrome://settings while the lookalike interstitial is being shown clears
  // the stored state:
  // 1. User navigates to lookalike.tld which redirects to site.tld.
  // 2. Interstitial shown.
  // 3. User navigates to chrome://settings.
  // If, after this, the user somehow ends up on site.tld with a reload (e.g.
  // with ReloadType::ORIGINAL_REQUEST_URL), this will correctly not show an
  // interstitial.
  LookalikeUrlTabStorage* tab_storage =
      LookalikeUrlTabStorage::GetOrCreate(handle->GetWebContents());
  const LookalikeUrlTabStorage::InterstitialParams interstitial_params =
      tab_storage->GetInterstitialParams();
  tab_storage->ClearInterstitialParams();

  if (!url.SchemeIsHTTPOrHTTPS()) {
    return content::NavigationThrottle::PROCEED;
  }

  // If the URL is in the component updater allowlist, don't show any warning.
  const auto* proto = GetSafetyTipsRemoteConfigProto();
  if (proto &&
      IsUrlAllowlistedBySafetyTipsComponent(proto, url.GetWithEmptyPath())) {
    return content::NavigationThrottle::PROCEED;
  }

  // If the URL is in the allowlist, don't show any warning.
  if (tab_storage->IsDomainAllowed(url.host())) {
    return content::NavigationThrottle::PROCEED;
  }

  // If this is a reload and if the current URL is the last URL of the stored
  // redirect chain, the interstitial was probably reloaded. Stop the reload and
  // navigate back to the original lookalike URL so that the whole throttle is
  // exercised again.
  if (handle->GetReloadType() != content::ReloadType::NONE &&
      IsInterstitialReload(url, interstitial_params.redirect_chain)) {
    CHECK(interstitial_params.url.SchemeIsHTTPOrHTTPS());
    // See
    // https://groups.google.com/a/chromium.org/forum/#!topic/chromium-dev/plIZV3Rkzok
    // for why this is OK. Assume interstitial reloads are always browser
    // initiated.
    navigation_handle()->GetWebContents()->OpenURL(content::OpenURLParams(
        interstitial_params.url, interstitial_params.referrer,
        WindowOpenDisposition::CURRENT_TAB,
        ui::PageTransition::PAGE_TRANSITION_RELOAD,
        false /* is_renderer_initiated */));
    return content::NavigationThrottle::CANCEL_AND_IGNORE;
  }

  const DomainInfo navigated_domain = GetDomainInfo(url);
  // Empty domain_and_registry happens on private domains.
  if (navigated_domain.domain_and_registry.empty() ||
      IsTopDomain(navigated_domain)) {
    return content::NavigationThrottle::PROCEED;
  }

  LookalikeUrlService* service = LookalikeUrlService::Get(profile_);
  if (!use_test_profile_ && service->EngagedSitesNeedUpdating()) {
    service->ForceUpdateEngagedSites(
        base::BindOnce(&LookalikeUrlNavigationThrottle::PerformChecksDeferred,
                       weak_factory_.GetWeakPtr(), url, navigated_domain,
                       check_safe_redirect));
    return content::NavigationThrottle::DEFER;
  }

  return PerformChecks(url, navigated_domain, check_safe_redirect,
                       service->GetLatestEngagedSites());
}

ThrottleCheckResult LookalikeUrlNavigationThrottle::WillProcessResponse() {
  if (navigation_handle()->GetNetErrorCode() != net::OK) {
    return content::NavigationThrottle::PROCEED;
  }
  // Do not check for if the redirect was safe. That should only be done when
  // the navigation is still being redirected.
  return HandleThrottleRequest(navigation_handle()->GetURL(), false);
}

ThrottleCheckResult LookalikeUrlNavigationThrottle::WillRedirectRequest() {
  const std::vector<GURL>& chain = navigation_handle()->GetRedirectChain();

  // WillRedirectRequest is called after a redirect occurs, so the end of the
  // chain is the URL that was redirected to. We need to check the preceding URL
  // that caused the redirection. The final URL in the chain is checked either:
  //  - after the next redirection (when there is a longer chain), or
  //  - by WillProcessResponse (before content is rendered).
  if (chain.size() < 2) {
    return content::NavigationThrottle::PROCEED;
  }
  return HandleThrottleRequest(chain[chain.size() - 2], true);
}

const char* LookalikeUrlNavigationThrottle::GetNameForLogging() {
  return "LookalikeUrlNavigationThrottle";
}

ThrottleCheckResult LookalikeUrlNavigationThrottle::ShowInterstitial(
    const GURL& safe_url,
    const GURL& url,
    ukm::SourceId source_id,
    LookalikeUrlMatchType match_type) {
  content::NavigationHandle* handle = navigation_handle();
  content::WebContents* web_contents = handle->GetWebContents();

  auto controller = std::make_unique<LookalikeUrlControllerClient>(
      web_contents, url, safe_url);

  std::unique_ptr<LookalikeUrlBlockingPage> blocking_page(
      new LookalikeUrlBlockingPage(web_contents, safe_url, url, source_id,
                                   match_type, std::move(controller)));

  base::Optional<std::string> error_page_contents =
      blocking_page->GetHTMLContents();

  security_interstitials::SecurityInterstitialTabHelper::AssociateBlockingPage(
      web_contents, handle->GetNavigationId(), std::move(blocking_page));

  // Store interstitial parameters in per-tab storage. Reloading the
  // interstitial once it's shown navigates to the final URL in the original
  // redirect chain. It also loses the original redirect chain. By storing these
  // parameters, we can check if the next navigation is a reload and act
  // accordingly.
  content::Referrer referrer(handle->GetReferrer().url,
                             handle->GetReferrer().policy);
  LookalikeUrlTabStorage::GetOrCreate(handle->GetWebContents())
      ->OnLookalikeInterstitialShown(url, referrer, handle->GetRedirectChain());

  return ThrottleCheckResult(content::NavigationThrottle::CANCEL,
                             net::ERR_BLOCKED_BY_CLIENT, error_page_contents);
}

std::unique_ptr<LookalikeUrlNavigationThrottle>
LookalikeUrlNavigationThrottle::MaybeCreateNavigationThrottle(
    content::NavigationHandle* navigation_handle) {
  // If the tab is being prerendered, stop here before it breaks metrics
  content::WebContents* web_contents = navigation_handle->GetWebContents();
  if (prerender::PrerenderContents::FromWebContents(web_contents)) {
    return nullptr;
  }

  // Otherwise, always insert the throttle for metrics recording.
  return std::make_unique<LookalikeUrlNavigationThrottle>(navigation_handle);
}

void LookalikeUrlNavigationThrottle::PerformChecksDeferred(
    const GURL& url,
    const DomainInfo& navigated_domain,
    bool check_safe_redirect,
    const std::vector<DomainInfo>& engaged_sites) {
  ThrottleCheckResult result =
      PerformChecks(url, navigated_domain, check_safe_redirect, engaged_sites);

  if (result.action() == content::NavigationThrottle::PROCEED) {
    Resume();
    return;
  }

  CancelDeferredNavigation(result);
}

bool ShouldBlockBySpoofCheckResult(const DomainInfo& navigated_domain) {
  // Here, only a subset of spoof checks that cause an IDN to fallback to
  // punycode are configured to show an interstitial.
  switch (navigated_domain.idn_result.spoof_check_result) {
    case url_formatter::IDNSpoofChecker::Result::kNone:
    case url_formatter::IDNSpoofChecker::Result::kSafe:
      return false;

    case url_formatter::IDNSpoofChecker::Result::kICUSpoofChecks:
      // If the eTLD+1 contains only a mix of ASCII + Emoji, allow.
      return !IsASCIIAndEmojiOnly(navigated_domain.idn_result.result);

    case url_formatter::IDNSpoofChecker::Result::kDeviationCharacters:
      // Failures because of deviation characters, especially ß, is common.
      return false;

    case url_formatter::IDNSpoofChecker::Result::kTLDSpecificCharacters:
    case url_formatter::IDNSpoofChecker::Result::kUnsafeMiddleDot:
    case url_formatter::IDNSpoofChecker::Result::kWholeScriptConfusable:
    case url_formatter::IDNSpoofChecker::Result::kDigitLookalikes:
    case url_formatter::IDNSpoofChecker::Result::
        kNonAsciiLatinCharMixedWithNonLatin:
    case url_formatter::IDNSpoofChecker::Result::kDangerousPattern:
      return true;
  }
}

ThrottleCheckResult LookalikeUrlNavigationThrottle::PerformChecks(
    const GURL& url,
    const DomainInfo& navigated_domain,
    bool check_safe_redirect,
    const std::vector<DomainInfo>& engaged_sites) {
  std::string matched_domain;
  LookalikeUrlMatchType match_type;

  // Ensure that this URL is not already engaged. We can't use the synchronous
  // SiteEngagementService::IsEngagementAtLeast as it has side effects. We check
  // in PerformChecks to ensure we have up-to-date engaged_sites.
  // This check ignores the scheme which is okay since it's more conservative:
  // If the user is engaged with http://domain.test, not showing the warning on
  // https://domain.test is acceptable.
  const auto already_engaged =
      std::find_if(engaged_sites.begin(), engaged_sites.end(),
                   [navigated_domain](const DomainInfo& engaged_domain) {
                     return (navigated_domain.domain_and_registry ==
                             engaged_domain.domain_and_registry);
                   });
  if (already_engaged != engaged_sites.end()) {
    return content::NavigationThrottle::PROCEED;
  }

  ukm::SourceId source_id = ukm::ConvertToSourceId(
      navigation_handle()->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);

  auto* config = GetSafetyTipsRemoteConfigProto();
  const LookalikeTargetAllowlistChecker in_target_allowlist =
      base::BindRepeating(&IsTargetHostAllowlistedBySafetyTipsComponent,
                          config);
  if (GetMatchingDomain(navigated_domain, engaged_sites, in_target_allowlist,
                        &matched_domain, &match_type)) {
    DCHECK(!matched_domain.empty());

    RecordUMAFromMatchType(match_type);

    if (check_safe_redirect &&
        IsSafeRedirect(matched_domain,
                       navigation_handle()->GetRedirectChain())) {
      return content::NavigationThrottle::PROCEED;
    }

    if (ShouldBlockLookalikeUrlNavigation(match_type, navigated_domain)) {
      // matched_domain can be a top domain or an engaged domain. Simply use its
      // eTLD+1 as the suggested domain.
      // 1. If matched_domain is a top domain: Top domain list already contains
      // eTLD+1s only so this works well.
      // 2. If matched_domain is an engaged domain and is not an eTLD+1, don't
      // suggest it. Otherwise, navigating to googlé.com and having engaged with
      // docs.google.com would suggest docs.google.com.
      //
      // When the navigated and matched domains are not eTLD+1s (e.g.
      // docs.googlé.com and docs.google.com), this will suggest google.com
      // instead of docs.google.com. This is less than ideal, but has two
      // benefits:
      // - Simpler code
      // - Fewer suggestions to non-existent domains. E.g. When the navigated
      // domain is nonexistent.googlé.com and the matched domain is
      // docs.google.com, we will suggest google.com instead of
      // nonexistent.google.com.
      const std::string suggested_domain = GetETLDPlusOne(matched_domain);
      DCHECK(!suggested_domain.empty());
      // Drop everything but the parts of the origin.
      GURL::Replacements replace_host;
      replace_host.SetHostStr(suggested_domain);
      const GURL suggested_url =
          url.ReplaceComponents(replace_host).GetWithEmptyPath();
      return ShowInterstitial(suggested_url, url, source_id, match_type);
    }
    // Interstitial normally records UKM, but still record when it's not shown.
    RecordUkmForLookalikeUrlBlockingPage(
        source_id, match_type,
        LookalikeUrlBlockingPageUserAction::kInterstitialNotShown);
    return content::NavigationThrottle::PROCEED;
  }

  if (base::FeatureList::IsEnabled(
          lookalikes::features::kLookalikeInterstitialForPunycode) &&
      ShouldBlockBySpoofCheckResult(navigated_domain)) {
    match_type = LookalikeUrlMatchType::kFailedSpoofChecks;
    RecordUMAFromMatchType(match_type);
    return ShowInterstitial(GURL(), url, source_id, match_type);
  }

  return content::NavigationThrottle::PROCEED;
}
