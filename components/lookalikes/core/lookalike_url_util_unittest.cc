// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lookalikes/core/lookalike_url_util.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/lookalikes/core/features.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(LookalikeUrlUtilTest, IsEditDistanceAtMostOne) {
  const struct TestCase {
    const wchar_t* domain;
    const wchar_t* top_domain;
    bool expected;
  } kTestCases[] = {
      {L"", L"", true},
      {L"a", L"a", true},
      {L"a", L"", true},
      {L"", L"a", true},

      {L"", L"ab", false},
      {L"ab", L"", false},

      {L"ab", L"a", true},
      {L"a", L"ab", true},
      {L"ab", L"b", true},
      {L"b", L"ab", true},
      {L"ab", L"ab", true},

      {L"", L"ab", false},
      {L"ab", L"", false},
      {L"a", L"abc", false},
      {L"abc", L"a", false},

      {L"aba", L"ab", true},
      {L"ba", L"aba", true},
      {L"abc", L"ac", true},
      {L"ac", L"abc", true},

      // Same length.
      {L"xbc", L"ybc", true},
      {L"axc", L"ayc", true},
      {L"abx", L"aby", true},

      // Should also work for non-ASCII.
      {L"é", L"", true},
      {L"", L"é", true},
      {L"tést", L"test", true},
      {L"test", L"tést", true},
      {L"tés", L"test", false},
      {L"test", L"tés", false},

      // Real world test cases.
      {L"google.com", L"gooogle.com", true},
      {L"gogle.com", L"google.com", true},
      {L"googlé.com", L"google.com", true},
      {L"google.com", L"googlé.com", true},
      // Different by two characters.
      {L"google.com", L"goooglé.com", false},
  };
  for (const TestCase& test_case : kTestCases) {
    bool result =
        IsEditDistanceAtMostOne(base::WideToUTF16(test_case.domain),
                                base::WideToUTF16(test_case.top_domain));
    EXPECT_EQ(test_case.expected, result)
        << "when comparing " << test_case.domain << " with "
        << test_case.top_domain;
  }
}

TEST(LookalikeUrlUtilTest, EditDistanceExcludesCommonFalsePositives) {
  const struct TestCase {
    const char* domain;
    const char* top_domain;
    bool is_likely_false_positive;
  } kTestCases[] = {
      // Most edit distance instances are not likely false positives.
      {"abcxd.com", "abcyd.com", false},   // Substitution
      {"abcxd.com", "abcxxd.com", false},  // Deletion
      {"abcxxd.com", "abcxd.com", false},  // Insertion

      // But we permit cases where the only difference is in the tld.
      {"abcde.com", "abcde.net", true},

      // We also permit matches that are only due to a numeric suffix,
      {"abcd1.com", "abcd2.com", true},    // Substitution
      {"abcde.com", "abcde1.com", true},   // Numeric deletion
      {"abcde1.com", "abcde.com", true},   // Numeric insertion
      {"abcd11.com", "abcd21.com", true},  // Not-final-digit substitution
      {"a.abcd1.com", "abcd2.com", true},  // Only relevant for eTLD+1.
      // ...and that change must be due to the numeric suffix.
      {"abcx1.com", "abcy1.com", false},   // Substitution before suffix
      {"abcd1.com", "abcde1.com", false},  // Deletion before suffix
      {"abcde1.com", "abcd1.com", false},  // Insertion before suffix
      {"abcdx.com", "abcdy.com", false},   // Non-numeric substitution at end

      // We also permit matches that are only due to a first-character change,
      {"xabcd.com", "yabcd.com", true},     // Substitution
      {"xabcde.com", "abcde.com", true},    // Insertion
      {"abcde.com", "xabcde.com", true},    // Deletion
      {"a.abcde.com", "xabcde.com", true},  // For eTLD+1
      // ...so long as that change is only on the first character, not later.
      {"abcde.com", "axbcde.com", false},   // Deletion
      {"axbcde.com", "abcde.com", false},   // Insertion
      {"axbcde.com", "aybcde.com", false},  // Substitution
  };
  for (const TestCase& test_case : kTestCases) {
    auto navigated =
        GetDomainInfo(GURL(std::string(url::kHttpsScheme) +
                           url::kStandardSchemeSeparator + test_case.domain));
    auto matched = GetDomainInfo(GURL(std::string(url::kHttpsScheme) +
                                      url::kStandardSchemeSeparator +
                                      test_case.top_domain));
    bool result = IsLikelyEditDistanceFalsePositive(navigated, matched);
    EXPECT_EQ(test_case.is_likely_false_positive, result)
        << "when comparing " << test_case.domain << " with "
        << test_case.top_domain;
  }
}

