// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/favicon_source.h"

#include <cmath>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/favicon/favicon_request_handler_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_io_context.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/favicon/core/favicon_request_handler.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/history/core/browser/top_sites.h"
#include "components/sync/driver/sync_service_utils.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "url/gurl.h"

namespace {
favicon::FaviconRequestOrigin ParseFaviconRequestOrigin(const GURL& url) {
  GURL history_url(chrome::kChromeUIHistoryURL);
  if (url == history_url.Resolve(chrome::kChromeUIHistorySyncedTabs))
    return favicon::FaviconRequestOrigin::HISTORY_SYNCED_TABS;
  if (url == history_url)
    return favicon::FaviconRequestOrigin::HISTORY;
  return favicon::FaviconRequestOrigin::UNKNOWN;
}

sync_sessions::OpenTabsUIDelegate* GetOpenTabsUIDelegate(Profile* profile) {
  sync_sessions::SessionSyncService* session_sync_service =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(profile);
  DCHECK(session_sync_service);
  return session_sync_service->GetOpenTabsUIDelegate();
}

scoped_refptr<base::RefCountedMemory> GetSyncedFaviconForPageURL(
    Profile* profile,
    const GURL& page_url) {
  sync_sessions::OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate(profile);
  return open_tabs ? open_tabs->GetSyncedFaviconForPageURL(page_url.spec())
                   : nullptr;
}

// Check if user settings allow querying a Google server using history
// information.
bool CanSendHistoryDataToServer(Profile* profile) {
  return syncer::GetUploadToGoogleState(
             ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile),
             syncer::ModelType::HISTORY_DELETE_DIRECTIVES) ==
         syncer::UploadState::ACTIVE;
}

}  // namespace

FaviconSource::IconRequest::IconRequest()
    : size_in_dip(gfx::kFaviconSize), device_scale_factor(1.0f) {}

FaviconSource::IconRequest::IconRequest(
    const content::URLDataSource::GotDataCallback& cb,
    const GURL& path,
    int size,
    float scale)
    : callback(cb),
      request_path(path),
      size_in_dip(size),
      device_scale_factor(scale) {}

FaviconSource::IconRequest::IconRequest(const IconRequest& other) = default;

FaviconSource::IconRequest::~IconRequest() {
}

FaviconSource::FaviconSource(Profile* profile)
    : profile_(profile->GetOriginalProfile()) {}

FaviconSource::~FaviconSource() {
}

std::string FaviconSource::GetSource() {
  return chrome::kChromeUIFaviconHost;
}

