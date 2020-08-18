// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/zero_suggest_verbatim_match_provider.h"

#include <list>
#include <map>
#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

class ZeroSuggestVerbatimMatchProviderTest : public testing::Test {
 public:
  ZeroSuggestVerbatimMatchProviderTest() = default;
  void SetUp() override;

 protected:
  scoped_refptr<ZeroSuggestVerbatimMatchProvider> provider_;
  MockAutocompleteProviderClient mock_client_;
};

void ZeroSuggestVerbatimMatchProviderTest::SetUp() {
  provider_ = new ZeroSuggestVerbatimMatchProvider(&mock_client_);
}

TEST_F(ZeroSuggestVerbatimMatchProviderTest,
       NoVerbatimMatchWhenOmniboxIsOnNTP) {
  std::string url("chrome://newtab");
  AutocompleteInput input(base::string16(), metrics::OmniboxEventProto::NTP,
                          TestSchemeClassifier());
  input.set_current_url(GURL(url));
  input.set_focus_type(OmniboxFocusType::ON_FOCUS);
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(ZeroSuggestVerbatimMatchProviderTest,
       NoVerbatimMatchForSearchResultsPage) {
  std::string query("https://google.com/search?q=test");
  AutocompleteInput input(
      base::ASCIIToUTF16("test"),
      metrics::OmniboxEventProto::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
      TestSchemeClassifier());
  input.set_current_url(GURL(query));
  input.set_focus_type(OmniboxFocusType::ON_FOCUS);
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(ZeroSuggestVerbatimMatchProviderTest,
       OffersVerbatimMatchInNonIncognito) {
  std::string url("https://www.wired.com/");
  AutocompleteInput input(base::ASCIIToUTF16(url),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_current_url(GURL(url));
  input.set_focus_type(OmniboxFocusType::ON_FOCUS);
  provider_->Start(input, false);
  ASSERT_EQ(1U, provider_->matches().size());
  // Note: we intentionally do not validate the match content here.
  // The content is populated either by HistoryURLProvider or
  // AutocompleteProviderClient both of which we would have to mock for this
  // test. As a result, the test would validate what the mocks fill in.
}

TEST_F(ZeroSuggestVerbatimMatchProviderTest, OffersVerbatimMatchInIncognito) {
  std::string url("https://www.theverge.com/");
  AutocompleteInput input(base::ASCIIToUTF16(url),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_current_url(GURL(url));
  input.set_focus_type(OmniboxFocusType::ON_FOCUS);
  provider_->Start(input, false);
  ASSERT_EQ(1U, provider_->matches().size());
  // Note: we intentionally do not validate the match content here.
  // The content is populated either by HistoryURLProvider or
  // AutocompleteProviderClient both of which we would have to mock for this
  // test. As a result, the test would validate what the mocks fill in.
}

TEST_F(ZeroSuggestVerbatimMatchProviderTest,
       OffersVerbatimMatchNonIncognitoWithEmptyOmnibox) {
  std::string url("https://www.wired.com/");
  AutocompleteInput input(base::string16(),  // Note: empty input.
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_current_url(GURL(url));
  input.set_focus_type(OmniboxFocusType::DEFAULT);
  provider_->Start(input, false);
  ASSERT_EQ(1U, provider_->matches().size());
  // Note: we intentionally do not validate the match content here.
  // The content is populated either by HistoryURLProvider or
  // AutocompleteProviderClient both of which we would have to mock for this
  // test. As a result, the test would validate what the mocks fill in.
}

TEST_F(ZeroSuggestVerbatimMatchProviderTest,
       OffersVerbatimMatchInIncognitoWithEmptyOmnibox) {
  std::string url("https://www.theverge.com/");
  AutocompleteInput input(base::string16(),  // Note: empty input.
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_current_url(GURL(url));
  input.set_focus_type(OmniboxFocusType::DEFAULT);
  provider_->Start(input, false);
  ASSERT_EQ(1U, provider_->matches().size());
  // Note: we intentionally do not validate the match content here.
  // The content is populated either by HistoryURLProvider or
  // AutocompleteProviderClient both of which we would have to mock for this
  // test. As a result, the test would validate what the mocks fill in.
}