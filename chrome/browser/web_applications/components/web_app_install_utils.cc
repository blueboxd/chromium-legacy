// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_install_utils.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "chrome/browser/banners/app_banner_manager.h"
#include "chrome/browser/banners/app_banner_manager_desktop.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/installable/installable_data.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/common/web_application_info.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace web_app {

namespace {

void ReplaceWebAppIcons(std::map<int, BitmapAndSource> bitmap_map,
                        WebApplicationInfo* web_app_info) {
  web_app_info->icons.clear();

  // Populate the icon data into the WebApplicationInfo we are using to
  // install the bookmark app.
  for (const auto& pair : bitmap_map) {
    WebApplicationInfo::IconInfo icon_info;
    icon_info.data = pair.second.bitmap;
    icon_info.url = pair.second.source_url;
    icon_info.width = icon_info.data.width();
    icon_info.height = icon_info.data.height();
    web_app_info->icons.push_back(icon_info);
  }
}

}  // namespace

void UpdateWebAppInfoFromManifest(const blink::Manifest& manifest,
                                  WebApplicationInfo* web_app_info,
                                  ForInstallableSite for_installable_site) {
  if (!manifest.short_name.is_null())
    web_app_info->title = manifest.short_name.string();

  // Give the full length name priority.
  if (!manifest.name.is_null())
    web_app_info->title = manifest.name.string();

  // Set the url based on the manifest value, if any.
  if (manifest.start_url.is_valid())
    web_app_info->app_url = manifest.start_url;

  if (for_installable_site == ForInstallableSite::kYes)
    web_app_info->scope = manifest.scope;

  if (manifest.theme_color)
    web_app_info->theme_color = *manifest.theme_color;

  // Create the WebApplicationInfo icons list *outside* of |web_app_info|, so
  // that we can decide later whether or not to replace the existing icons array
  // (conditionally on whether there were any that didn't have purpose ANY).
  std::vector<WebApplicationInfo::IconInfo> web_app_icons;
  for (const auto& icon : manifest.icons) {
    // An icon's purpose vector should never be empty (the manifest parser
    // should have added ANY if there was no purpose specified in the manifest).
    DCHECK(!icon.purpose.empty());

    if (!base::Contains(icon.purpose,
                        blink::Manifest::ImageResource::Purpose::ANY)) {
      continue;
    }

    // TODO(benwells): Take the declared icon density and sizes into account.
    WebApplicationInfo::IconInfo info;
    info.url = icon.src;
    web_app_icons.push_back(info);
  }

  // If any icons are specified in the manifest, they take precedence over any
  // we picked up from the web_app stuff.
  if (!web_app_icons.empty())
    web_app_info->icons = std::move(web_app_icons);

  // Copy across the file handler info.
  web_app_info->file_handler = manifest.file_handler;
}

std::set<int> SizesToGenerate() {
  // Generate container icons from smaller icons.
  return std::set<int>({
      icon_size::k32, icon_size::k64, icon_size::k48, icon_size::k96,
      icon_size::k128, icon_size::k256,
  });
}

std::vector<GURL> GetValidIconUrlsToDownload(
    const WebApplicationInfo& web_app_info,
    const InstallableData* data) {
  // Add icon urls to download from the WebApplicationInfo.
  std::vector<GURL> web_app_info_icon_urls;
  for (auto& info : web_app_info.icons) {
    if (!info.url.is_valid())
      continue;

    // Skip downloading icon if we already have it from the InstallableManager.
    if (data && data->primary_icon && data->primary_icon_url == info.url)
      continue;

    web_app_info_icon_urls.push_back(info.url);
  }

  return web_app_info_icon_urls;
}

void MergeInstallableDataIcon(const InstallableData& data,
                              WebApplicationInfo* web_app_info) {
  if (data.primary_icon_url.is_valid()) {
    WebApplicationInfo::IconInfo primary_icon_info;
    const SkBitmap& icon = *data.primary_icon;
    primary_icon_info.url = data.primary_icon_url;
    primary_icon_info.data = icon;
    primary_icon_info.width = icon.width();
    primary_icon_info.height = icon.height();
    web_app_info->icons.push_back(primary_icon_info);
  }
}

