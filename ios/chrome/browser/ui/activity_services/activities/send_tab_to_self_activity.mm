// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activities/send_tab_to_self_activity.h"

#include "base/ios/block_types.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_presentation.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/send_tab_to_self_command.h"
#import "ios/chrome/browser/ui/context_menu/context_menu_item.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kSendTabToSelfActivityType =
    @"com.google.chrome.sendTabToSelfActivity";

const char kClickResultHistogramName[] = "SendTabToSelf.ShareMenu.ClickResult";
const char kDeviceCountHistogramName[] = "SendTabToSelf.ShareMenu.DeviceCount";

// TODO(crbug.com/970886): Move to a directory accessible on all platforms.
// State of the send tab to self option in the context menu.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SendTabToSelfClickResult {
  kShowItem = 0,
  kClickItem = 1,
  kShowDeviceList = 2,
  kMaxValue = kShowDeviceList,
};

}  // namespace

@interface SendTabToSelfActivity ()
// The dispatcher that handles when the activity is performed.
@property(nonatomic, weak, readonly) id<BrowserCommands> dispatcher;

// The dictionary of target devices and their cache guids.
@property(nonatomic, strong, readonly)
    NSDictionary<NSString*, NSString*>* sendTabToSelfTargets;

// The presenter that will present the action sheet to show devices.
@property(nonatomic, weak, readonly) id<ActivityServicePresentation> presenter;

// The title of the shared tab.
@property(nonatomic, copy, readonly) NSString* title;
@end

@implementation SendTabToSelfActivity

+ (NSString*)activityIdentifier {
  return kSendTabToSelfActivityType;
}

- (instancetype)initWithDispatcher:(id<BrowserCommands>)dispatcher
              sendTabToSelfTargets:
                  (NSDictionary<NSString*, NSString*>*)sendTabToSelfTargets
                         presenter:(id<ActivityServicePresentation>)presenter
                             title:(NSString*)title {
  base::UmaHistogramEnumeration(kClickResultHistogramName,
                                SendTabToSelfClickResult::kShowItem);
  if (self = [super init]) {
    _sendTabToSelfTargets = sendTabToSelfTargets;
    _dispatcher = dispatcher;
    _presenter = presenter;
    _title = [title copy];
  }
  return self;
}

#pragma mark - UIActivity

- (NSString*)activityType {
  return [[self class] activityIdentifier];
}

- (NSString*)activityTitle {
  return l10n_util::GetNSString(IDS_IOS_SHARE_MENU_SEND_TAB_TO_SELF_ACTION);
}

- (UIImage*)activityImage {
  return [UIImage imageNamed:@"activity_services_send_tab_to_self"];
}

- (BOOL)canPerformWithActivityItems:(NSArray*)activityItems {
  return YES;
}

+ (UIActivityCategory)activityCategory {
  return UIActivityCategoryAction;
}

- (void)performActivity {
  // TODO(crbug.com/970284) once the modal dialog is created this will be
  // removed.
  BOOL useContextMenu = YES;
  if (useContextMenu) {
    NSMutableArray<ContextMenuItem*>* targetActions =
        [NSMutableArray arrayWithCapacity:[_sendTabToSelfTargets count]];

    for (NSString* key in _sendTabToSelfTargets) {
      NSString* deviceId = _sendTabToSelfTargets[key];
      // Retain |self| here since a |weakSelf| would be deallocated when
      // displaying the target device sheet, as the ActivitySheet will be gone.
      ProceduralBlock action = ^{
        SendTabToSelfCommand* command =
            [[SendTabToSelfCommand alloc] initWithTargetDeviceId:deviceId];
        base::UmaHistogramEnumeration(kClickResultHistogramName,
                                      SendTabToSelfClickResult::kClickItem);
        [self.dispatcher sendTabToSelf:command];
      };
      [targetActions addObject:[[ContextMenuItem alloc] initWithTitle:key
                                                               action:action]];
    }

    base::UmaHistogramEnumeration(kClickResultHistogramName,
                                  SendTabToSelfClickResult::kShowDeviceList);
    base::UmaHistogramCounts100(kDeviceCountHistogramName,
                                [targetActions count]);

    NSString* title = l10n_util::GetNSStringF(
        IDS_IOS_SHARE_MENU_SEND_TAB_TO_SELF_DEVICE_ACTION,
        base::SysNSStringToUTF16(_title));
    [_presenter showActivityServiceContextMenu:title items:targetActions];
  } else {
    [self.dispatcher showSendTabToSelfUI];
  }
  [self activityDidFinish:YES];
}

@end