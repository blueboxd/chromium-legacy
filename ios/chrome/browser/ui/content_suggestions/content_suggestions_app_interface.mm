// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_app_interface.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ntp_snippets/mock_content_suggestions_provider.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory_util.h"
#include "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_provider_test_singleton.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_test_utils.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ntp_snippets::AdditionalSuggestionsHelper;
using ntp_snippets::Category;
using ntp_snippets::CategoryStatus;
using ntp_snippets::ContentSuggestion;
using ntp_snippets::ContentSuggestionsService;
using ntp_snippets::CreateChromeContentSuggestionsService;
using ntp_snippets::KnownCategories;
using ntp_snippets::MockContentSuggestionsProvider;
using testing::_;
using testing::Invoke;
using testing::WithArg;

namespace {
// Returns a suggestion created from the |category|, |suggestion_id| and the
// |url|.
ContentSuggestion CreateSuggestion(Category category,
                                   std::string suggestion_id,
                                   GURL url) {
  ContentSuggestion suggestion(category, suggestion_id, url);
  suggestion.set_title(base::UTF8ToUTF16(url.spec()));

  return suggestion;
}

}  // namespace

@implementation ContentSuggestionsAppInterface

+ (void)setUpService {
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  // Sets the ContentSuggestionsService associated with this browserState to a
  // service with no provider registered, allowing to register fake providers
  // which do not require internet connection. The previous service is deleted.
  IOSChromeContentSuggestionsServiceFactory::GetInstance()->SetTestingFactory(
      browserState,
      base::BindRepeating(&CreateChromeContentSuggestionsService));

  ContentSuggestionsService* service =
      IOSChromeContentSuggestionsServiceFactory::GetForBrowserState(
          browserState);
  [[ContentSuggestionsTestSingleton sharedInstance]
      registerArticleProvider:service];
}

+ (void)resetService {
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();

  // Resets the Service associated with this browserState to a new service with
  // no providers. The previous service is deleted.
  IOSChromeContentSuggestionsServiceFactory::GetInstance()->SetTestingFactory(
      browserState,
      base::BindRepeating(&CreateChromeContentSuggestionsService));
}

+ (void)makeSuggestionsAvailable {
  [self provider] -> FireCategoryStatusChanged([self category],
                                               CategoryStatus::AVAILABLE);
}

+ (void)disableSuggestions {
  [self provider] -> FireCategoryStatusChanged(
                      [self category],
                      CategoryStatus::ALL_SUGGESTIONS_EXPLICITLY_DISABLED);
}

+ (void)addNumberOfSuggestions:(NSInteger)numberOfSuggestions
      additionalSuggestionsURL:(NSURL*)URL {
  GURL newURL = net::GURLWithNSURL(URL);
  std::vector<ContentSuggestion> suggestions;
  for (NSInteger i = 1; i <= numberOfSuggestions; i++) {
    std::string index = base::SysNSStringToUTF8(@(i).stringValue);
    suggestions.push_back(
        CreateSuggestion([self category], "chromium" + index,
                         GURL("http://chromium.org/" + index)));
  }
  [self provider] -> FireSuggestionsChanged([self category],
                                            std::move(suggestions));

  // Set up the action when "More" is tapped.
  [[ContentSuggestionsTestSingleton sharedInstance]
      resetAdditionalSuggestionsHelperWithURL:newURL];
  EXPECT_CALL(*[self provider], FetchMock(_, _, _))
      .WillOnce(WithArg<2>(
          Invoke([[ContentSuggestionsTestSingleton sharedInstance]
                     additionalSuggestionsHelper],
                 &AdditionalSuggestionsHelper::SendAdditionalSuggestions)));
}

+ (void)addSuggestionNumber:(NSInteger)suggestionNumber {
  std::string index = base::NumberToString(suggestionNumber);
  std::vector<ContentSuggestion> suggestions;
  suggestions.push_back(CreateSuggestion([self category], "chromium" + index,
                                         GURL("http://chromium.org/" + index)));
  [self provider] -> FireSuggestionsChanged([self category],
                                            std::move(suggestions));
}

#pragma mark - Helper

+ (MockContentSuggestionsProvider*)provider {
  return [[ContentSuggestionsTestSingleton sharedInstance] provider];
}

+ (Category)category {
  return Category::FromKnownCategory(KnownCategories::ARTICLES);
}

@end
