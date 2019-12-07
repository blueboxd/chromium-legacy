// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_icon_manager.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_registrar.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace extensions {

namespace {

void OnExtensionIconLoaded(BookmarkAppIconManager::ReadIconCallback callback,
                           const gfx::Image& image) {
  std::move(callback).Run(image.IsEmpty() ? SkBitmap() : *image.ToSkBitmap());
}

const Extension* GetBookmarkApp(Profile* profile,
                                const web_app::AppId& app_id) {
  const Extension* extension =
      ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(app_id);
  return (extension && extension->from_bookmark()) ? extension : nullptr;
}

void ReadExtensionIcon(Profile* profile,
                       const web_app::AppId& app_id,
                       int icon_size_in_px,
                       ExtensionIconSet::MatchType match_type,
                       BookmarkAppIconManager::ReadIconCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const Extension* extension = GetBookmarkApp(profile, app_id);
  DCHECK(extension);

  ImageLoader* loader = ImageLoader::Get(profile);
  loader->LoadImageAsync(
      extension,
      IconsInfo::GetIconResource(extension, icon_size_in_px, match_type),
      gfx::Size(icon_size_in_px, icon_size_in_px),
      base::BindOnce(&OnExtensionIconLoaded, std::move(callback)));
}

void OnExtensionIconsLoaded(
    BookmarkAppIconManager::ReadAllIconsCallback callback,
    const gfx::Image& image) {
  std::map<SquareSizePx, SkBitmap> icons_map;

  gfx::ImageSkia image_skia = image.AsImageSkia();
  for (const gfx::ImageSkiaRep& image_skia_rep : image_skia.image_reps())
    icons_map[image_skia_rep.pixel_width()] = image_skia_rep.GetBitmap();

  std::move(callback).Run(std::move(icons_map));
}

void ReadExtensionIcons(Profile* profile,
                        const web_app::AppId& app_id,
                        BookmarkAppIconManager::ReadAllIconsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const Extension* extension = GetBookmarkApp(profile, app_id);
  DCHECK(extension);

  const ExtensionIconSet& icons = IconsInfo::GetIcons(extension);

  std::vector<ImageLoader::ImageRepresentation> info_list;
  for (const ExtensionIconSet::IconMap::value_type& icon_info : icons.map()) {
    const int size_in_px = icon_info.first;

    ExtensionResource resource = IconsInfo::GetIconResource(
        extension, size_in_px, ExtensionIconSet::MATCH_EXACTLY);
    ImageLoader::ImageRepresentation image_rep{
        resource, ImageLoader::ImageRepresentation::NEVER_RESIZE,
        gfx::Size{size_in_px, size_in_px}, /*scale_factor=*/0.0f};
    info_list.push_back(image_rep);
  }

  ImageLoader* loader = ImageLoader::Get(profile);
  loader->LoadImagesAsync(
      extension, info_list,
      base::BindOnce(&OnExtensionIconsLoaded, std::move(callback)));
}

}  // anonymous namespace

BookmarkAppIconManager::BookmarkAppIconManager(Profile* profile)
    : profile_(profile) {}

BookmarkAppIconManager::~BookmarkAppIconManager() = default;

bool BookmarkAppIconManager::HasIcon(const web_app::AppId& app_id,
                                     int icon_size_in_px) const {
  return GetBookmarkApp(profile_, app_id);
}

bool BookmarkAppIconManager::HasSmallestIcon(const web_app::AppId& app_id,
                                             int icon_size_in_px) const {
  return GetBookmarkApp(profile_, app_id);
}

void BookmarkAppIconManager::ReadIcon(const web_app::AppId& app_id,
                                      int icon_size_in_px,
                                      ReadIconCallback callback) const {
  DCHECK(HasIcon(app_id, icon_size_in_px));
  ReadExtensionIcon(profile_, app_id, icon_size_in_px,
                    ExtensionIconSet::MATCH_EXACTLY, std::move(callback));
}

void BookmarkAppIconManager::ReadAllIcons(const web_app::AppId& app_id,
                                          ReadAllIconsCallback callback) const {
  ReadExtensionIcons(profile_, app_id, std::move(callback));
}

void BookmarkAppIconManager::ReadSmallestIcon(const web_app::AppId& app_id,
                                              int icon_size_in_px,
                                              ReadIconCallback callback) const {
  DCHECK(HasSmallestIcon(app_id, icon_size_in_px));
  ReadExtensionIcon(profile_, app_id, icon_size_in_px,
                    ExtensionIconSet::MATCH_BIGGER, std::move(callback));
}

void BookmarkAppIconManager::ReadSmallestCompressedIcon(
    const web_app::AppId& app_id,
    int icon_size_in_px,
    ReadCompressedIconCallback callback) const {
  NOTIMPLEMENTED();
  DCHECK(HasSmallestIcon(app_id, icon_size_in_px));
  std::move(callback).Run(std::vector<uint8_t>());
}

}  // namespace extensions
