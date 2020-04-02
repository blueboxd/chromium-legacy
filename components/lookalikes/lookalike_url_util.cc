// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lookalikes/lookalike_url_util.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "components/security_state/core/features.h"
#include "components/url_formatter/spoof_checks/top_domains/top500_domains.h"
#include "components/url_formatter/spoof_checks/top_domains/top_domain_util.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"

namespace lookalikes {

const char kHistogramName[] = "NavigationSuggestion.Event";
const base::FeatureParam<std::string> kImportantTlds{
    &security_state::features::kSafetyTipUI, "targetembedding_important_tlds",
    "com,edu,org,gov"};

}  // namespace lookalikes

namespace {

bool SkeletonsMatch(const url_formatter::Skeletons& skeletons1,
                    const url_formatter::Skeletons& skeletons2) {
  DCHECK(!skeletons1.empty());
  DCHECK(!skeletons2.empty());
  for (const std::string& skeleton1 : skeletons1) {
    if (base::Contains(skeletons2, skeleton1)) {
      return true;
    }
  }
  return false;
}

// Returns a site that the user has used before that the eTLD+1 in
// |domain_and_registry| may be attempting to spoof, based on skeleton
// comparison.
std::string GetMatchingSiteEngagementDomain(
    const std::vector<DomainInfo>& engaged_sites,
    const DomainInfo& navigated_domain) {
  DCHECK(!navigated_domain.domain_and_registry.empty());
  for (const DomainInfo& engaged_site : engaged_sites) {
    DCHECK(!engaged_site.domain_and_registry.empty());
    DCHECK_NE(navigated_domain.domain_and_registry,
              engaged_site.domain_and_registry);
    if (SkeletonsMatch(navigated_domain.skeletons, engaged_site.skeletons)) {
      return engaged_site.domain_and_registry;
    }
  }
  return std::string();
}

// Returns the first matching top domain with an edit distance of at most one
// to |domain_and_registry|. This search is done in lexicographic order on the
// top 500 suitable domains, instead of in order by popularity. This means that
// the resulting "similar" domain may not be the most popular domain that
// matches.
std::string GetSimilarDomainFromTop500(const DomainInfo& navigated_domain) {
  for (const std::string& navigated_skeleton : navigated_domain.skeletons) {
    for (const char* const top_domain_skeleton :
         top500_domains::kTop500EditDistanceSkeletons) {
      if (IsEditDistanceAtMostOne(base::UTF8ToUTF16(navigated_skeleton),
                                  base::UTF8ToUTF16(top_domain_skeleton))) {
        const std::string top_domain =
            url_formatter::LookupSkeletonInTopDomains(top_domain_skeleton)
                .domain;
        DCHECK(!top_domain.empty());
        // If the only difference between the navigated and top
        // domains is the registry part, this is unlikely to be a spoofing
        // attempt. Ignore this match and continue. E.g. If the navigated domain
        // is google.com.tw and the top domain is google.com.tr, this won't
        // produce a match.
        const std::string top_domain_without_registry =
            url_formatter::top_domains::HostnameWithoutRegistry(top_domain);
        DCHECK(url_formatter::top_domains::IsEditDistanceCandidate(
            top_domain_without_registry));
        if (navigated_domain.domain_without_registry !=
            top_domain_without_registry) {
          return top_domain;
        }
      }
    }
  }
  return std::string();
}

// Returns the first matching engaged domain with an edit distance of at most
// one to |domain_and_registry|.
std::string GetSimilarDomainFromEngagedSites(
    const DomainInfo& navigated_domain,
    const std::vector<DomainInfo>& engaged_sites) {
  for (const std::string& navigated_skeleton : navigated_domain.skeletons) {
    for (const DomainInfo& engaged_site : engaged_sites) {
      if (!url_formatter::top_domains::IsEditDistanceCandidate(
              engaged_site.domain_and_registry)) {
        continue;
      }
      for (const std::string& engaged_skeleton : engaged_site.skeletons) {
        if (IsEditDistanceAtMostOne(base::UTF8ToUTF16(navigated_skeleton),
                                    base::UTF8ToUTF16(engaged_skeleton))) {
          // If the only difference between the navigated and engaged
          // domain is the registry part, this is unlikely to be a spoofing
          // attempt. Ignore this match and continue. E.g. If the navigated
          // domain is google.com.tw and the top domain is google.com.tr, this
          // won't produce a match.
          if (navigated_domain.domain_without_registry !=
              engaged_site.domain_without_registry) {
            return engaged_site.domain_and_registry;
          }
        }
      }
    }
  }
  return std::string();
}

void RecordEvent(NavigationSuggestionEvent event) {
  UMA_HISTOGRAM_ENUMERATION(lookalikes::kHistogramName, event);
}

// Returns the parts of the url that are separated by "." or "-" not including
// the eTLD.
std::vector<base::string16> SplitNoneTLDDomainIntoTokens(
    const base::string16& host_without_etld) {
  return base::SplitString(host_without_etld, base::ASCIIToUTF16("-."),
                           base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}

// For each possible e2LD+eTLD pair, check whether it forms a top domain.
bool IsTopDomainCandidate(const std::set<std::string>& important_tlds,
                          const base::string16& e2LD,
                          GURL* found_domain) {
  // We need to identify top domains, even when the spoof uses the 'wrong' TLD
  // (e.g. google.gov). To do that, we check the embedded domain with each
  // possible |important_tld| against the top domain list.
  for (const auto& tld : important_tlds) {
    // Create a GURL so we can get a DomainInfo from it for IsTopDomain
    // e2LD is the smallest unit of a domain name that could be registered.
    // (e.g. example in example.com)
    base::string16 target16 =
        e2LD + base::ASCIIToUTF16(".") + base::ASCIIToUTF16(tld);
    GURL possible_target(base::ASCIIToUTF16(url::kHttpsScheme) +
                         base::ASCIIToUTF16(url::kStandardSchemeSeparator) +
                         target16);
    DomainInfo possible_target_domain = GetDomainInfo(possible_target);
    if (IsTopDomain(possible_target_domain)) {
      *found_domain = GURL(possible_target.spec());
      return true;
    }
    // If no match is found, check if e2LD is a unicode spoof
    std::string top_targeted_domain =
        url_formatter::IDNSpoofChecker().GetSimilarTopDomain(target16).domain;
    if (!top_targeted_domain.empty()) {
      *found_domain = GURL(std::string(url::kHttpsScheme) +
                           url::kStandardSchemeSeparator + top_targeted_domain);
      return true;
    }
  }
  return false;
}

}  // namespace

DomainInfo::DomainInfo(const std::string& arg_hostname,
                       const std::string& arg_domain_and_registry,
                       const std::string& arg_domain_without_registry,
                       const url_formatter::IDNConversionResult& arg_idn_result,
                       const url_formatter::Skeletons& arg_skeletons)
    : hostname(arg_hostname),
      domain_and_registry(arg_domain_and_registry),
      domain_without_registry(arg_domain_without_registry),
      idn_result(arg_idn_result),
      skeletons(arg_skeletons) {}

DomainInfo::~DomainInfo() = default;

DomainInfo::DomainInfo(const DomainInfo&) = default;

DomainInfo GetDomainInfo(const GURL& url) {
  if (net::IsLocalhost(url) || net::IsHostnameNonUnique(url.host())) {
    return DomainInfo(std::string(), std::string(), std::string(),
                      url_formatter::IDNConversionResult(),
                      url_formatter::Skeletons());
  }
  const std::string hostname = url.host();
  const std::string domain_and_registry = GetETLDPlusOne(url.host());
  const std::string domain_without_registry =
      domain_and_registry.empty()
          ? std::string()
          : url_formatter::top_domains::HostnameWithoutRegistry(
                domain_and_registry);

  // eTLD+1 can be empty for private domains.
  if (domain_and_registry.empty()) {
    return DomainInfo(hostname, domain_and_registry, domain_without_registry,
                      url_formatter::IDNConversionResult(),
                      url_formatter::Skeletons());
  }
  // Compute skeletons using eTLD+1, skipping all spoofing checks. Spoofing
  // checks in url_formatter can cause the converted result to be punycode.
  // We want to avoid this in order to get an accurate skeleton for the unicode
  // version of the domain.
  const url_formatter::IDNConversionResult idn_result =
      url_formatter::UnsafeIDNToUnicodeWithDetails(domain_and_registry);
  const url_formatter::Skeletons skeletons =
      url_formatter::GetSkeletons(idn_result.result);
  return DomainInfo(hostname, domain_and_registry, domain_without_registry,
                    idn_result, skeletons);
}

std::string GetETLDPlusOne(const std::string& hostname) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      hostname, net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
}

