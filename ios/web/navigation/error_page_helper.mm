// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/error_page_helper.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Escapes HTML characters in |text|.
NSString* EscapeHTMLCharacters(NSString* text) {
  return base::SysUTF8ToNSString(
      net::EscapeForHTML(base::SysNSStringToUTF8(text)));
}

// Resturns the path for the error page to be loaded.
NSString* LoadedErrorPageFilePath() {
  NSString* path = [NSBundle.mainBundle pathForResource:@"error_page_loaded"
                                                 ofType:@"html"];
  DCHECK(path) << "Loaded error page should exist";
  return path;
}

// Returns the path for the error page to be injected.
NSString* InjectedErrorPageFilePath() {
  NSString* path = [NSBundle.mainBundle pathForResource:@"error_page_injected"
                                                 ofType:@"html"];
  DCHECK(path) << "Injected error page should exist";
  return path;
}

}  // namespace

@interface ErrorPageHelper ()
@property(nonatomic, strong) NSError* error;
// The error page HTML to be injected into existing page.
@property(nonatomic, strong, readonly) NSString* HTMLToInject;
@property(nonatomic, strong, readonly) NSString* failedNavigationURLString;
@end

@implementation ErrorPageHelper

@synthesize failedNavigationURL = _failedNavigationURL;
@synthesize errorPageFileURL = _errorPageFileURL;
@synthesize HTMLToInject = _HTMLToInject;
@synthesize scriptToInject = _scriptToInject;

- (instancetype)initWithError:(NSError*)error {
  if (self = [super init]) {
    _error = [error copy];
  }
  return self;
}

#pragma mark - Properties

- (NSURL*)failedNavigationURL {
  if (!_failedNavigationURL) {
    _failedNavigationURL = [NSURL URLWithString:self.failedNavigationURLString];
  }
  return _failedNavigationURL;
}

- (NSString*)failedNavigationURLString {
  return self.error.userInfo[NSURLErrorFailingURLStringErrorKey];
}

- (NSURL*)errorPageFileURL {
  if (!_errorPageFileURL) {
    NSURLQueryItem* itemURL =
        [NSURLQueryItem queryItemWithName:@"url"
                                    value:self.failedNavigationURLString];
    NSURLQueryItem* itemError =
        [NSURLQueryItem queryItemWithName:@"error"
                                    value:_error.localizedDescription];
    NSURLQueryItem* itemDontLoad = [NSURLQueryItem queryItemWithName:@"dontLoad"
                                                               value:@"true"];
    NSURLComponents* URL = [[NSURLComponents alloc] initWithString:@"file:///"];
    URL.path = LoadedErrorPageFilePath();
    URL.queryItems = @[ itemURL, itemError, itemDontLoad ];
    DCHECK(URL.URL) << "file URL should be valid";
    _errorPageFileURL = URL.URL;
  }
  return _errorPageFileURL;
}

- (NSString*)HTMLToInject {
  if (!_HTMLToInject) {
    NSString* path = InjectedErrorPageFilePath();
    NSString* HTMLTemplate =
        [NSString stringWithContentsOfFile:path
                                  encoding:NSUTF8StringEncoding
                                     error:nil];
    NSString* failedNavigationURLString =
        EscapeHTMLCharacters(self.failedNavigationURLString);
    NSString* errorInfo = EscapeHTMLCharacters(self.error.localizedDescription);
    _HTMLToInject =
        [NSString stringWithFormat:HTMLTemplate, failedNavigationURLString,
                                   failedNavigationURLString, errorInfo];
  }
  return _HTMLToInject;
}

- (NSString*)scriptToInject {
  if (!_scriptToInject) {
    NSString* JSON = [[NSString alloc]
        initWithData:[NSJSONSerialization
                         dataWithJSONObject:@[ self.HTMLToInject ]
                                    options:0
                                      error:nil]
            encoding:NSUTF8StringEncoding];
    NSString* escapedHtml =
        [JSON substringWithRange:NSMakeRange(1, JSON.length - 2)];

    _scriptToInject =
        [NSString stringWithFormat:
                      @"document.open(); document.write(%@); document.close();",
                      escapedHtml];
  }
  return _scriptToInject;
}

#pragma mark - Public

+ (GURL)failedNavigationURLFromErrorPageFileURL:(const GURL&)URL {
  if (!URL.is_valid())
    return GURL();

  if (URL.SchemeIsFile() &&
      URL.path() == base::SysNSStringToUTF8(LoadedErrorPageFilePath())) {
    for (net::QueryIterator it(URL); !it.IsAtEnd(); it.Advance()) {
      if (it.GetKey() == "file") {
        return GURL(it.GetValue());
      }
    }
  }

  return GURL();
}

- (BOOL)isErrorPageFileURLForFailedNavigationURL:(NSURL*)URL {
  // Check that |URL| is a file URL of error page.
  if (!URL.fileURL || ![URL.path isEqualToString:self.errorPageFileURL.path]) {
    return NO;
  }
  // Check that |URL| has the same failed URL as |self|.
  NSURLComponents* URLComponents = [NSURLComponents componentsWithURL:URL
                                              resolvingAgainstBaseURL:NO];
  NSURL* failedNavigationURL = nil;
  for (NSURLQueryItem* item in URLComponents.queryItems) {
    if ([item.name isEqualToString:@"url"]) {
      failedNavigationURL = [NSURL URLWithString:item.value];
      break;
    }
  }
  return [failedNavigationURL isEqual:self.failedNavigationURL];
}

@end
