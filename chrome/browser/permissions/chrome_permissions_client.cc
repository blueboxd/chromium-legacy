// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/chrome_permissions_client.h"

#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/metrics/ukm_background_recorder_service.h"
#include "chrome/browser/permissions/adaptive_quiet_notification_permission_ui_enabler.h"
#include "chrome/browser/permissions/contextual_notification_permission_ui_selector.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "components/ukm/content/source_url_recorder.h"
#include "extensions/common/constants.h"
#include "url/origin.h"

#if !defined(OS_ANDROID)
#include "chrome/app/vector_icons/vector_icons.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_data.h"
#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif

// static
ChromePermissionsClient* ChromePermissionsClient::GetInstance() {
  static base::NoDestructor<ChromePermissionsClient> instance;
  return instance.get();
}

HostContentSettingsMap* ChromePermissionsClient::GetSettingsMap(
    content::BrowserContext* browser_context) {
  return HostContentSettingsMapFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
}

permissions::PermissionDecisionAutoBlocker*
ChromePermissionsClient::GetPermissionDecisionAutoBlocker(
    content::BrowserContext* browser_context) {
  return PermissionDecisionAutoBlockerFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
}

double ChromePermissionsClient::GetSiteEngagementScore(
    content::BrowserContext* browser_context,
    const GURL& origin) {
  return SiteEngagementService::Get(
             Profile::FromBrowserContext(browser_context))
      ->GetScore(origin);
}

void ChromePermissionsClient::GetUkmSourceId(
    content::BrowserContext* browser_context,
    const content::WebContents* web_contents,
    const GURL& requesting_origin,
    GetUkmSourceIdCallback callback) {
  if (web_contents) {
    ukm::SourceId source_id =
        ukm::GetSourceIdForWebContentsDocument(web_contents);
    std::move(callback).Run(source_id);
  } else {
    // We only record a permission change if the origin is in the user's
    // history.
    ukm::UkmBackgroundRecorderFactory::GetForProfile(
        Profile::FromBrowserContext(browser_context))
        ->GetBackgroundSourceIdIfAllowed(url::Origin::Create(requesting_origin),
                                         std::move(callback));
  }
}

permissions::PermissionRequest::IconId
ChromePermissionsClient::GetOverrideIconId(ContentSettingsType type) {
#if defined(OS_CHROMEOS)
  // TODO(xhwang): fix this icon, see crbug.com/446263.
  if (type == ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER)
    return kProductIcon;
#endif
  return PermissionsClient::GetOverrideIconId(type);
}

std::unique_ptr<permissions::NotificationPermissionUiSelector>
ChromePermissionsClient::CreateNotificationPermissionUiSelector(
    content::BrowserContext* browser_context) {
  return std::make_unique<ContextualNotificationPermissionUiSelector>(
      Profile::FromBrowserContext(browser_context));
}

void ChromePermissionsClient::OnPromptResolved(
    content::BrowserContext* browser_context,
    permissions::PermissionRequestType request_type,
    permissions::PermissionAction action) {
  if (request_type ==
      permissions::PermissionRequestType::PERMISSION_NOTIFICATIONS) {
    AdaptiveQuietNotificationPermissionUiEnabler::GetForProfile(
        Profile::FromBrowserContext(browser_context))
        ->RecordPermissionPromptOutcome(action);
  }
}

base::Optional<url::Origin> ChromePermissionsClient::GetAutoApprovalOrigin() {
#if defined(OS_CHROMEOS)
  // In web kiosk mode, all permission requests are auto-approved for the origin
  // of the main app.
  if (user_manager::UserManager::IsInitialized() &&
      user_manager::UserManager::Get()->IsLoggedInAsWebKioskApp()) {
    const AccountId& account_id =
        user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId();
    DCHECK(chromeos::WebKioskAppManager::IsInitialized());
    const chromeos::WebKioskAppData* app_data =
        chromeos::WebKioskAppManager::Get()->GetAppByAccountId(account_id);
    DCHECK(app_data);
    return url::Origin::Create(app_data->install_url());
  }
#endif
  return base::nullopt;
}

bool ChromePermissionsClient::CanBypassEmbeddingOriginCheck(
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  // The New Tab Page is excluded from origin checks as its effective requesting
  // origin may be the Default Search Engine origin. Extensions are also
  // excluded as currently they can request permission from iframes when
  // embedded in non-secure contexts (https://crbug.com/530507).
  return embedding_origin == GURL(chrome::kChromeUINewTabURL).GetOrigin() ||
         requesting_origin.SchemeIs(extensions::kExtensionScheme);
}