bool IsEditDistanceAtMostOne(const base::string16& str1,
                             const base::string16& str2) {
  if (str1.size() > str2.size() + 1 || str2.size() > str1.size() + 1) {
    return false;
  }
  base::string16::const_iterator i = str1.begin();
  base::string16::const_iterator j = str2.begin();
  size_t edit_count = 0;
  while (i != str1.end() && j != str2.end()) {
    if (*i == *j) {
      i++;
      j++;
    } else {
      edit_count++;
      if (edit_count > 1) {
        return false;
      }

      if (str1.size() > str2.size()) {
        // First string is longer than the second. This can only happen if the
        // first string has an extra character.
        i++;
      } else if (str2.size() > str1.size()) {
        // Second string is longer than the first. This can only happen if the
        // second string has an extra character.
        j++;
      } else {
        // Both strings are the same length. This can only happen if the two
        // strings differ by a single character.
        i++;
        j++;
      }
    }
  }
  if (i != str1.end() || j != str2.end()) {
    // A character at the end did not match.
    edit_count++;
  }
  return edit_count <= 1;
}

bool IsTopDomain(const DomainInfo& domain_info) {
  // Top domains are only accessible through their skeletons, so query the top
  // domains trie for each skeleton of this domain.
  for (const std::string& skeleton : domain_info.skeletons) {
    const url_formatter::TopDomainEntry top_domain =
        url_formatter::LookupSkeletonInTopDomains(skeleton);
    if (domain_info.domain_and_registry == top_domain.domain) {
      return true;
    }
  }
  return false;
}

