// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/notifications/stub_alert_dispatcher_mac.h"

#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"

@implementation StubAlertDispatcher {
  base::scoped_nsobject<NSMutableArray> _alerts;
}

- (instancetype)init {
  if ((self = [super init])) {
    _alerts.reset([[NSMutableArray alloc] init]);
  }
  return self;
}

- (void)dispatchNotification:(NSDictionary*)data {
  [_alerts addObject:data];
}

- (void)closeNotificationWithId:(NSString*)notificationId
                  withProfileId:(NSString*)profileId {
  DCHECK(profileId);
  DCHECK(notificationId);
  for (NSDictionary* toast in _alerts.get()) {
    NSString* toastId =
        [toast objectForKey:notification_constants::kNotificationId];
    NSString* persistentProfileId =
        [toast objectForKey:notification_constants::kNotificationProfileId];
    if ([toastId isEqualToString:notificationId] &&
        [persistentProfileId isEqualToString:profileId]) {
      [_alerts removeObject:toast];
      break;
    }
  }
}

- (void)closeAllNotifications {
  [_alerts removeAllObjects];
}

- (void)
getDisplayedAlertsForProfileId:(NSString*)profileId
                     incognito:(BOOL)incognito
            notificationCenter:(NSUserNotificationCenter*)notificationCenter
                      callback:(GetDisplayedNotificationsCallback)callback {
  std::set<std::string> displayedNotifications;
  for (NSUserNotification* toast in
       [notificationCenter deliveredNotifications]) {
    NSString* toastProfileId = [toast.userInfo
        objectForKey:notification_constants::kNotificationProfileId];
    if ([toastProfileId isEqualToString:profileId]) {
      displayedNotifications.insert(base::SysNSStringToUTF8([toast.userInfo
          objectForKey:notification_constants::kNotificationId]));
    }
  }

  std::move(callback).Run(std::move(displayedNotifications),
                          true /* supports_synchronization */);
}

- (NSArray*)alerts {
  return [[_alerts copy] autorelease];
}

@end
