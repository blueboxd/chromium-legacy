// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_prompt_overlay_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_prompt_overlay.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_action.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_view_controller.h"
#import "ios/chrome/browser/ui/elements/text_field_configuration.h"
#import "ios/chrome/browser/ui/overlays/overlay_ui_dismissal_delegate.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_dialog_blocking_action.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_dialog_overlay_coordinator+subclassing.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kJavaScriptPromptTextFieldAccessibiltyIdentifier =
    @"JavaScriptPromptTextFieldAccessibiltyIdentifier";

@interface JavaScriptPromptOverlayMediator ()
// The confirmation config.
@property(nonatomic, readonly) JavaScriptPromptOverlayRequestConfig* config;
@end

@implementation JavaScriptPromptOverlayMediator

#pragma mark - Accessors

- (const JavaScriptDialogSource*)requestSource {
  return &self.config->source();
}

- (JavaScriptPromptOverlayRequestConfig*)config {
  return self.request->GetConfig<JavaScriptPromptOverlayRequestConfig>();
}

- (void)setConsumer:(id<AlertConsumer>)consumer {
  if (self.consumer == consumer)
    return;
  [super setConsumer:consumer];
  [self.consumer setMessage:base::SysUTF8ToNSString(self.config->message())];
  __weak __typeof__(self) weakSelf = self;
  NSArray* actions = @[
    [AlertAction actionWithTitle:l10n_util::GetNSString(IDS_OK)
                           style:UIAlertActionStyleDefault
                         handler:^(AlertAction* action) {
                           __typeof__(self) strongSelf = weakSelf;
                           NSString* input = [strongSelf.dataSource
                               promptInputForMediator:strongSelf];
                           [strongSelf setPromptResponse:input ? input : @""];
                           [strongSelf.delegate
                               stopDialogForMediator:strongSelf];
                         }],
    [AlertAction actionWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                           style:UIAlertActionStyleCancel
                         handler:^(AlertAction* action) {
                           [weakSelf.delegate stopDialogForMediator:weakSelf];
                         }],
  ];
  AlertAction* blockingAction = GetBlockingAlertAction(self);
  if (blockingAction)
    actions = [actions arrayByAddingObject:blockingAction];
  [self.consumer setActions:actions];

  NSString* defaultPromptValue =
      base::SysUTF8ToNSString(self.config->default_prompt_value());
  [self.consumer setTextFieldConfigurations:@[
    [[TextFieldConfiguration alloc]
                   initWithText:defaultPromptValue
                    placeholder:nil
        accessibilityIdentifier:kJavaScriptPromptTextFieldAccessibiltyIdentifier
                secureTextEntry:NO]
  ]];
}

#pragma mark - Response helpers

// Sets the OverlayResponse using the user input from the prompt UI.
- (void)setPromptResponse:(NSString*)textInput {
  self.request->set_response(
      OverlayResponse::CreateWithInfo<JavaScriptPromptOverlayResponseInfo>(
          base::SysNSStringToUTF8(textInput)));
}

@end
