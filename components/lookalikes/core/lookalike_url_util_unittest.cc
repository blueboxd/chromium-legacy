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
    const GURL url;
    bool should_trigger;
  } kTestCases[] = {

      // Scheme should not affect the outcome.
      {GURL("http://google.com.com"), true},
      {GURL("https://google.com.com"), true},

      // The length of the url should not affect the outcome.
      {GURL("http://this-is-a-very-long-url-but-it-should-not-affect-the-"
            "outcome-of-this-target-embedding-test-google.com-login.com"),
       true},
      {GURL(
           "http://this-is-a-very-long-url-but-it-should-not-affect-google-the-"
           "outcome-of-this-target-embedding-test.com-login.com"),
       false},
      {GURL(
           "http://google-this-is-a-very-long-url-but-it-should-not-affect-the-"
           "outcome-of-this-target-embedding-test.com-login.com"),
       false},

      // We need exact skeleton match for our domain so exclude edit-distance
      // matches.
      {GURL("http://goog0le.com-login.com"), false},

      // Unicode characters are currently not handled. As a result, target
      // embedding sites that embed lookalikes of top domains aren't flagged.
      {GURL("http://googlé.com-login.com"), false},
      {GURL("http://sth-googlé.com-sth.com"), false},

      // The basic state
      {GURL("http://google.com.sth.com"), true},
      // - before the domain name should be ignored.
      {GURL("http://sth-google.com-sth.com"), true},

      // The embedded target's TLD doesn't necessarily need to be followed by a
      // '-' and could be a subdomain by itself.
      {GURL("http://sth-google.com.sth.com"), true},
      {GURL("http://a.b.c.d.e.f.g.h.sth-google.com.sth.com"), true},
      {GURL("http://a.b.c.d.e.f.g.h.google.com-sth.com"), true},
      {GURL("http://1.2.3.4.5.6.google.com-sth.com"), true},

      // Target domain could be in the middle of subdomains.
      {GURL("http://sth.google.com.sth.com"), true},
      {GURL("http://sth.google.com-sth.com"), true},

      // The target domain and its tld should be next to each other.
      {GURL("http://sth-google.l.com-sth.com"), false},

      // Target domain should match only with its actual TLD.
      {GURL("http://google.edu.com"), false},
      // Target domain might be separated with a dash instead of dot.
      {GURL("http://sth.google-com-sth.com"), true},
      // Target domain could be an engaged domain
      {GURL("http://highengagement-com-login.com"), true},
      // If target domain is an allowlisted domain, it should not trigger the
      // heuristic.
      {GURL("http://foo.scholar.google-com-login.com"), false},
      // An allowlisted domain will make sure it is not marked as an embedded
      // target. However, other targets could still be embedded in the domain.
      {GURL("http://foo.google.com.scholar.google-com-login.com"), true},
      {GURL("http://foo.scholar.google-com.google.com-login.com"), true},

      // Ensure legitimate domains don't trigger the heuristic.
      {GURL("http://google.com"), false},
      {GURL("http://google.co.uk"), false},
      {GURL("http://google.randomreg-login.com"), false},

  };

  for (const auto& kTestCase : kTestCases) {
    std::string safe_hostname;
    if (kTestCase.should_trigger) {
      EXPECT_TRUE(IsTargetEmbeddingLookalike(
          kTestCase.url.host(), engaged_sites,
          base::BindRepeating(&IsGoogleScholar), &safe_hostname))
          << "Expected that \"" << kTestCase.url
          << " should trigger but it didn't.";
    } else {
      EXPECT_FALSE(IsTargetEmbeddingLookalike(
          kTestCase.url.host(), engaged_sites,
          base::BindRepeating(&IsGoogleScholar), &safe_hostname))
          << "Expected that \"" << kTestCase.url
          << " shouldn't trigger but it did. For URL: " << safe_hostname;
    }
  }
}