bool ShouldBlockLookalikeUrlNavigation(LookalikeUrlMatchType match_type,
                                       const DomainInfo& navigated_domain) {
  if (match_type == LookalikeUrlMatchType::kSiteEngagement) {
    return true;
  }
  return match_type == LookalikeUrlMatchType::kTopSite &&
         navigated_domain.idn_result.matching_top_domain.is_top_500;
}

bool GetMatchingDomain(const DomainInfo& navigated_domain,
                       const std::vector<DomainInfo>& engaged_sites,
                       std::string* matched_domain,
                       LookalikeUrlMatchType* match_type) {
  DCHECK(!navigated_domain.domain_and_registry.empty());
  DCHECK(matched_domain);
  DCHECK(match_type);

  if (navigated_domain.idn_result.has_idn_component) {
    // If the navigated domain is IDN, check its skeleton against engaged sites
    // and top domains.
    const std::string matched_engaged_domain =
        GetMatchingSiteEngagementDomain(engaged_sites, navigated_domain);
    if (!matched_engaged_domain.empty()) {
      *matched_domain = matched_engaged_domain;
      *match_type = LookalikeUrlMatchType::kSiteEngagement;
      return true;
    }

    if (!navigated_domain.idn_result.matching_top_domain.domain.empty()) {
      // In practice, this is not possible since the top domain list does not
      // contain IDNs, so domain_and_registry can't both have IDN and be a top
      // domain. Still, sanity check in case the top domain list changes in the
      // future.
      // At this point, navigated domain should not be a top domain.
      DCHECK_NE(navigated_domain.domain_and_registry,
                navigated_domain.idn_result.matching_top_domain.domain);
      *matched_domain = navigated_domain.idn_result.matching_top_domain.domain;
      *match_type = LookalikeUrlMatchType::kTopSite;
      return true;
    }
  }

  if (url_formatter::top_domains::IsEditDistanceCandidate(
          navigated_domain.domain_and_registry)) {
    // If we can't find an exact top domain or an engaged site, try to find an
    // engaged domain within an edit distance of one.
    const std::string similar_engaged_domain =
        GetSimilarDomainFromEngagedSites(navigated_domain, engaged_sites);
    if (!similar_engaged_domain.empty() &&
        navigated_domain.domain_and_registry != similar_engaged_domain) {
      *matched_domain = similar_engaged_domain;
      *match_type = LookalikeUrlMatchType::kEditDistanceSiteEngagement;
      return true;
    }

    // Finally, try to find a top domain within an edit distance of one.
    const std::string similar_top_domain =
        GetSimilarDomainFromTop500(navigated_domain);
    if (!similar_top_domain.empty() &&
        navigated_domain.domain_and_registry != similar_top_domain) {
      *matched_domain = similar_top_domain;
      *match_type = LookalikeUrlMatchType::kEditDistance;
      return true;
    }
  }

  GURL safe_url;
  std::vector<std::string> important_tlds_list =
      base::SplitString(lookalikes::kImportantTlds.Get(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::set<std::string> important_tlds(important_tlds_list.begin(),
                                       important_tlds_list.end());
  if (IsTargetEmbeddingLookalike(
          GURL(std::string(url::kHttpsScheme) +
               std::string(url::kStandardSchemeSeparator) +
               navigated_domain.hostname),
          important_tlds, &safe_url)) {
    *matched_domain = safe_url.host();
    *match_type = LookalikeUrlMatchType::kTargetEmbedding;
    return true;
  }

  return false;
}

void RecordUMAFromMatchType(LookalikeUrlMatchType match_type) {
  switch (match_type) {
    case LookalikeUrlMatchType::kTopSite:
      RecordEvent(NavigationSuggestionEvent::kMatchTopSite);
      break;
    case LookalikeUrlMatchType::kSiteEngagement:
      RecordEvent(NavigationSuggestionEvent::kMatchSiteEngagement);
      break;
    case LookalikeUrlMatchType::kEditDistance:
      RecordEvent(NavigationSuggestionEvent::kMatchEditDistance);
      break;
    case LookalikeUrlMatchType::kEditDistanceSiteEngagement:
      RecordEvent(NavigationSuggestionEvent::kMatchEditDistanceSiteEngagement);
      break;
    case LookalikeUrlMatchType::kTargetEmbedding:
      RecordEvent(NavigationSuggestionEvent::kMatchTargetEmbedding);
      break;
    case LookalikeUrlMatchType::kNone:
      break;
  }
}

bool IsTargetEmbeddingLookalike(const GURL& url,
                                const std::set<std::string>& important_tlds,
                                GURL* safe_url) {
  DCHECK(url.SchemeIsHTTPOrHTTPS());

  size_t registry_length = net::registry_controlled_domains::GetRegistryLength(
      url, net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
      net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  // url.host() will give punycode-encoded hostname, as we need all the unicode
  // characters to stay in the url for further check we convert host to unicode
  base::string16 host =
      url_formatter::UnsafeIDNToUnicodeWithDetails(url.host()).result;
  base::string16 host_without_etld =
      host.substr(0, host.size() - 1 - registry_length);
  const std::vector<base::string16> hostname_tokens_without_etld =
      SplitNoneTLDDomainIntoTokens(host_without_etld);

  // When we find a valid TLD, we look backwards to the previous token
  // to see if we can use it to build a top domain.
  base::string16 prev_part = base::EmptyString16();

  // We could have domains separated by '-'s or '.'s, in order to find target
  // embedding urls with google.com.com or google-com.com, we get url parts as
  // anything that is between two '-'s or '.'s. We check to see if an important
  // TLD is following an important domain.
  // Because of the way this matching is working, we can not identify target
  // embedding attacks on legitimate websites that contain '-' in their names
  // (e.g programme-tv.net).
  for (const auto& token : hostname_tokens_without_etld) {
    if (prev_part.empty()) {
      prev_part = token;
      continue;
    }

    const std::string tld = base::UTF16ToUTF8(token);
    if (base::Contains(important_tlds, tld) &&
        IsTopDomainCandidate(important_tlds, prev_part, safe_url)) {
      return true;
    }
    prev_part = token;
  }
  *safe_url = GURL();
  return false;
}
