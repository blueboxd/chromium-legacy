// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/common/alerts/alert_overlay_mediator.h"

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/overlays/public/common/alerts/alert_overlay.h"
#import "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#import "ios/chrome/browser/ui/alert_view/alert_action.h"
#import "ios/chrome/browser/ui/alert_view/alert_consumer.h"
#import "ios/chrome/browser/ui/overlays/common/alerts/alert_overlay_mediator+alert_consumer_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using alert_overlays::AlertRequest;
using alert_overlays::AlertResponse;

@interface AlertOverlayMediator ()
// The AlertRequest config used to construct the mediator.  Returns null if
// the mediator was not constructed with an AlertRequest config.
// TODO(crbug.com/1071543): Once all alerts are managed using AlertRequest
// configs, this can be updated to always return a non-null value.
@property(nonatomic, readonly) AlertRequest* alertConfig;
// Returns an array containing the current values of all alert text fields.
@property(nonatomic, readonly) NSArray<NSString*>* textFieldValues;
@end

@implementation AlertOverlayMediator

#pragma mark - Accessors

- (void)setConsumer:(id<AlertConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;
  NSString* alertTitle = self.alertTitle;
  [_consumer setTitle:alertTitle];
  NSString* alertMessage = self.alertMessage;
  [_consumer setMessage:alertMessage];
  [_consumer setTextFieldConfigurations:self.alertTextFieldConfigurations];
  NSArray<AlertAction*>* alertActions = self.alertActions;
  [_consumer setActions:alertActions];
  [_consumer setAlertAccessibilityIdentifier:self.alertAccessibilityIdentifier];
  DCHECK_GT(alertTitle.length + alertMessage.length, 0U);
  DCHECK_GT(alertActions.count, 0U);
}

- (AlertRequest*)alertConfig {
  return self.request ? self.request->GetConfig<AlertRequest>() : nullptr;
}

- (NSArray<NSString*>*)textFieldValues {
  AlertRequest* config = self.alertConfig;
  if (!config || !config->text_field_configs().count)
    return nil;

  NSMutableArray<NSString*>* textFieldValues =
      [NSMutableArray<NSString*> array];
  for (size_t i = 0; i < config->text_field_configs().count; ++i) {
    [textFieldValues addObject:[self.dataSource textFieldInputForMediator:self
                                                           textFieldIndex:i]];
  }
  return textFieldValues;
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return AlertRequest::RequestSupport();
}

#pragma mark - Private

// Sets a completion OverlayResponse after the button at |tappedButtonIndex|
// was tapped.
- (void)setCompletionResponse:(size_t)tappedButtonIndex {
  AlertRequest* config = self.alertConfig;
  if (!config)
    return;
  std::unique_ptr<OverlayResponse> alertResponse =
      OverlayResponse::CreateWithInfo<AlertResponse>(tappedButtonIndex,
                                                     self.textFieldValues);
  self.request->GetCallbackManager()->SetCompletionResponse(
      config->response_converter().Run(std::move(alertResponse)));
}

// Returns the action block for the button at |index|.
- (void (^)(AlertAction* action))actionForButtonAtIndex:(size_t)index {
  __weak __typeof__(self) weakSelf = self;
  base::StringPiece actionName =
      self.alertConfig->button_configs()[index].user_action_name;
  return ^(AlertAction*) {
    if (!actionName.empty()) {
      base::RecordComputedAction(actionName.data());
    }
    __typeof__(self) strongSelf = weakSelf;
    [strongSelf setCompletionResponse:index];
    [strongSelf.delegate stopOverlayForMediator:strongSelf];
  };
}

@end

@implementation AlertOverlayMediator (AlertConsumerSupport)

- (NSString*)alertTitle {
  return self.alertConfig ? self.alertConfig->title() : nil;
}

- (NSString*)alertMessage {
  return self.alertConfig ? self.alertConfig->message() : nil;
}

- (NSArray<TextFieldConfiguration*>*)alertTextFieldConfigurations {
  return self.alertConfig ? self.alertConfig->text_field_configs() : nil;
}

- (NSArray<AlertAction*>*)alertActions {
  AlertRequest* config = self.alertConfig;
  if (!config || config->button_configs().empty())
    return nil;
  NSMutableArray<AlertAction*>* actions = [NSMutableArray<AlertAction*> array];
  const std::vector<alert_overlays::ButtonConfig>& button_configs =
      config->button_configs();
  for (size_t i = 0; i < button_configs.size(); ++i) {
    [actions addObject:[AlertAction
                           actionWithTitle:button_configs[i].title
                                     style:button_configs[i].style
                                   handler:[self actionForButtonAtIndex:i]]];
  }
  return actions;
}

- (NSString*)alertAccessibilityIdentifier {
  return self.alertConfig ? self.alertConfig->accessibility_identifier() : nil;
}

@end
