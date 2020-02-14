// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/chrome_permissions_client.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/metrics/ukm_background_recorder_service.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/ukm/content/source_url_recorder.h"
#include "url/origin.h"

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
