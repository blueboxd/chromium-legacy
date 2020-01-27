// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/announcement_notification/announcement_notification_handler.h"

#include "base/metrics/field_trial_params.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/updates/announcement_notification/announcement_notification_delegate.h"
#include "chrome/browser/updates/announcement_notification/announcement_notification_metrics.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const int kReviewButtonIndex = 1;

}  // namespace

AnnouncementNotificationHandler::AnnouncementNotificationHandler() = default;

AnnouncementNotificationHandler::~AnnouncementNotificationHandler() = default;

void AnnouncementNotificationHandler::OnShow(
    Profile* profile,
    const std::string& notification_id) {
  RecordAnnouncementHistogram(AnnouncementNotificationEvent::kShown);
}

void AnnouncementNotificationHandler::OnClose(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    bool by_user,
    base::OnceClosure completed_closure) {
  RecordAnnouncementHistogram(AnnouncementNotificationEvent::kClose);
  std::move(completed_closure).Run();
}

void AnnouncementNotificationHandler::OnClick(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply,
    base::OnceClosure completed_closure) {
  int button_index = action_index.has_value() ? action_index.value() : -1;

  // Open the announcement link when the user clicks the notification or clicks
  // the button to open.
  if (button_index == kReviewButtonIndex || !action_index.has_value()) {
    RecordAnnouncementHistogram(action_index.has_value()
                                    ? AnnouncementNotificationEvent::kOpen
                                    : AnnouncementNotificationEvent::kClick);
    OpenAnnouncement(profile);
    std::move(completed_closure).Run();
    return;
  }

  // Otherwise, close the notification.
  RecordAnnouncementHistogram(AnnouncementNotificationEvent::kAck);
  NotificationDisplayServiceFactory::GetInstance()
      ->GetForProfile(profile)
      ->Close(NotificationHandler::Type::ANNOUNCEMENT,
              kAnnouncementNotificationId);
  std::move(completed_closure).Run();
}

void AnnouncementNotificationHandler::OpenAnnouncement(Profile* profile) {
  // Open the announcement URL in a new tab.
  // Fallback to default URL if |remote_url| is empty.
  // TODO(xingliu): Pass |remote_url| from AnnouncementNotificationService.
  std::string remote_url = base::GetFieldTrialParamValueByFeature(
      kAnnouncementNotification, kAnnouncementUrl);
  std::string url = remote_url.empty()
                        ? l10n_util::GetStringUTF8(IDS_TOS_NOTIFICATION_LINK)
                        : remote_url;
  chrome::ScopedTabbedBrowserDisplayer browser_displayer(profile);
  content::OpenURLParams params(GURL(url), content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PageTransition::PAGE_TRANSITION_FIRST,
                                false /*is_renderer_initiated*/);
  browser_displayer.browser()->OpenURL(params);
}