bool IsGoogleScholar(const std::string& hostname) {
  return hostname == "scholar.google.com";
}

struct TargetEmbeddingHeuristicTestCase {
  const std::string hostname;
  // Empty when there is no match.
  const std::string expected_safe_host;
  const TargetEmbeddingType expected_type;
};

void ValidateTestCases(
    const std::vector<DomainInfo>& engaged_sites,
    const std::vector<TargetEmbeddingHeuristicTestCase>& test_cases) {
  for (auto& test_case : test_cases) {
    std::string safe_hostname;
    TargetEmbeddingType embedding_type = GetTargetEmbeddingType(
        test_case.hostname, engaged_sites,
        base::BindRepeating(&IsGoogleScholar), &safe_hostname);
    if (test_case.expected_type != TargetEmbeddingType::kNone) {
      EXPECT_EQ(safe_hostname, test_case.expected_safe_host)
          << "Expected that \"" << test_case.hostname
          << " should trigger because of " << test_case.expected_safe_host
          << " But "
          << (safe_hostname.empty() ? "it didn't trigger."
                                    : "triggered because of ")
          << safe_hostname;
      EXPECT_EQ(embedding_type, test_case.expected_type)
          << "Right warning type was not triggered for " << test_case.hostname
          << ".";
    } else {
      EXPECT_EQ(embedding_type, TargetEmbeddingType::kNone)
          << "Expected that " << test_case.hostname
          << "\" shouldn't trigger but it did. Because of URL:"
          << safe_hostname;
    }
  }
}

