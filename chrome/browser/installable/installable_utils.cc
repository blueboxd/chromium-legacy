// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/installable/installable_utils.h"

#include "build/build_config.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/shortcut_helper.h"
#else
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#endif

bool IsWebAppInstalledForUrl(content::BrowserContext* browser_context,
                             const GURL& url) {
#if defined(OS_ANDROID)
  // This will still detect the presence of a WebAPK even if Chrome's data is
  // cleared
  return ShortcutHelper::IsWebApkInstalled(browser_context, url);
#else
  return web_app::FindInstalledAppWithUrlInScope(
             Profile::FromBrowserContext(browser_context), url)
      .has_value();
#endif
}

bool DoesOriginContainAnyInstalledWebApp(
    content::BrowserContext* browser_context,
    const GURL& origin) {
  DCHECK_EQ(origin, origin.GetOrigin());
#if defined(OS_ANDROID)
  return ShortcutHelper::DoesOriginContainAnyInstalledWebApk(origin);
#else
  auto* provider = web_app::WebAppProviderFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
  return provider->registrar().DoesScopeContainAnyApp(origin);
#endif
}
