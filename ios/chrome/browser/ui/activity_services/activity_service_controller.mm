// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activity_service_controller.h"

#import <MobileCoreServices/MobileCoreServices.h>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/prefs/pref_service.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "ios/chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/ui/activity_services/activities/bookmark_activity.h"
#import "ios/chrome/browser/ui/activity_services/activities/copy_activity.h"
#import "ios/chrome/browser/ui/activity_services/activities/find_in_page_activity.h"
#import "ios/chrome/browser/ui/activity_services/activities/generate_qr_code_activity.h"
#import "ios/chrome/browser/ui/activity_services/activities/print_activity.h"
#import "ios/chrome/browser/ui/activity_services/activities/reading_list_activity.h"
#import "ios/chrome/browser/ui/activity_services/activities/request_desktop_or_mobile_site_activity.h"
#import "ios/chrome/browser/ui/activity_services/activities/send_tab_to_self_activity.h"
#import "ios/chrome/browser/ui/activity_services/activity_type_util.h"
#import "ios/chrome/browser/ui/activity_services/chrome_activity_item_source.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_positioner.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_presentation.h"
#import "ios/chrome/browser/ui/activity_services/share_to_data.h"
#import "ios/chrome/browser/ui/commands/qr_generation_commands.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Snackbar category for activity services.
NSString* const kActivityServicesSnackbarCategory =
    @"ActivityServicesSnackbarCategory";

@interface ActivityServiceController () {
  __weak id<ActivityServicePresentation> _presentationProvider;
  UIActivityViewController* _activityViewController;
  __weak id<SnackbarCommands> _dispatcher;
}

// Resets the controller's user interface and delegate.
- (void)resetUserInterface;
// Called when UIActivityViewController user interface is dismissed by user
// signifying the end of the Share/Action activity.
- (void)shareFinishedWithActivityType:(NSString*)activityType
                            completed:(BOOL)completed
                        returnedItems:(NSArray*)returnedItems
                                error:(NSError*)activityError;
// Returns an array of UIActivityItemSource objects to provide the |data| to
// share to the sharing activities.
- (NSArray*)activityItemsForData:(ShareToData*)data;
// Returns an array of UIActivity objects that can handle the given |data|.
- (NSArray*)
    applicationActivitiesForData:(ShareToData*)data
                  commandHandler:(id<BrowserCommands,
                                     FindInPageCommands,
                                     QRGenerationCommands>)commandHandler
                   bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                     prefService:(PrefService*)prefService
                canSendTabToSelf:(BOOL)canSendTabToSelf;
@end

@implementation ActivityServiceController

#pragma mark - Public

+ (ActivityServiceController*)sharedInstance {
  static ActivityServiceController* instance =
      [[ActivityServiceController alloc] init];
  return instance;
}

- (BOOL)isActive {
  return [_activityViewController isBeingPresented];
}

- (void)cancelShareAnimated:(BOOL)animated {
  if (![self isActive]) {
    return;
  }
  DCHECK(_activityViewController);
  // There is no guarantee that the completion callback will be called because
  // the |activityViewController_| may have been dismissed already. For example,
  // if the user selects Facebook Share Extension, the UIActivityViewController
  // is first dismissed and then the UI for Facebook Share Extension comes up.
  // At this time, if the user backgrounds Chrome and then relaunch Chrome
  // through an external app (e.g. with googlechrome://url.com), Chrome restart
  // dismisses the modal UI coming through this path. But since the
  // UIActivityViewController has already been dismissed, the following method
  // does nothing and completion callback is not called. The call
  // -shareFinishedWithActivityType:completed:returnedItems:error: must be
  // called explicitly to do the clean up or else future attempts to use
  // Share will fail.
  [_activityViewController dismissViewControllerAnimated:animated
                                              completion:nil];
  [self shareFinishedWithActivityType:nil
                            completed:NO
                        returnedItems:nil
                                error:nil];
}

