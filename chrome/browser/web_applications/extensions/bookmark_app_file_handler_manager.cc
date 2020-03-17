// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_file_handler_manager.h"

#include <vector>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/web_app_file_handler.h"

namespace extensions {

BookmarkAppFileHandlerManager::BookmarkAppFileHandlerManager(Profile* profile)
    : web_app::FileHandlerManager(profile) {}

BookmarkAppFileHandlerManager::~BookmarkAppFileHandlerManager() = default;

const apps::FileHandlers* BookmarkAppFileHandlerManager::GetAllFileHandlers(
    const web_app::AppId& app_id) {
  auto* bookmark_app_registrar = registrar()->AsBookmarkAppRegistrar();
  DCHECK(bookmark_app_registrar);

  return WebAppFileHandlers::GetWebAppFileHandlers(
      bookmark_app_registrar->FindExtension(app_id));
}

}  // namespace extensions