void FilterSquareIconsFromInfo(const WebApplicationInfo& web_app_info,
                               std::vector<BitmapAndSource>* square_icons) {
  // Add all existing icons from WebApplicationInfo.
  for (const WebApplicationInfo::IconInfo& icon_info : web_app_info.icons) {
    const SkBitmap& icon = icon_info.data;
    if (!icon.drawsNothing() && icon.width() == icon.height())
      square_icons->push_back(BitmapAndSource(icon_info.url, icon));
  }
}

void FilterSquareIconsFromMap(const IconsMap& icons_map,
                              std::vector<BitmapAndSource>* square_icons) {
  for (const std::pair<GURL, std::vector<SkBitmap>>& url_icon : icons_map) {
    for (const SkBitmap& icon : url_icon.second) {
      if (!icon.empty() && icon.width() == icon.height())
        square_icons->push_back(BitmapAndSource(url_icon.first, icon));
    }
  }
}

std::vector<BitmapAndSource> FilterSquareIcons(
    const IconsMap& icons_map,
    const WebApplicationInfo& web_app_info) {
  std::vector<BitmapAndSource> square_icons;
  FilterSquareIconsFromMap(icons_map, &square_icons);
  FilterSquareIconsFromInfo(web_app_info, &square_icons);
  return square_icons;
}

void ResizeDownloadedIconsGenerateMissing(
    std::vector<BitmapAndSource> downloaded_icons,
    WebApplicationInfo* web_app_info) {
  web_app_info->generated_icon_color = SK_ColorTRANSPARENT;
  std::map<int, BitmapAndSource> size_to_icons = ResizeIconsAndGenerateMissing(
      downloaded_icons, SizesToGenerate(), web_app_info->app_url,
      &web_app_info->generated_icon_color);

  ReplaceWebAppIcons(size_to_icons, web_app_info);
}

void UpdateWebAppIconsWithoutChangingLinks(
    const std::map<int, BitmapAndSource>& size_map,
    WebApplicationInfo* web_app_info) {
  // First add in the icon data that have urls with the url / size data from the
  // original web app info, and the data from the new icons (if any).
  for (auto& icon : web_app_info->icons) {
    if (!icon.url.is_empty() && icon.data.empty()) {
      const auto& it = size_map.find(icon.width);
      if (it != size_map.end() && it->second.source_url == icon.url)
        icon.data = it->second.bitmap;
    }
  }

  // Now add in any icons from the updated list that don't have URLs.
  for (const auto& pair : size_map) {
    if (pair.second.source_url.is_empty()) {
      WebApplicationInfo::IconInfo icon_info;
      icon_info.data = pair.second.bitmap;
      icon_info.width = pair.first;
      icon_info.height = pair.first;
      web_app_info->icons.push_back(icon_info);
    }
  }
}

void RecordAppBanner(content::WebContents* contents, const GURL& app_url) {
  AppBannerSettingsHelper::RecordBannerEvent(
      contents, app_url, app_url.spec(),
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN,
      base::Time::Now());
}

WebappInstallSource ConvertExternalInstallSourceToInstallSource(
    ExternalInstallSource external_install_source) {
  WebappInstallSource install_source;
  switch (external_install_source) {
    case ExternalInstallSource::kInternalDefault:
      install_source = WebappInstallSource::INTERNAL_DEFAULT;
      break;
    case ExternalInstallSource::kExternalDefault:
      install_source = WebappInstallSource::EXTERNAL_DEFAULT;
      break;
    case ExternalInstallSource::kExternalPolicy:
      install_source = WebappInstallSource::EXTERNAL_POLICY;
      break;
    case ExternalInstallSource::kSystemInstalled:
      install_source = WebappInstallSource::SYSTEM_DEFAULT;
      break;
    case ExternalInstallSource::kArc:
      install_source = WebappInstallSource::ARC;
      break;
  }

  return install_source;
}

void RecordExternalAppInstallResultCode(
    const char* histogram_name,
    std::map<GURL, InstallResultCode> install_results) {
  for (const auto& url_and_result : install_results)
    base::UmaHistogramEnumeration(histogram_name, url_and_result.second);
}

}  // namespace web_app