- (void)shareWithData:(ShareToData*)data
            browserState:(ChromeBrowserState*)browserState
              dispatcher:(id<BrowserCommands,
                             FindInPageCommands,
                             QRGenerationCommands,
                             SnackbarCommands>)dispatcher
        positionProvider:(id<ActivityServicePositioner>)positionProvider
    presentationProvider:(id<ActivityServicePresentation>)presentationProvider {
  DCHECK(data);
  DCHECK(![self isActive]);

  CGRect fromRect = CGRectZero;
  UIView* inView = nil;
  if (IsIPadIdiom() && !IsCompactWidth()) {
    DCHECK(positionProvider);
    inView = [positionProvider shareButtonView];
    fromRect = inView.bounds;
    DCHECK(fromRect.size.height);
    DCHECK(fromRect.size.width);
    DCHECK(inView);
  }

  DCHECK(!_presentationProvider);
  _presentationProvider = presentationProvider;

  _dispatcher = dispatcher;

  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(browserState);

  BOOL canSendTabToSelf =
      send_tab_to_self::ShouldOfferFeature(browserState, data.shareURL);

  DCHECK(!_activityViewController);
  _activityViewController = [[UIActivityViewController alloc]
      initWithActivityItems:[self activityItemsForData:data]
      applicationActivities:[self
                                applicationActivitiesForData:data
                                              commandHandler:dispatcher
                                               bookmarkModel:bookmarkModel
                                                 prefService:browserState
                                                                 ->GetPrefs()
                                            canSendTabToSelf:canSendTabToSelf]];

  // Reading List and Print activities refer to iOS' version of these.
  // Chrome-specific implementations of these two activities are provided
  // below in
  // applicationActivitiesForData:browserCommandHandler:
  // findInPageHandler:bookmarkModel:
  // The "Copy" action is also provided by chrome in order to change its icon.
  NSArray* excludedActivityTypes = @[
    UIActivityTypeAddToReadingList, UIActivityTypeCopyToPasteboard,
    UIActivityTypePrint, UIActivityTypeSaveToCameraRoll
  ];
  [_activityViewController setExcludedActivityTypes:excludedActivityTypes];

  __weak ActivityServiceController* weakSelf = self;
  [_activityViewController setCompletionWithItemsHandler:^(
                               NSString* activityType, BOOL completed,
                               NSArray* returnedItems, NSError* activityError) {
    [weakSelf shareFinishedWithActivityType:activityType
                                  completed:completed
                              returnedItems:returnedItems
                                      error:activityError];
  }];

  _activityViewController.modalPresentationStyle = UIModalPresentationPopover;
  _activityViewController.popoverPresentationController.sourceView = inView;
  _activityViewController.popoverPresentationController.sourceRect = fromRect;
  [_presentationProvider
      presentActivityServiceViewController:_activityViewController];
}

#pragma mark - Private

- (void)resetUserInterface {
  _presentationProvider = nil;
  _activityViewController = nil;
}

- (void)shareFinishedWithActivityType:(NSString*)activityType
                            completed:(BOOL)completed
                        returnedItems:(NSArray*)returnedItems
                                error:(NSError*)activityError {
  DCHECK(_presentationProvider);

  if (activityType) {
    activity_type_util::ActivityType type =
        activity_type_util::TypeFromString(activityType);
    activity_type_util::RecordMetricForActivity(type);
    NSString* completionMessage =
        activity_type_util::CompletionMessageForActivity(type);
    [self shareDidComplete:completed completionMessage:completionMessage];
  } else {
    [self shareDidComplete:NO completionMessage:nil];
  }

  [self resetUserInterface];
}

- (NSArray*)activityItemsForData:(ShareToData*)data {
  // The provider object UIActivityURLSource supports the public.url UTType for
  // Share Extensions (e.g. Facebook, Twitter).
  UIActivityURLSource* urlActivitySource =
      [[UIActivityURLSource alloc] initWithShareURL:data.shareNSURL
                                            subject:data.title
                                 thumbnailGenerator:data.thumbnailGenerator];
  return @[ urlActivitySource ];
}

- (NSString*)sendTabToSelfContextMenuTitleForDevice:(NSString*)device_name
                                daysSinceLastUpdate:(int)days {
  NSString* active_time = @"";
  if (days == 0) {
    active_time = l10n_util::GetNSString(
        IDS_IOS_SEND_TAB_TO_SELF_TARGET_DEVICE_ITEM_SUBTITLE_TODAY);
  } else if (days == 1) {
    active_time = l10n_util::GetNSString(
        IDS_IOS_SEND_TAB_TO_SELF_TARGET_DEVICE_ITEM_SUBTITLE_DAY);
  } else {
    active_time = l10n_util::GetNSStringF(
        IDS_IOS_SEND_TAB_TO_SELF_TARGET_DEVICE_ITEM_SUBTITLE_DAYS,
        base::NumberToString16(days));
  }
  return [NSString stringWithFormat:@"%@ \u2022 %@", device_name, active_time];
}

