// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/notifications/notification_builder_mac.h"

#import <AppKit/AppKit.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"

#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"

namespace {

// Internal builder constants representing the different notification fields
// They don't need to be exposed outside the builder.

NSString* const kNotificationTitle = @"title";
NSString* const kNotificationSubTitle = @"subtitle";
NSString* const kNotificationInformativeText = @"informativeText";
NSString* const kNotificationImage = @"icon";
NSString* const kNotificationButtonOne = @"buttonOne";
NSString* const kNotificationButtonTwo = @"buttonTwo";
NSString* const kNotificationTag = @"tag";
NSString* const kNotificationCloseButtonTag = @"closeButton";
NSString* const kNotificationOptionsButtonTag = @"optionsButton";
NSString* const kNotificationSettingsButtonTag = @"settingsButton";

}  // namespace

@implementation NotificationBuilder {
  base::scoped_nsobject<NSMutableDictionary> _notificationData;
}

- (instancetype)initWithCloseLabel:(NSString*)closeLabel
                      optionsLabel:(NSString*)optionsLabel
                     settingsLabel:(NSString*)settingsLabel {
  if ((self = [super init])) {
    _notificationData.reset([[NSMutableDictionary alloc] init]);
    [_notificationData setObject:closeLabel forKey:kNotificationCloseButtonTag];
    [_notificationData setObject:optionsLabel
                          forKey:kNotificationOptionsButtonTag];
    [_notificationData setObject:settingsLabel
                          forKey:kNotificationSettingsButtonTag];
  }
  return self;
}

- (instancetype)initWithDictionary:(NSDictionary*)data {
  if ((self = [super init])) {
    _notificationData.reset([data copy]);
  }
  return self;
}

- (void)setTitle:(NSString*)title {
  if (title.length)
    [_notificationData setObject:title forKey:kNotificationTitle];
}

- (void)setSubTitle:(NSString*)subTitle {
  if (subTitle.length)
    [_notificationData setObject:subTitle forKey:kNotificationSubTitle];
}

- (void)setContextMessage:(NSString*)contextMessage {
  if (contextMessage.length)
    [_notificationData setObject:contextMessage
                          forKey:kNotificationInformativeText];
}

- (void)setIcon:(NSImage*)icon {
  if (icon) {
    if ([icon conformsToProtocol:@protocol(NSSecureCoding)]) {
      [_notificationData setObject:icon forKey:kNotificationImage];
    } else {  // NSImage only conforms to NSSecureCoding from 10.10 onwards.
      [_notificationData setObject:[icon TIFFRepresentation]
                            forKey:kNotificationImage];
    }
  }
}

- (void)setButtons:(NSString*)primaryButton
    secondaryButton:(NSString*)secondaryButton {
  DCHECK(primaryButton.length);
  [_notificationData setObject:primaryButton forKey:kNotificationButtonOne];
  if (secondaryButton.length) {
    [_notificationData setObject:secondaryButton forKey:kNotificationButtonTwo];
  }
}

- (void)setTag:(NSString*)tag {
  if (tag.length)
    [_notificationData setObject:tag forKey:kNotificationTag];
}

- (void)setOrigin:(NSString*)origin {
  if (origin.length)
    [_notificationData setObject:origin
                          forKey:notification_constants::kNotificationOrigin];
}

- (void)setNotificationId:(NSString*)notificationId {
  DCHECK(notificationId.length);
  [_notificationData setObject:notificationId
                        forKey:notification_constants::kNotificationId];
}

- (void)setProfileId:(NSString*)profileId {
  DCHECK(profileId.length);
  [_notificationData setObject:profileId
                        forKey:notification_constants::kNotificationProfileId];
}

- (void)setIncognito:(BOOL)incognito {
  [_notificationData setObject:[NSNumber numberWithBool:incognito]
                        forKey:notification_constants::kNotificationIncognito];
}

- (void)setNotificationType:(NSNumber*)notificationType {
  [_notificationData setObject:notificationType
                        forKey:notification_constants::kNotificationType];
}

- (void)setShowSettingsButton:(BOOL)showSettingsButton {
  [_notificationData
      setObject:[NSNumber numberWithBool:showSettingsButton]
         forKey:notification_constants::kNotificationHasSettingsButton];
}

