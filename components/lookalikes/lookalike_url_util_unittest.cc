// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lookalikes/lookalike_url_util.h"

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

TEST(LookalikeUrlUtilTest, TargetEmbeddingTest) {
  const std::set<std::string> important_tlds = {"com", "org", "edu", "gov",
                                                "co"};
  const struct TargetEmbeddingHeuristicTestCase {
    const GURL url;
    bool should_trigger;
  } kTestCases[] = {

      // We test everything with the correct TLD and another popular TLD.

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

      // Unicode characters should be handled
      {GURL("http://googlé.com-login.com"), true},
      {GURL("http://sth-googlé.com-sth.com"), true},

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

      {GURL("http://google.edu.com"), true},
      {GURL("https://google.edu.com"), true},
      {GURL("http://this-is-a-very-long-url-but-it-should-not-affect-the-"
            "outcome-of-this-target-embedding-test-google.edu-login.com"),
       true},
      {GURL(
           "http://this-is-a-very-long-url-but-it-should-not-affect-google-the-"
           "outcome-of-this-target-embedding-test.edu-login.com"),
       false},
      {GURL(
           "http://google-this-is-a-very-long-url-but-it-should-not-affect-the-"
           "outcome-of-this-target-embedding-test.edu-login.com"),
       false},
      {GURL("http://goog0le.edu-login.com"), false},
      {GURL("http://googlé.edu-login.com"), true},
      {GURL("http://sth-googlé.edu-sth.com"), true},
      {GURL("http://google.edu.sth.com"), true},
      {GURL("http://sth-google.edu-sth.com"), true},
      {GURL("http://sth-google.edu.sth.com"), true},
      {GURL("http://a.b.c.d.e.f.g.h.sth-google.edu.sth.com"), true},
      {GURL("http://a.b.c.d.e.f.g.h.google.edu-sth.com"), true},
      {GURL("http://1.2.3.4.5.6.google.edu-sth.com"), true},
      {GURL("http://sth.google.edu.sth.com"), true},
      {GURL("http://sth.google.edu-sth.com"), true},
      {GURL("http://sth-google.l.edu-sth.com"), false},
      {GURL("http://sth-google-l.edu-sth.com"), false},
      {GURL("http://sth-google.l-edu-sth.com"), false},

      // Target domain might be separated with a dash instead of dot.
      {GURL("http://sth.google-com-sth.com"), true},

      // Ensure legitimate domains don't trigger.
      {GURL("http://google.com"), false},
      {GURL("http://google.co.uk"), false},
      {GURL("http://google.randomreg-login.com"), false},

  };

  for (const auto& kTestCase : kTestCases) {
    GURL safe_url = GURL();
    if (kTestCase.should_trigger) {
      EXPECT_TRUE(
          IsTargetEmbeddingLookalike(kTestCase.url, important_tlds, &safe_url))
          << "Expected that \"" << kTestCase.url
          << " should trigger but it didn't.";
    } else {
      EXPECT_FALSE(
          IsTargetEmbeddingLookalike(kTestCase.url, important_tlds, &safe_url))
          << "Expected that \"" << kTestCase.url
          << " shouldn't trigger but it did. For URL: " << safe_url.spec();
    }
  }
}
