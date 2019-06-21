// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_dialog_overlay_mediator.h"

#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ios/chrome/browser/overlays/public/web_content_area/java_script_dialog_source.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_consumer.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation JavaScriptDialogOverlayMediator

- (instancetype)initWithRequest:(OverlayRequest*)request {
  if (self = [super init]) {
    _request = request;
    DCHECK(_request);
  }
  return self;
}

#pragma mark - Accessors

- (const JavaScriptDialogSource*)requestSource {
  NOTREACHED() << "Subclasses must implement.";
  return nullptr;
}

- (void)setConsumer:(id<AlertConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;

  if (!_consumer)
    return;
  const JavaScriptDialogSource* source = self.requestSource;
  NSString* consumerTitle = nil;
  if (source->is_main_frame()) {
    base::string16 title = url_formatter::FormatUrlForSecurityDisplay(
        source->url(), url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
    consumerTitle =
        l10n_util::GetNSStringF(IDS_JAVASCRIPT_MESSAGEBOX_TITLE, title);
  } else {
    consumerTitle = l10n_util::GetNSString(
        IDS_JAVASCRIPT_MESSAGEBOX_TITLE_NONSTANDARD_URL_IFRAME);
  }
  [_consumer setTitle:consumerTitle];
}

@end