- (NSArray*)
    applicationActivitiesForData:(ShareToData*)data
                  commandHandler:(id<BrowserCommands,
                                     FindInPageCommands,
                                     QRGenerationCommands>)commandHandler
                   bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                     prefService:(PrefService*)prefService
                canSendTabToSelf:(BOOL)canSendTabToSelf {
  NSMutableArray* applicationActivities = [NSMutableArray array];

  [applicationActivities
      addObject:[[CopyActivity alloc] initWithURL:data.shareURL]];

  if (data.shareURL.SchemeIsHTTPOrHTTPS()) {
    if (canSendTabToSelf) {
      SendTabToSelfActivity* sendTabToSelfActivity =
          [[SendTabToSelfActivity alloc] initWithDispatcher:commandHandler];
      [applicationActivities addObject:sendTabToSelfActivity];
    }

    ReadingListActivity* readingListActivity =
        [[ReadingListActivity alloc] initWithURL:data.shareURL
                                           title:data.title
                                      dispatcher:commandHandler];
    [applicationActivities addObject:readingListActivity];

    if (bookmarkModel) {
      BOOL bookmarked = bookmarkModel->loaded() &&
                        bookmarkModel->IsBookmarked(data.visibleURL);
      BookmarkActivity* bookmarkActivity =
          [[BookmarkActivity alloc] initWithURL:data.visibleURL
                                     bookmarked:bookmarked
                                     dispatcher:commandHandler
                                    prefService:prefService];
      [applicationActivities addObject:bookmarkActivity];
    }

    if (base::FeatureList::IsEnabled(kQRCodeGeneration)) {
      GenerateQrCodeActivity* generateQrCodeActivity =
          [[GenerateQrCodeActivity alloc] initWithURL:data.shareURL
                                                title:data.title
                                           dispatcher:commandHandler];
      [applicationActivities addObject:generateQrCodeActivity];
    }

    if (data.isPageSearchable) {
      FindInPageActivity* findInPageActivity =
          [[FindInPageActivity alloc] initWithHandler:commandHandler];
      [applicationActivities addObject:findInPageActivity];
    }

    if (data.userAgent != web::UserAgentType::NONE) {
      RequestDesktopOrMobileSiteActivity* requestActivity =
          [[RequestDesktopOrMobileSiteActivity alloc]
              initWithDispatcher:commandHandler
                       userAgent:data.userAgent];
      [applicationActivities addObject:requestActivity];
    }
  }
  if (data.isPagePrintable) {
    PrintActivity* printActivity = [[PrintActivity alloc] init];
    printActivity.dispatcher = commandHandler;
    [applicationActivities addObject:printActivity];
  }
  return applicationActivities;
}

- (void)shareDidComplete:(BOOL)isSuccess completionMessage:(NSString*)message {
  if (isSuccess) {
    if ([message length]) {
      TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
      [self showSnackbar:message];
    }
  } else {
    // Share action was cancelled.
    base::RecordAction(base::UserMetricsAction("MobileShareMenuCancel"));
  }

  // The shareTo dialog dismisses itself instead of through
  // |-dismissViewControllerAnimated:completion:| so we must notify the
  // presentation provider here so that it can clear its presenting state.
  [_presentationProvider activityServiceDidEndPresenting];
}

- (void)showSnackbar:(NSString*)text {
  MDCSnackbarMessage* message = [MDCSnackbarMessage messageWithText:text];
  message.accessibilityLabel = text;
  message.duration = 2.0;
  message.category = kActivityServicesSnackbarCategory;
  [_dispatcher showSnackbarMessage:message];
}

#pragma mark - For Testing

- (void)setProvidersForTesting:(id<ActivityServicePresentation>)provider
                    dispatcher:(id<SnackbarCommands>)dispatcher {
  _presentationProvider = provider;
  _dispatcher = dispatcher;
}

@end
