// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// App interface for the Content Suggestions.
@interface ContentSuggestionsAppInterface : NSObject

// Sets the fake service up.
+ (void)setUpService;

// Resets the service to the real, non-fake service to avoid leaking the fake.
+ (void)resetService;

// Marks the suggestions as available.
+ (void)makeSuggestionsAvailable;

// Disables the suggestions.
+ (void)disableSuggestions;

// Adds |numberOfSuggestions| suggestions to the list of suggestions provided.
// The suggestions have the name "chromium<suggestionNumber>" and the url
// http://chromium/<suggestionNumber>.
+ (void)addNumberOfSuggestions:(NSInteger)numberOfSuggestions
      additionalSuggestionsURL:(NSURL*)URL;

// Add one particular suggestion, following the convention explained above, with
// |suggestionNumber|.
+ (void)addSuggestionNumber:(NSInteger)suggestionNumber;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_APP_INTERFACE_H_
