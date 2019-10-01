// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "remoting/ios/app/notification_presenter.h"

#import "ios/third_party/material_components_ios/src/components/Dialogs/src/MaterialDialogs.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#import "remoting/ios/facade/remoting_authentication.h"
#import "remoting/ios/facade/remoting_service.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "remoting/base/string_resources.h"
#include "remoting/client/chromoting_client_runtime.h"
#include "remoting/ios/app/view_utils.h"
#include "ui/base/l10n/l10n_util.h"

namespace remoting {

namespace {

// This is to make sure notification shows up after the user status toast (see
// UserStatusPresenter).
// TODO(yuweih): Chain this class with UserStatusPresenter on the
// kUserDidUpdate event.
constexpr base::TimeDelta kFetchNotificationDelay =
    base::TimeDelta::FromSeconds(2);

}  // namespace

// static
NotificationPresenter* NotificationPresenter::GetInstance() {
  static base::NoDestructor<NotificationPresenter> instance;
  return instance.get();
}

NotificationPresenter::NotificationPresenter()
    : notification_client_(
          ChromotingClientRuntime::GetInstance()->network_task_runner()) {}

void NotificationPresenter::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!user_update_observer_);

  if ([RemotingService.instance.authentication.user isAuthenticated]) {
    FetchNotificationIfNecessary();
  }
  user_update_observer_ = [NSNotificationCenter.defaultCenter
      addObserverForName:kUserDidUpdate
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification*) {
                // This implicitly captures |this|, but should be fine since
                // |NotificationPresenter| is singleton.
                FetchNotificationIfNecessary();
              }];
}

void NotificationPresenter::FetchNotificationIfNecessary() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ != State::NOT_FETCHED) {
    return;
  }
  UserInfo* user = RemotingService.instance.authentication.user;
  if (![user isAuthenticated]) {
    // Can't show notification since user email is unknown.
    return;
  }

  state_ = State::FETCHING;
  fetch_notification_timer_.Start(
      FROM_HERE, kFetchNotificationDelay,
      base::BindOnce(
          [](NotificationPresenter* that, const std::string& user_email) {
            that->notification_client_.GetNotification(
                user_email,
                base::BindOnce(&NotificationPresenter::OnNotificationFetched,
                               base::Unretained(that)));
          },
          base::Unretained(this), base::SysNSStringToUTF8(user.userEmail)));
}

void NotificationPresenter::OnNotificationFetched(
    base::Optional<NotificationMessage> notification) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::FETCHING, state_);

  if (!notification) {
    // No notification for this user. Need to check again when user changes.
    state_ = State::NOT_FETCHED;
    return;
  }

  state_ = State::FETCHED;

  NSURL* url =
      [NSURL URLWithString:base::SysUTF8ToNSString(notification->link_url)];
  auto open_url_block = ^{
    [[UIApplication sharedApplication] openURL:url
                                       options:@{}
                             completionHandler:nil];
  };

  NSString* message_text_nsstring =
      base::SysUTF8ToNSString(notification->message_text);
  NSString* link_text_nsstring =
      base::SysUTF8ToNSString(notification->link_text);

  switch (notification->appearance) {
    case NotificationMessage::Appearance::TOAST: {
      MDCSnackbarMessage* message = [MDCSnackbarMessage new];
      message.text = message_text_nsstring;
      MDCSnackbarMessageAction* action = [MDCSnackbarMessageAction new];
      action.handler = open_url_block;
      action.title = link_text_nsstring;
      message.action = action;
      message.duration = MDCSnackbarMessageDurationMax;
      [MDCSnackbarManager showMessage:message];
      break;
    }
    case NotificationMessage::Appearance::DIALOG: {
      // TODO(yuweih): Implement "Don't show again".
      MDCAlertController* alert_controller =
          [MDCAlertController alertControllerWithTitle:nil
                                               message:message_text_nsstring];

      MDCAlertAction* link_action =
          [MDCAlertAction actionWithTitle:link_text_nsstring
                                  handler:^(MDCAlertAction*) {
                                    open_url_block();
                                  }];
      [alert_controller addAction:link_action];

      MDCAlertAction* dismiss_action =
          [MDCAlertAction actionWithTitle:l10n_util::GetNSString(IDS_DISMISS)
                                  handler:nil];
      [alert_controller addAction:dismiss_action];

      [remoting::TopPresentingVC() presentViewController:alert_controller
                                                animated:YES
                                              completion:nil];
      break;
    }
    default:
      LOG(DFATAL) << "Unknown appearance: "
                  << static_cast<int>(notification->appearance);
      break;
  }
}

}  // namespace remoting