void FaviconSource::StartDataRequest(
    const std::string& path,
    const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!favicon_service) {
    SendDefaultResponse(callback);
    return;
  }

  chrome::ParsedFaviconPath parsed;
  bool success = chrome::ParseFaviconPath(path, &parsed);
  if (!success) {
    SendDefaultResponse(callback);
    return;
  }

  GURL url(parsed.url);
  if (!url.is_valid()) {
    SendDefaultResponse(callback);
    return;
  }

  int desired_size_in_pixel =
      std::ceil(parsed.size_in_dip * parsed.device_scale_factor);

  content::WebContents* web_contents = wc_getter.Run();
  // web_contents->GetURL will not necessarily yield the original URL that
  // started the request, but for collecting metrics for chrome://history and
  // chrome://history/syncedTabs this will be ok.
  favicon::FaviconRequestOrigin unsafe_request_origin =
      web_contents ? ParseFaviconRequestOrigin(web_contents->GetURL())
                   : favicon::FaviconRequestOrigin::UNKNOWN;
  if (parsed.is_icon_url) {
    // TODO(michaelbai): Change GetRawFavicon to support combination of
    // IconType.
    favicon_service->GetRawFavicon(
        url, favicon_base::IconType::kFavicon, desired_size_in_pixel,
        base::BindRepeating(&FaviconSource::OnFaviconDataAvailable,
                            base::Unretained(this),
                            IconRequest(callback, url, parsed.size_in_dip,
                                        parsed.device_scale_factor)),
        &cancelable_task_tracker_);
  } else {
    // Intercept requests for prepopulated pages if TopSites exists.
    scoped_refptr<history::TopSites> top_sites =
        TopSitesFactory::GetForProfile(profile_);
    if (top_sites) {
      for (const auto& prepopulated_page : top_sites->GetPrepopulatedPages()) {
        if (url == prepopulated_page.most_visited.url) {
          ui::ScaleFactor resource_scale_factor =
              ui::GetSupportedScaleFactor(parsed.device_scale_factor);
          callback.Run(
              ui::ResourceBundle::GetSharedInstance()
                  .LoadDataResourceBytesForScale(prepopulated_page.favicon_id,
                                                 resource_scale_factor));
          return;
        }
      }
    }
    favicon::FaviconRequestHandler* favicon_request_handler =
        FaviconRequestHandlerFactory::GetForBrowserContext(profile_);
    if (!favicon_request_handler) {
      SendDefaultResponse(callback);
      return;
    }
    sync_sessions::OpenTabsUIDelegate* open_tabs =
        GetOpenTabsUIDelegate(profile_);
    favicon_request_handler->GetRawFaviconForPageURL(
        url, desired_size_in_pixel,
        base::BindOnce(&FaviconSource::OnFaviconDataAvailable,
                       base::Unretained(this),
                       IconRequest(callback, url, parsed.size_in_dip,
                                   parsed.device_scale_factor)),
        unsafe_request_origin, favicon::FaviconRequestPlatform::kDesktop,
        /*icon_url_for_uma=*/
        open_tabs ? open_tabs->GetIconUrlForPageUrl(url) : GURL(),
        base::BindOnce(&GetSyncedFaviconForPageURL, base::Unretained(profile_)),
        CanSendHistoryDataToServer(profile_), &cancelable_task_tracker_);
  }
}

std::string FaviconSource::GetMimeType(const std::string&) {
  // We need to explicitly return a mime type, otherwise if the user tries to
  // drag the image they get no extension.
  return "image/png";
}

bool FaviconSource::AllowCaching() {
  return false;
}

bool FaviconSource::ShouldReplaceExistingSource() {
  // Leave the existing DataSource in place, otherwise we'll drop any pending
  // requests on the floor.
  return false;
}

bool FaviconSource::ShouldServiceRequest(
    const GURL& url,
    content::ResourceContext* resource_context,
    int render_process_id) {
  if (url.SchemeIs(chrome::kChromeSearchScheme)) {
    return InstantIOContext::ShouldServiceRequest(url, resource_context,
                                                  render_process_id);
  }
  return URLDataSource::ShouldServiceRequest(url, resource_context,
                                             render_process_id);
}

ui::NativeTheme* FaviconSource::GetNativeTheme() {
  return ui::NativeTheme::GetInstanceForNativeUi();
}

void FaviconSource::OnFaviconDataAvailable(
    const IconRequest& request,
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  if (bitmap_result.is_valid()) {
    // Forward the data along to the networking system.
    request.callback.Run(bitmap_result.bitmap_data.get());
  } else {
    SendDefaultResponse(request);
  }
}

void FaviconSource::SendDefaultResponse(
    const content::URLDataSource::GotDataCallback& callback) {
  SendDefaultResponse(IconRequest(callback, GURL(), 16, 1.0f));
}

void FaviconSource::SendDefaultResponse(const IconRequest& icon_request) {
  const bool dark = GetNativeTheme()->SystemDarkModeEnabled();
  int resource_id;
  switch (icon_request.size_in_dip) {
    case 64:
      resource_id = dark ? IDR_DEFAULT_FAVICON_DARK_64 : IDR_DEFAULT_FAVICON_64;
      break;
    case 32:
      resource_id = dark ? IDR_DEFAULT_FAVICON_DARK_32 : IDR_DEFAULT_FAVICON_32;
      break;
    default:
      resource_id = dark ? IDR_DEFAULT_FAVICON_DARK : IDR_DEFAULT_FAVICON;
      break;
  }
  icon_request.callback.Run(LoadIconBytes(icon_request, resource_id));
}

base::RefCountedMemory* FaviconSource::LoadIconBytes(const IconRequest& request,
                                                     int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      resource_id, ui::GetSupportedScaleFactor(request.device_scale_factor));
}
