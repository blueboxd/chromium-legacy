// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_INSTALL_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_INSTALL_UTILS_H_

#include <map>
#include <set>
#include <vector>

#include "url/gurl.h"

enum class WebappInstallSource;
struct InstallableData;
struct WebApplicationInfo;
class SkBitmap;

namespace blink {
struct Manifest;
}

namespace content {
class WebContents;
}

namespace web_app {

enum class ExternalInstallSource;
enum class InstallResultCode;
struct BitmapAndSource;

enum class ForInstallableSite {
  kYes,
  kNo,
  kUnknown,
};

// A map of icon urls to the bitmaps provided by that url.
using IconsMap = std::map<GURL, std::vector<SkBitmap>>;

// Update the given WebApplicationInfo with information from the manifest.
void UpdateWebAppInfoFromManifest(const blink::Manifest& manifest,
                                  WebApplicationInfo* web_app_info,
                                  ForInstallableSite installable_site);

// Returns icon sizes to be generated from downloaded icons.
std::set<int> SizesToGenerate();

// Form a list of icons to download: Remove icons with invalid urls. If |data|
// is specified and contains a valid primary icon, we skip downloading any
// matching icon in |web_app_info|.
std::vector<GURL> GetValidIconUrlsToDownload(
    const WebApplicationInfo& web_app_info,
    const InstallableData* data);

// Merge primary icon from installability check phase:
// Add the primary icon to the final web app creation data.
void MergeInstallableDataIcon(const InstallableData& data,
                              WebApplicationInfo* web_app_info);

// Get a list of non-empty square icons from |web_app_info|.
void FilterSquareIconsFromInfo(const WebApplicationInfo& web_app_info,
                               std::vector<BitmapAndSource>* square_icons);

// Get a list of non-empty square icons from |icons_map|.
void FilterSquareIconsFromMap(const IconsMap& icons_map,
                              std::vector<BitmapAndSource>* square_icons);

// Get a list of non-empty square icons from downloaded |icons_map| and
// |web_app_info| (merged together).
std::vector<BitmapAndSource> FilterSquareIcons(
    const IconsMap& icons_map,
    const WebApplicationInfo& web_app_info);

// Ensure that the necessary-sized icons are available by resizing larger
// icons down to smaller sizes, and generating icons for sizes where resizing
// is not possible.
void ResizeDownloadedIconsGenerateMissing(
    std::vector<BitmapAndSource> downloaded_icons,
    WebApplicationInfo* web_app_info);

// It is important that the linked app information in any web app that
// gets created from sync matches the linked app information that came from
// sync. If there are any changes, they will be synced back to other devices
// and could potentially create a never ending sync cycle.
// This function updates |web_app_info| with the image data of any icon from
// |size_map| that has a URL and size matching that in |web_app_info|, as
// well as adding any new images from |size_map| that have no URL.
void UpdateWebAppIconsWithoutChangingLinks(
    const std::map<int, BitmapAndSource>& size_map,
    WebApplicationInfo* web_app_info);

// Record an app banner added to homescreen event to ensure banners are not
// shown for this app.
void RecordAppBanner(content::WebContents* contents, const GURL& app_url);

WebappInstallSource ConvertExternalInstallSourceToInstallSource(
    ExternalInstallSource external_install_source);

void RecordExternalAppInstallResultCode(
    const char* histogram_name,
    std::map<GURL, InstallResultCode> install_results);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_INSTALL_UTILS_H_
