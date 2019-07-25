// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/http_auth_dialogs/http_auth_dialog_overlay_mediator.h"

#include "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/public/overlay_response.h"
#include "ios/chrome/browser/overlays/public/web_content_area/http_auth_overlay.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_action.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_consumer.h"
#import "ios/chrome/browser/ui/elements/text_field_configuration.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface HTTPAuthDialogOverlayMediator ()
@property(nonatomic, readonly) OverlayRequest* request;
// The HTTP authentication config.
@property(nonatomic, readonly) HTTPAuthOverlayRequestConfig* config;
@end

@implementation HTTPAuthDialogOverlayMediator

- (instancetype)initWithRequest:(OverlayRequest*)request {
  if (self = [super init]) {
    _request = request;
    DCHECK(_request);
    // Verify that the request is configured for HTTP authentication dialogs.
    DCHECK(_request->GetConfig<HTTPAuthOverlayRequestConfig>());
  }
  return self;
}

#pragma mark - Accessors

- (HTTPAuthOverlayRequestConfig*)config {
  return self.request->GetConfig<HTTPAuthOverlayRequestConfig>();
}

- (void)setConsumer:(id<AlertConsumer>)consumer {
  if (self.consumer == consumer)
    return;
  _consumer = consumer;
  [_consumer setTitle:l10n_util::GetNSStringWithFixup(IDS_LOGIN_DIALOG_TITLE)];
  [self.consumer setMessage:base::SysUTF8ToNSString(self.config->message())];
  __weak __typeof__(self) weakSelf = self;
  NSString* OKLabel =
      l10n_util::GetNSStringWithFixup(IDS_LOGIN_DIALOG_OK_BUTTON_LABEL);
  [self.consumer setActions:@[
    [AlertAction actionWithTitle:OKLabel
                           style:UIAlertActionStyleDefault
                         handler:^(AlertAction* action) {
                           __typeof__(self) strongSelf = weakSelf;
                           [strongSelf updateResponseCancelled:NO];
                           [strongSelf.delegate
                               stopDialogForMediator:strongSelf];
                         }],
    [AlertAction actionWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                           style:UIAlertActionStyleCancel
                         handler:^(AlertAction* action) {
                           __typeof__(self) strongSelf = weakSelf;
                           [strongSelf updateResponseCancelled:YES];
                           [strongSelf.delegate
                               stopDialogForMediator:strongSelf];
                         }],
  ]];

  NSString* defaultUsername =
      base::SysUTF8ToNSString(self.config->default_username());
  NSString* usernamePlaceholder =
      l10n_util::GetNSString(IDS_IOS_HTTP_LOGIN_DIALOG_USERNAME_PLACEHOLDER);
  NSString* passwordPlaceholder =
      l10n_util::GetNSString(IDS_IOS_HTTP_LOGIN_DIALOG_PASSWORD_PLACEHOLDER);
  [self.consumer setTextFieldConfigurations:@[
    [[TextFieldConfiguration alloc] initWithText:defaultUsername
                                     placeholder:usernamePlaceholder
                         accessibilityIdentifier:nil
                                 secureTextEntry:NO],
    [[TextFieldConfiguration alloc] initWithText:nil
                                     placeholder:passwordPlaceholder
                         accessibilityIdentifier:nil
                                 secureTextEntry:YES]
  ]];
}

#pragma mark - Response helpers

// Sets the OverlayResponse using the user input from the prompt UI.
// |cancelled| indicates whether the alert's cancel button was tapped.
- (void)updateResponseCancelled:(BOOL)cancelled {
  std::unique_ptr<OverlayResponse> response;
  if (!cancelled) {
    std::string user =
        base::SysNSStringToUTF8([self.dataSource userForMediator:self]);
    std::string password =
        base::SysNSStringToUTF8([self.dataSource passwordForMediator:self]);
    response = OverlayResponse::CreateWithInfo<HTTPAuthOverlayResponseInfo>(
        user, password);
  }
  self.request->set_response(std::move(response));
}

@end