- (NSUserNotification*)buildUserNotification {
  base::scoped_nsobject<NSUserNotification> toast(
      [[NSUserNotification alloc] init]);
  [toast setTitle:[_notificationData objectForKey:kNotificationTitle]];
  [toast setSubtitle:[_notificationData objectForKey:kNotificationSubTitle]];
  [toast setInformativeText:[_notificationData
                                objectForKey:kNotificationInformativeText]];

  // Icon
  if ([_notificationData objectForKey:kNotificationImage]) {
    if ([[NSImage class] conformsToProtocol:@protocol(NSSecureCoding)]) {
      NSImage* image = [_notificationData objectForKey:kNotificationImage];
      [toast setContentImage:image];
    } else {  // NSImage only conforms to NSSecureCoding from 10.10 onwards.
      base::scoped_nsobject<NSImage> image([[NSImage alloc]
          initWithData:[_notificationData objectForKey:kNotificationImage]]);
      [toast setContentImage:image];
    }
  }

  // Type (needed to define the buttons)
  NSNumber* type = [_notificationData
      objectForKey:notification_constants::kNotificationType];

  // Extensions don't have a settings button.
  NSNumber* showSettingsButton = [_notificationData
      objectForKey:notification_constants::kNotificationHasSettingsButton];

  // Buttons
  if ([toast respondsToSelector:@selector(_showsButtons)]) {
    DCHECK([_notificationData objectForKey:kNotificationCloseButtonTag]);
    DCHECK([_notificationData objectForKey:kNotificationSettingsButtonTag]);
    DCHECK([_notificationData objectForKey:kNotificationOptionsButtonTag]);
    DCHECK([_notificationData
        objectForKey:notification_constants::kNotificationHasSettingsButton]);

    BOOL settingsButton = [showSettingsButton boolValue];

    [toast setValue:@YES forKey:@"_showsButtons"];
    // A default close button label is provided by the platform but we
    // explicitly override it in case the user decides to not
    // use the OS language in Chrome.
    [toast setOtherButtonTitle:[_notificationData
                                   objectForKey:kNotificationCloseButtonTag]];

    // Display the Settings button as the action button if there are either no
    // developer-provided action buttons, or the alternate action menu is not
    // available on this Mac version. This avoids needlessly showing the menu.
    if (![_notificationData objectForKey:kNotificationButtonOne] ||
        ![toast respondsToSelector:@selector(_alwaysShowAlternateActionMenu)]) {
      if (settingsButton) {
        [toast setActionButtonTitle:
                   [_notificationData
                       objectForKey:kNotificationSettingsButtonTag]];
      } else {
        [toast setHasActionButton:NO];
      }

    } else {
      // Otherwise show the alternate menu, then show the developer actions and
      // finally the settings one if needed.
      DCHECK(
          [toast respondsToSelector:@selector(_alwaysShowAlternateActionMenu)]);
      DCHECK(
          [toast respondsToSelector:@selector(_alternateActionButtonTitles)]);
      [toast
          setActionButtonTitle:[_notificationData
                                   objectForKey:kNotificationOptionsButtonTag]];
      [toast setValue:@YES forKey:@"_alwaysShowAlternateActionMenu"];

      NSMutableArray* buttons = [NSMutableArray arrayWithCapacity:3];
      [buttons
          addObject:[_notificationData objectForKey:kNotificationButtonOne]];
      if ([_notificationData objectForKey:kNotificationButtonTwo]) {
        [buttons
            addObject:[_notificationData objectForKey:kNotificationButtonTwo]];
      }
      if (settingsButton) {
        [buttons addObject:[_notificationData
                               objectForKey:kNotificationSettingsButtonTag]];
      }

      [toast setValue:buttons forKey:@"_alternateActionButtonTitles"];
    }
  }

  // Tag
  if ([toast respondsToSelector:@selector(setIdentifier:)] &&
      [_notificationData objectForKey:kNotificationTag]) {
    [toast setValue:[_notificationData objectForKey:kNotificationTag]
             forKey:@"identifier"];
  }

  NSString* origin =
      [_notificationData
          objectForKey:notification_constants::kNotificationOrigin]
          ? [_notificationData
                objectForKey:notification_constants::kNotificationOrigin]
          : @"";
  DCHECK(
      [_notificationData objectForKey:notification_constants::kNotificationId]);
  NSString* notificationId =
      [_notificationData objectForKey:notification_constants::kNotificationId];

  DCHECK([_notificationData
      objectForKey:notification_constants::kNotificationProfileId]);
  NSString* profileId = [_notificationData
      objectForKey:notification_constants::kNotificationProfileId];

  DCHECK([_notificationData
      objectForKey:notification_constants::kNotificationIncognito]);
  NSNumber* incognito = [_notificationData
      objectForKey:notification_constants::kNotificationIncognito];

  toast.get().userInfo = @{
    notification_constants::kNotificationOrigin : origin,
    notification_constants::kNotificationId : notificationId,
    notification_constants::kNotificationProfileId : profileId,
    notification_constants::kNotificationIncognito : incognito,
    notification_constants::kNotificationType : type,
    notification_constants::kNotificationHasSettingsButton : showSettingsButton,
  };

  return toast.autorelease();
}

- (NSDictionary*)buildDictionary {
  return [[_notificationData copy] autorelease];
}

@end
