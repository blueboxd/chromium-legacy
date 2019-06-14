// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_tab_helper.h"

#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

namespace extensions {

BookmarkAppTabHelper::BookmarkAppTabHelper(content::WebContents* web_contents)
    : WebAppTabHelperBase(web_contents) {
}

BookmarkAppTabHelper::~BookmarkAppTabHelper() = default;

// static
BookmarkAppTabHelper* BookmarkAppTabHelper::CreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(!FromWebContents(web_contents));

  auto tab_helper = std::make_unique<BookmarkAppTabHelper>(web_contents);
  BookmarkAppTabHelper* result = tab_helper.get();
  web_contents->SetUserData(UserDataKey(), std::move(tab_helper));
  return result;
}

web_app::WebAppTabHelperBase* BookmarkAppTabHelper::CloneForWebContents(
    content::WebContents* web_contents) const {
  BookmarkAppTabHelper* new_tab_helper =
      BookmarkAppTabHelper::CreateForWebContents(web_contents);
  return new_tab_helper;
}

bool BookmarkAppTabHelper::IsInAppWindow() const {
  return util::IsWebContentsInAppWindow(web_contents());
}

bool BookmarkAppTabHelper::IsUserInstalled() const {
  const Extension* app = GetExtension();
  return app && !app->was_installed_by_default();
}

bool BookmarkAppTabHelper::IsFromInstallButton() const {
  const Extension* app = GetExtension();
  // TODO(loyso): Use something better to record apps installed from promoted
  // UIs. crbug.com/774918.
  return app && app->is_hosted_app() && UrlHandlers::GetUrlHandlers(app);
}

const Extension* BookmarkAppTabHelper::GetExtension() const {
  DCHECK(!app_id().empty());
  content::BrowserContext* browser_context =
      web_contents()->GetBrowserContext();
  const Extension* app =
      ExtensionRegistry::Get(browser_context)
          ->GetExtensionById(app_id(), ExtensionRegistry::EVERYTHING);
  return app;
}

}  // namespace extensions
