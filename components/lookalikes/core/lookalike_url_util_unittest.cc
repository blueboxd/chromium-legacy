// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lookalikes/core/lookalike_url_util.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
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
    EXPECT_EQ(test_case.expected, result);
  }
}

bool IsGoogleScholar(const GURL& hostname) {
  return hostname.host() == "scholar.google.com";
}

TEST(LookalikeUrlUtilTest, TargetEmbeddingTest) {
  const std::vector<DomainInfo> engaged_sites = {
      GetDomainInfo(GURL("https://highengagement.com"))};
  const struct TargetEmbeddingHeuristicTestCase {
    const std::string hostname;
    // Empty when there is no match.
    const std::string safe_hostname;
  } kTestCases[] = {
      // The length of the url should not affect the outcome.
      {"this-is-a-very-long-url-but-it-should-not-affect-the-"
       "outcome-of-this-target-embedding-test-google.com-login.com",
       "google.com"},
      {"google-com-this-is-a-very-long-url-but-it-should-not-affect-"
       "the-outcome-of-this-target-embedding-test-login.com",
       "google.com"},
      {"this-is-a-very-long-url-but-it-should-not-affect-google-the-"
       "outcome-of-this-target-embedding-test.com-login.com",
       ""},
      {"google-this-is-a-very-long-url-but-it-should-not-affect-the-"
       "outcome-of-this-target-embedding-test.com-login.com",
       ""},

      // We need exact skeleton match for our domain so exclude edit-distance
      // matches.
      {"goog0le.com-login.com", ""},

      // Unicode characters should be handled
      {"googlé.com-login.com", "google.com"},
      {"foo-googlé.com-bar.com", "google.com"},

      // The basic states
      {"google.com.foo.com", "google.com"},
      // - before the domain name should be ignored.
      {"foo-google.com-bar.com", "google.com"},
      // The embedded target's TLD doesn't necessarily need to be followed by a
      // '-' and could be a subdomain by itself.
      {"foo-google.com.foo.com", "google.com"},
      {"a.b.c.d.e.f.g.h.foo-google.com.foo.com", "google.com"},
      {"a.b.c.d.e.f.g.h.google.com-foo.com", "google.com"},
      {"1.2.3.4.5.6.google.com-foo.com", "google.com"},
      // Target domain could be in the middle of subdomains.
      {"foo.google.com.foo.com", "google.com"},
      // The target domain and its tld should be next to each other.
      {"foo-google.l.com-foo.com", ""},
      // Target domain might be separated with a dash instead of dot.
      {"foo.google-com-foo.com", "google.com"},

      // Allowlisted domains should not trigger heuristic.
      {"scholar.google.com.foo.com", ""},
      {"scholar.google.com-google.com.foo.com", "google.com"},
      {"google.com-scholar.google.com.foo.com", "google.com"},
      {"foo.scholar.google.com.foo.com", ""},
      {"scholar.foo.google.com.foo.com", "google.com"},

      // Targets should be longer than 6 characters.
      {"hp.com-foo.com", ""},

      // Targets with common words as e2LD are not considered embedded targets
      // either for all TLDs or another-TLD matching.
      {"foo.jobs.com-foo.com", ""},
      {"foo.office.com-foo.com", "office.com"},
      {"foo.jobs.org-foo.com", ""},
      {"foo.office.org-foo.com", ""},

      // Targets could be embedded without their dots and dashes.
      {"foo.googlecom-foo.com", "google.com"},

      // Ensure legitimate domains don't trigger.
      {"foo.google.com", ""},
      {"foo.bar.google.com", ""},
      {"google.com", ""},
      {"google.co.uk", ""},
      {"google.randomreg-login.com", ""},

      // Same tests with another important TLDs.
      {"this-is-a-very-long-url-but-it-should-not-affect-the-"
       "outcome-of-this-target-embedding-test-google.edu-login.com",
       "google.com"},
      {"google-edu-this-is-a-very-long-url-but-it-should-not-affect-"
       "the-outcome-of-this-target-embedding-test-login.com",
       "google.com"},
      {"this-is-a-very-long-url-but-it-should-not-affect-google-the-"
       "outcome-of-this-target-embedding-test.com-login.com",
       ""},
      {"google-this-is-a-very-long-url-but-it-should-not-affect-the-"
       "outcome-of-this-target-embedding-test.com-login.com",
       ""},
      {"goog0le.edu-login.com", ""},
      {"googlé.edu-login.com", "google.com"},
      {"foo-googlé.edu-bar.com", "google.com"},
      {"google.edu.foo.com", "google.com"},
      {"foo-google.edu-bar.com", "google.com"},
      {"foo-google.edu.foo.com", "google.com"},
      {"a.b.c.d.e.f.g.h.foo-google.edu.foo.com", "google.com"},
      {"a.b.c.d.e.f.g.h.google.edu-foo.com", "google.com"},
      {"1.2.3.4.5.6.google.edu-foo.com", "google.com"},
      {"foo.google.edu.foo.com", "google.com"},
      {"foo-google.l.edu-foo.com", ""},
      {"foo.google-edu-foo.com", "google.com"},

      // When ccTLDs are used instead of the actual TLD, it should not trigger
      // the heuristic.
      {"googlebr-foo.com", ""},

      // Allowlisted domains should trigger heuristic when paired with other
      // important TLDs.
      {"scholar.google.edu.foo.com", "google.com"},
      {"scholar.google.edu-google.edu.foo.com", "google.com"},
      {"google.edu-scholar.google.edu.foo.com", "google.com"},
      {"foo.scholar.google.edu.foo.com", "google.com"},
      {"scholar.foo.google.edu.foo.com", "google.com"},

      // Targets should be longer than 6 characters. Even if the embedded domain
      // is longer than 6 characters, if the real target is not more than 6
      // characters, it will be allowlisted.
      {"hp.edu-foo.com", ""},
      {"hp.info-foo.com", ""},

      // Targets that are embedded without their dots and dashes can not use
      // other TLDs.
      {"foo.googleedu-foo.com", ""},
  };

  for (const auto& kTestCase : kTestCases) {
    std::string safe_hostname;
    IsTargetEmbeddingLookalike(kTestCase.hostname, engaged_sites,
                               base::BindRepeating(&IsGoogleScholar),
                               &safe_hostname);

    if (!kTestCase.safe_hostname.empty()) {
      EXPECT_EQ(safe_hostname, kTestCase.safe_hostname)
          << "Expected that \"" << kTestCase.hostname
          << " should trigger because of " << kTestCase.safe_hostname << " But "
          << (safe_hostname.empty() ? "it didn't trigger."
                                    : "triggered because of ")
          << safe_hostname;
    } else {
      EXPECT_EQ(safe_hostname, kTestCase.safe_hostname)
          << "Expected that \"" << kTestCase.hostname
          << " shouldn't trigger but it did. Because of URL:" << safe_hostname;
    }
  }
}
