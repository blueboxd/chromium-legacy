// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/mac_apps_navigation_throttle.h"

#import <Cocoa/Cocoa.h>
#include <dlfcn.h>

#include "base/mac/sdk_forward_declarations.h"
#include "base/strings/sys_string_conversions.h"
#include "content/public/browser/navigation_handle.h"
#include "net/base/mac/url_conversions.h"

namespace apps {

namespace {

const char kSafariServicesFrameworkPath[] =
    "/System/Library/Frameworks/SafariServices.framework/"
    "Versions/Current/SafariServices";

IntentPickerAppInfo AppInfoForAppUrl(NSURL* app_url) {
  NSString* app_name = nil;
  if (![app_url getResourceValue:&app_name
                          forKey:NSURLLocalizedNameKey
                           error:nil]) {
    // This shouldn't happen but just in case.
    app_name = [app_url lastPathComponent];
  }
  NSImage* app_icon = nil;
  if (![app_url getResourceValue:&app_icon
                          forKey:NSURLEffectiveIconKey
                           error:nil]) {
    // This shouldn't happen but just in case.
    app_icon = [NSImage imageNamed:NSImageNameApplicationIcon];
  }
  app_icon.size = NSMakeSize(16, 16);

  return IntentPickerAppInfo(apps::mojom::AppType::kMacNative,
                             gfx::Image(app_icon),
                             base::SysNSStringToUTF8([app_url path]),
                             base::SysNSStringToUTF8(app_name));
}

SFUniversalLink* GetUniversalLink(const GURL& url) {
  static void* safari_services = []() -> void* {
    if (@available(macOS 10.15, *))
      return dlopen(kSafariServicesFrameworkPath, RTLD_LAZY);
    return nullptr;
  }();

  static const Class SFUniversalLink_class =
      NSClassFromString(@"SFUniversalLink");

  if (!safari_services || !SFUniversalLink_class)
    return nil;

  return [[[SFUniversalLink_class alloc]
      initWithWebpageURL:net::NSURLWithGURL(url)] autorelease];
}

std::vector<IntentPickerAppInfo> AppInfoForUrlImpl(const GURL& url) {
  std::vector<IntentPickerAppInfo> apps;

  SFUniversalLink* link = GetUniversalLink(url);
  if (link)
    apps.push_back(AppInfoForAppUrl(link.applicationURL));

  return apps;
}

}  // namespace

// static
std::unique_ptr<apps::AppsNavigationThrottle>
MacAppsNavigationThrottle::MaybeCreate(content::NavigationHandle* handle) {
  if (!handle->IsInMainFrame())
    return nullptr;

  if (!apps::AppsNavigationThrottle::CanCreate(handle->GetWebContents()))
    return nullptr;

  return std::make_unique<MacAppsNavigationThrottle>(handle);
}

// static
void MacAppsNavigationThrottle::ShowIntentPickerBubble(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url) {
  // First, the Universal Link, if there is one.
  std::vector<IntentPickerAppInfo> apps = AppInfoForUrlImpl(url);

  // Then, any PWAs.
  apps = apps::AppsNavigationThrottle::FindPwaForUrl(web_contents, url,
                                                     std::move(apps));

  bool show_persistence_options = ShouldShowPersistenceOptions(apps);
  apps::AppsNavigationThrottle::ShowIntentPickerBubbleForApps(
      web_contents, std::move(apps), show_persistence_options,
      base::BindOnce(&OnIntentPickerClosed, web_contents,
                     ui_auto_display_service, url));
}

MacAppsNavigationThrottle::MacAppsNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : apps::AppsNavigationThrottle(navigation_handle) {}

MacAppsNavigationThrottle::~MacAppsNavigationThrottle() = default;

// static
void MacAppsNavigationThrottle::OnIntentPickerClosed(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url,
    const std::string& launch_name,
    apps::mojom::AppType app_type,
    apps::IntentPickerCloseReason close_reason,
    bool should_persist) {
  if (app_type == apps::mojom::AppType::kMacNative) {
    if (close_reason == apps::IntentPickerCloseReason::OPEN_APP) {
      [[NSWorkspace sharedWorkspace]
                      openURLs:@[ net::NSURLWithGURL(url) ]
          withApplicationAtURL:[NSURL fileURLWithPath:base::SysUTF8ToNSString(
                                                          launch_name)]
                       options:0
                 configuration:@{}
                         error:nil];
    }
    apps::AppsNavigationThrottle::RecordUma(launch_name, app_type, close_reason,
                                            Source::kHttpOrHttps,
                                            should_persist);
    return;
  }
  apps::AppsNavigationThrottle::OnIntentPickerClosed(
      web_contents, ui_auto_display_service, url, launch_name, app_type,
      close_reason, should_persist);
}

apps::AppsNavigationThrottle::PickerShowState
MacAppsNavigationThrottle::GetPickerShowState(
    const std::vector<apps::IntentPickerAppInfo>& apps_for_picker,
    content::WebContents* web_contents,
    const GURL& url) {
  return PickerShowState::kOmnibox;
}

IntentPickerResponse MacAppsNavigationThrottle::GetOnPickerClosedCallback(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url) {
  return base::BindOnce(&OnIntentPickerClosed, web_contents,
                        ui_auto_display_service, url);
}

std::vector<IntentPickerAppInfo> MacAppsNavigationThrottle::AppInfoForUrl(
    const GURL& url) {
  return AppInfoForUrlImpl(url);
}

}  // namespace apps