TEST(LookalikeUrlUtilTest, TargetEmbeddingTest) {
  const std::vector<DomainInfo> kEngagedSites = {
      GetDomainInfo(GURL("https://highengagement.com"))};
  const std::vector<TargetEmbeddingHeuristicTestCase> kTestCases = {
      // The length of the url should not affect the outcome.
      {"this-is-a-very-long-url-but-it-should-not-affect-the-"
       "outcome-of-this-target-embedding-test-google.com-login.com",
       "google.com", TargetEmbeddingType::kInterstitial},
      {"google-com-this-is-a-very-long-url-but-it-should-not-affect-"
       "the-outcome-of-this-target-embedding-test-login.com",
       "google.com", TargetEmbeddingType::kInterstitial},
      {"this-is-a-very-long-url-but-it-should-not-affect-google-the-"
       "outcome-of-this-target-embedding-test.com-login.com",
       "", TargetEmbeddingType::kNone},
      {"google-this-is-a-very-long-url-but-it-should-not-affect-the-"
       "outcome-of-this-target-embedding-test.com-login.com",
       "", TargetEmbeddingType::kNone},

      // We need exact skeleton match for our domain so exclude edit-distance
      // matches.
      {"goog0le.com-login.com", "", TargetEmbeddingType::kNone},

      // Unicode characters should be handled
      {"googlé.com-login.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"foo-googlé.com-bar.com", "google.com",
       TargetEmbeddingType::kInterstitial},

      // The basic states
      {"google.com.foo.com", "google.com", TargetEmbeddingType::kInterstitial},
      // - before the domain name should be ignored.
      {"foo-google.com-bar.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      // The embedded target's TLD doesn't necessarily need to be followed by a
      // '-' and could be a subdomain by itself.
      {"foo-google.com.foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"a.b.c.d.e.f.g.h.foo-google.com.foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"a.b.c.d.e.f.g.h.google.com-foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"1.2.3.4.5.6.google.com-foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      // Target domain could be in the middle of subdomains.
      {"foo.google.com.foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      // The target domain and its tld should be next to each other.
      {"foo-google.l.com-foo.com", "", TargetEmbeddingType::kNone},
      // Target domain might be separated with a dash instead of dot.
      {"foo.google-com-foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},

      // Allowlisted domains should not trigger heuristic.
      {"scholar.google.com.foo.com", "", TargetEmbeddingType::kNone},
      {"scholar.google.com-google.com.foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"google.com-scholar.google.com.foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"foo.scholar.google.com.foo.com", "", TargetEmbeddingType::kNone},
      {"scholar.foo.google.com.foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},

      // Targets should be longer than 6 characters.
      {"hp.com-foo.com", "", TargetEmbeddingType::kNone},

      // Targets with common words as e2LD are not considered embedded targets
      // either for all TLDs or another-TLD matching.
      {"foo.jobs.com-foo.com", "", TargetEmbeddingType::kNone},
      {"foo.office.com-foo.com", "office.com",
       TargetEmbeddingType::kInterstitial},
      {"foo.jobs.org-foo.com", "", TargetEmbeddingType::kNone},
      {"foo.office.org-foo.com", "", TargetEmbeddingType::kNone},

      // Targets could be embedded without their dots and dashes.
      {"foo.googlecom-foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},

      // Ensure legitimate domains don't trigger.
      {"foo.google.com", "", TargetEmbeddingType::kNone},
      {"foo.bar.google.com", "", TargetEmbeddingType::kNone},
      {"google.com", "", TargetEmbeddingType::kNone},
      {"google.co.uk", "", TargetEmbeddingType::kNone},
      {"google.randomreg-login.com", "", TargetEmbeddingType::kNone},
  };

  // Test cases for "enhanced protection", aka mixed-TLD, target embedding.
  const std::vector<TargetEmbeddingHeuristicTestCase> kEPTestCases = {
      // Same tests with another important TLDs.
      {"this-is-a-very-long-url-but-it-should-not-affect-the-"
       "outcome-of-this-target-embedding-test-google.edu-login.com",
       "google.com", TargetEmbeddingType::kInterstitial},
      {"google-edu-this-is-a-very-long-url-but-it-should-not-affect-"
       "the-outcome-of-this-target-embedding-test-login.com",
       "google.com", TargetEmbeddingType::kInterstitial},
      {"this-is-a-very-long-url-but-it-should-not-affect-google-the-"
       "outcome-of-this-target-embedding-test.com-login.com",
       "", TargetEmbeddingType::kNone},
      {"google-this-is-a-very-long-url-but-it-should-not-affect-the-"
       "outcome-of-this-target-embedding-test.com-login.com",
       "", TargetEmbeddingType::kNone},
      {"goog0le.edu-login.com", "", TargetEmbeddingType::kNone},
      {"googlé.edu-login.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"foo-googlé.edu-bar.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"google.edu.foo.com", "google.com", TargetEmbeddingType::kInterstitial},
      {"foo-google.edu-bar.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"foo-google.edu.foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"a.b.c.d.e.f.g.h.foo-google.edu.foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"a.b.c.d.e.f.g.h.google.edu-foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"1.2.3.4.5.6.google.edu-foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"foo.google.edu.foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"foo-google.l.edu-foo.com", "", TargetEmbeddingType::kNone},
      {"foo.google-edu-foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},

      // When ccTLDs are used instead of the actual TLD, it will still trigger
      // the heuristic but will show Safety Tips instead of Lookalike
      // Interstitials.
      {"google.br-foo.com", "google.com", TargetEmbeddingType::kSafetyTip},

      // Allowlisted domains should trigger heuristic when paired with other
      // important TLDs.
      {"scholar.google.edu.foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"scholar.google.edu-google.edu.foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"google.edu-scholar.google.edu.foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"foo.scholar.google.edu.foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},
      {"scholar.foo.google.edu.foo.com", "google.com",
       TargetEmbeddingType::kInterstitial},

      // Targets should be longer than 6 characters. Even if the embedded domain
      // is longer than 6 characters, if the real target is not more than 6
      // characters, it will be allowlisted.
      {"hp.edu-foo.com", "", TargetEmbeddingType::kNone},
      {"hp.info-foo.com", "", TargetEmbeddingType::kNone},

      // Targets that are embedded without their dots and dashes can not use
      // other TLDs.
      {"foo.googleedu-foo.com", "", TargetEmbeddingType::kNone},
  };

  ValidateTestCases(kEngagedSites, kTestCases);

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      lookalikes::features::kDetectTargetEmbeddingLookalikes,
      {{"enhanced_protection_enabled", "true"}});

  ValidateTestCases(kEngagedSites, kEPTestCases);
}
