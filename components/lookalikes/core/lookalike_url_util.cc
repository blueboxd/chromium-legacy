// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lookalikes/core/lookalike_url_util.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "components/lookalikes/core/features.h"
#include "components/security_state/core/features.h"
#include "components/url_formatter/spoof_checks/top_domains/top500_domains.h"
#include "components/url_formatter/spoof_checks/top_domains/top_domain_util.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"

namespace lookalikes {

const char kHistogramName[] = "NavigationSuggestion.Event";

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
std::string GetSimilarDomainFromTop500(
    const DomainInfo& navigated_domain,
    const LookalikeTargetAllowlistChecker& target_allowlisted) {
  for (const std::string& navigated_skeleton : navigated_domain.skeletons) {
    for (const char* const top_domain_skeleton :
         top500_domains::kTop500EditDistanceSkeletons) {
      if (!IsEditDistanceAtMostOne(base::UTF8ToUTF16(navigated_skeleton),
                                   base::UTF8ToUTF16(top_domain_skeleton))) {
        continue;
      }

      const std::string top_domain =
          url_formatter::LookupSkeletonInTopDomains(top_domain_skeleton).domain;
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
      if (navigated_domain.domain_without_registry ==
          top_domain_without_registry) {
        continue;
      }

      // Skip past domains that are allowed to be spoofed.
      if (target_allowlisted.Run(GURL(std::string(url::kHttpsScheme) +
                                      url::kStandardSchemeSeparator +
                                      top_domain))) {
        continue;
      }

      return top_domain;
    }
  }
  return std::string();
}

// Returns the first matching engaged domain with an edit distance of at most
// one to |domain_and_registry|.
std::string GetSimilarDomainFromEngagedSites(
    const DomainInfo& navigated_domain,
    const std::vector<DomainInfo>& engaged_sites,
    const LookalikeTargetAllowlistChecker& target_allowlisted) {
  for (const std::string& navigated_skeleton : navigated_domain.skeletons) {
    for (const DomainInfo& engaged_site : engaged_sites) {
      if (!url_formatter::top_domains::IsEditDistanceCandidate(
              engaged_site.domain_and_registry)) {
        continue;
      }
      for (const std::string& engaged_skeleton : engaged_site.skeletons) {
        if (!IsEditDistanceAtMostOne(base::UTF8ToUTF16(navigated_skeleton),
                                     base::UTF8ToUTF16(engaged_skeleton))) {
          continue;
        }

        // If the only difference between the navigated and engaged
        // domain is the registry part, this is unlikely to be a spoofing
        // attempt. Ignore this match and continue. E.g. If the navigated
        // domain is google.com.tw and the top domain is google.com.tr, this
        // won't produce a match.
        if (navigated_domain.domain_without_registry ==
            engaged_site.domain_without_registry) {
          continue;
        }

        // Skip past domains that are allowed to be spoofed.
        if (target_allowlisted.Run(GURL(std::string(url::kHttpsScheme) +
                                        url::kStandardSchemeSeparator +
                                        engaged_site.domain_and_registry))) {
          continue;
        }

        return engaged_site.domain_and_registry;
      }
    }
  }
  return std::string();
}

void RecordEvent(NavigationSuggestionEvent event) {
  UMA_HISTOGRAM_ENUMERATION(lookalikes::kHistogramName, event);
}

// Returns the parts of the domain that are separated by "." or "-", not
// including the eTLD.
std::vector<std::string> SplitDomainWithouteTLDIntoTokens(
    const std::string& host_without_etld) {
  return base::SplitString(host_without_etld, "-.", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

// Checks whether |domain| is a top domain. If yes, returns true and fills
// |found_domain| with the matching top domain.
bool IsTop500Domain(const DomainInfo& domain, std::string* found_domain) {
  for (auto& skeleton : domain.skeletons) {
    // Matching with top domains is only done with skeleton matching. We check
    // if the skeleton of our hostname matches the skeleton of any top domain.
    url_formatter::TopDomainEntry matched_domain =
        url_formatter::IDNSpoofChecker().LookupSkeletonInTopDomains(skeleton);
    // We are only interested in an exact match with a top 500 domain (as
    // opposed to skeleton match). Here we check that the matched domain is a
    // top 500 domain and also the hostname of the matched domain is exactly the
    // same as our input eTLD+1.
    if (matched_domain.is_top_500 &&
        matched_domain.domain == domain.domain_and_registry) {
      *found_domain = matched_domain.domain;
      return true;
    }
  }
  return false;
}

// Checks if the targeted domain is allowlisted. To check that we need to
// check all of the subdomains that could be made. The reason is for example
// in the case of "foo.scholar.google.com.university.edu", "google.com" is
// considered as the targeted domain. We need to make sure
// "scholar.google.com" or "foo.scholar.google.com" are not allowlisted
// before marking the input domain as a target embedding domain.
bool ASubdomainIsAllowlisted(
    const std::string& embedded_target,
    const base::span<const std::string>& subdomain_labels_so_far,
    const LookalikeTargetAllowlistChecker& in_target_allowlist) {
  const std::string https_scheme =
      url::kHttpsScheme + std::string(url::kStandardSchemeSeparator);

  if (in_target_allowlist.Run(GURL(https_scheme + embedded_target))) {
    return true;
  }
  std::string potential_hostname = embedded_target;
  // Attach each token from the end to the embedded target to check if that
  // subdomain has been allowlisted.
  for (int i = subdomain_labels_so_far.size() - 1; i >= 0; i--) {
    potential_hostname = subdomain_labels_so_far[i] + "." + potential_hostname;
    if (in_target_allowlist.Run(GURL(https_scheme + potential_hostname))) {
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
  if (match_type == LookalikeUrlMatchType::kTargetEmbedding &&
      base::FeatureList::IsEnabled(
          lookalikes::features::kDetectTargetEmbeddingLookalikes)) {
    return true;
  }
  return match_type == LookalikeUrlMatchType::kTopSite &&
         navigated_domain.idn_result.matching_top_domain.is_top_500;
}

bool GetMatchingDomain(
    const DomainInfo& navigated_domain,
    const std::vector<DomainInfo>& engaged_sites,
    const LookalikeTargetAllowlistChecker& in_target_allowlist,
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
    DCHECK_NE(navigated_domain.domain_and_registry, matched_engaged_domain);
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
    const std::string similar_engaged_domain = GetSimilarDomainFromEngagedSites(
        navigated_domain, engaged_sites, in_target_allowlist);
    if (!similar_engaged_domain.empty() &&
        navigated_domain.domain_and_registry != similar_engaged_domain) {
      *matched_domain = similar_engaged_domain;
      *match_type = LookalikeUrlMatchType::kEditDistanceSiteEngagement;
      return true;
    }

    // Finally, try to find a top domain within an edit distance of one.
    const std::string similar_top_domain =
        GetSimilarDomainFromTop500(navigated_domain, in_target_allowlist);
    if (!similar_top_domain.empty() &&
        navigated_domain.domain_and_registry != similar_top_domain) {
      *matched_domain = similar_top_domain;
      *match_type = LookalikeUrlMatchType::kEditDistance;
      return true;
    }
  }

  if (IsTargetEmbeddingLookalike(navigated_domain.hostname, engaged_sites,
                                 in_target_allowlist, matched_domain)) {
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

bool IsTargetEmbeddingLookalike(
    const std::string& hostname,
    const std::vector<DomainInfo>& engaged_sites,
    const LookalikeTargetAllowlistChecker& in_target_allowlist,
    std::string* safe_hostname) {
  const std::string host_without_etld =
      url_formatter::top_domains::HostnameWithoutRegistry(hostname);
  const std::vector<std::string> hostname_tokens_without_etld =
      SplitDomainWithouteTLDIntoTokens(host_without_etld);

  // For each token, we look backwards to the previous token to see if
  // "|prev_token|.|token|" forms a top domain or a high engaged domain.
  std::string prev_token;

  // We can have domains separated by '-'s or '.'s. In order to find target
  // embedding urls with google.com.com or google-com.com, we get url parts as
  // anything that is between two '-'s or '.'s. We check to see if any two
  // consecutive tokens form a top or highly-engaged domain.
  // Because of the way this matching is working, we can not identify target
  // embedding attacks against domains that contain '-' in their address
  // (e.g programme-tv.net). Also if the eTLD of the target has more than one
  // part, we won't be able to protect it (e.g. google.co.uk).
  for (size_t i = 0; i < hostname_tokens_without_etld.size(); i++) {
    const std::string token = hostname_tokens_without_etld[i];
    const std::string possible_embedded_target = prev_token + "." + token;
    if (prev_token.empty()) {
      prev_token = token;
      continue;
    }
    prev_token = token;

    // Short domains are more likely to be misidentified as being embedded. For
    // example "mi.com", "mk.ru", or "com.ru" are a few examples of domains that
    // could trigger the target embedding heuristic falsely.
    if (possible_embedded_target.size() < 7) {
      continue;
    }

    // We want to protect user's high engaged websites as well as top domains.
    GURL possible_target_url(url::kHttpsScheme +
                             std::string(url::kStandardSchemeSeparator) +
                             possible_embedded_target);
    DomainInfo possible_target_domain = GetDomainInfo(possible_target_url);
    // We check if the eTLD+1 is a valid domain, otherwise there is no point in
    // checking if it is a top domain or an engaged domain.
    if (possible_target_domain.domain_and_registry.empty()) {
      continue;
    }

    *safe_hostname =
        GetMatchingSiteEngagementDomain(engaged_sites, possible_target_domain);
    // |GetMatchingSiteEngagementDomain| uses skeleton matching, we make sure
    // the found engaged site is an exact match of the embedded target.
    if (*safe_hostname != possible_embedded_target) {
      *safe_hostname = std::string();
    }
    if (safe_hostname->empty() &&
        !IsTop500Domain(possible_target_domain, safe_hostname)) {
      continue;
    }

    // Check if any subdomain is allowlisted.
    std::vector<std::string> subdomain_labels_so_far(
        hostname_tokens_without_etld.begin(),
        hostname_tokens_without_etld.begin() + i - 1);
    if (!ASubdomainIsAllowlisted(possible_embedded_target,
                                 subdomain_labels_so_far,
                                 in_target_allowlist)) {
      return true;
    }

    // A target is found but it was allowlisted.
    *safe_hostname = std::string();
  }
  return false;
}
