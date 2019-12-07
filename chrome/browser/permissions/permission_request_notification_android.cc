// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_request_notification_android.h"

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/permissions/permission_request_notification_handler.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/native_theme/native_theme.h"

namespace {
const gfx::Image GetNotificationsSmallImage() {
  return gfx::Image(
      CreateVectorIcon(vector_icons::kNotificationsOffIcon,
                       message_center::kNotificationIconSize,
                       ui::NativeTheme::GetInstanceForWeb()->GetSystemColor(
                           ui::NativeTheme::kColorId_DefaultIconColor)));
}

constexpr char kNotificationIdPrefix[] = "notification_permission_request_";
}  // namespace

PermissionRequestNotificationAndroid::~PermissionRequestNotificationAndroid() {
  permission_request_notification_handler_->RemoveNotificationDelegate(
      notification_->id());

  notification_display_service_->Close(
      NotificationHandler::Type::PERMISSION_REQUEST, notification_->id());
}

// static
std::unique_ptr<PermissionRequestNotificationAndroid>
PermissionRequestNotificationAndroid::Create(
    content::WebContents* web_contents,
    PermissionPrompt::Delegate* delegate) {
  return base::WrapUnique(
      new PermissionRequestNotificationAndroid(web_contents, delegate));
}

// static
bool PermissionRequestNotificationAndroid::ShouldShowAsNotification(
    content::WebContents* web_contents,
    ContentSettingsType type) {
  return false;
}

// static
std::string PermissionRequestNotificationAndroid::NotificationIdForOrigin(
    const std::string& origin) {
  return kNotificationIdPrefix + origin;
}

// static
PermissionPrompt::TabSwitchingBehavior
PermissionRequestNotificationAndroid::GetTabSwitchingBehavior() {
    return PermissionPrompt::TabSwitchingBehavior::
        kDestroyPromptButKeepRequestPending;
}

void PermissionRequestNotificationAndroid::Close() {
  delegate_->Closing();
}

void PermissionRequestNotificationAndroid::Click(int button_index) {
  switch (button_index) {
    // "Show for site" button.
    case 0:
      delegate_->Accept();
      break;
    default:
      delegate_->Closing();
  }
}

PermissionRequestNotificationAndroid::PermissionRequestNotificationAndroid(
    content::WebContents* web_contents,
    PermissionPrompt::Delegate* delegate)
    : delegate_(delegate),
      notification_display_service_(
          NotificationDisplayServiceImpl::GetForProfile(
              Profile::FromBrowserContext(web_contents->GetBrowserContext()))),
      permission_request_notification_handler_(
          static_cast<PermissionRequestNotificationHandler*>(
              notification_display_service_->GetNotificationHandler(
                  NotificationHandler::Type::PERMISSION_REQUEST))) {
  DCHECK(delegate_);
  DCHECK(notification_display_service_);
  DCHECK(permission_request_notification_handler_);

  message_center::RichNotificationData data;
  // TODO(andypaicu): Refactor this to make it content settings type agnostic.
  data.buttons.push_back(message_center::ButtonInfo(l10n_util::GetStringUTF16(
      IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_ALLOW_BUTTON)));
  data.small_image = GetNotificationsSmallImage();

  CHECK(!delegate_->Requests().empty());
  const PermissionRequest* permission_request = delegate_->Requests()[0];

  notification_ = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      NotificationIdForOrigin(permission_request->GetOrigin().spec()),
      permission_request->GetQuietTitleText(),
      permission_request->GetQuietMessageText(), data.small_image,
      base::UTF8ToUTF16(permission_request->GetOrigin().host()),
      permission_request->GetOrigin(),
      message_center::NotifierId(permission_request->GetOrigin()), data,
      nullptr);
  notification_->set_silent(true);

  permission_request_notification_handler_->AddNotificationDelegate(
      notification_->id(), this);

  notification_display_service_->Display(
      NotificationHandler::Type::PERMISSION_REQUEST, *notification_,
      nullptr /* metadata */);
}
