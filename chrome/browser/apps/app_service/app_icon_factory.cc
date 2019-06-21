// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon_factory.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/apps/app_service/dip_px_util.h"
#include "chrome/browser/extensions/chrome_app_icon.h"
#include "chrome/browser/extensions/chrome_app_icon_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "content/public/browser/system_connector.h"
#include "extensions/browser/component_extension_resource_manager.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/app_list/md_icon_normalizer.h"
#endif

namespace {

void ApplyIconEffects(apps::IconEffects icon_effects,
                      int size_hint_in_dip,
                      gfx::ImageSkia* image_skia) {
  extensions::ChromeAppIcon::ResizeFunction resize_function;
#if defined(OS_CHROMEOS)
  if (icon_effects & apps::IconEffects::kResizeAndPad) {
    // TODO(crbug.com/826982): khmel@ notes that MD post-processing is not
    // always applied: "See legacy code:
    // https://cs.chromium.org/search/?q=ChromeAppIconLoader&type=cs In one
    // cases MD design is used in another not."
    resize_function =
        base::BindRepeating(&app_list::MaybeResizeAndPadIconForMd);
  }
#endif

  bool apply_chrome_badge = icon_effects & apps::IconEffects::kBadge;
  bool app_launchable = !(icon_effects & apps::IconEffects::kGray);
  bool from_bookmark = icon_effects & apps::IconEffects::kRoundCorners;

  extensions::ChromeAppIcon::ApplyEffects(size_hint_in_dip, resize_function,
                                          apply_chrome_badge, app_launchable,
                                          from_bookmark, image_skia);
}

std::vector<uint8_t> ReadFileAsCompressedData(const base::FilePath path) {
  std::string data;
  base::ReadFileToString(path, &data);
  return std::vector<uint8_t>(data.begin(), data.end());
}

std::vector<uint8_t> CompressedDataFromResource(
    extensions::ExtensionResource resource) {
  const base::FilePath& path = resource.GetFilePath();
  if (path.empty()) {
    return std::vector<uint8_t>();
  }
  return ReadFileAsCompressedData(path);
}

// Runs |callback| passing an IconValuePtr with a compressed image: a
// std::vector<uint8_t>.
//
// It will fall back to the |default_icon_resource| if the data is empty.
void RunCallbackWithCompressedData(
    int size_hint_in_dip,
    int default_icon_resource,
    bool is_placeholder_icon,
    apps::IconEffects icon_effects,
    apps::mojom::Publisher::LoadIconCallback callback,
    std::vector<uint8_t> data) {
  // TODO(crbug.com/826982): if icon_effects is non-zero, we should arguably
  // decompress the image, apply the icon_effects, and recompress the
  // post-processed image.
  //
  // Even if there are no icon_effects, we might also want to do this if the
  // size_hint_in_dip doesn't match the compressed image's pixel size. This
  // isn't trivial, though, as determining the compressed image's pixel size
  // might involve a sandboxed decoder process.

  if (!data.empty()) {
    apps::mojom::IconValuePtr iv = apps::mojom::IconValue::New();
    iv->icon_compression = apps::mojom::IconCompression::kCompressed;
    iv->compressed = std::move(data);
    iv->is_placeholder_icon = is_placeholder_icon;
    std::move(callback).Run(std::move(iv));
    return;
  }
  if (default_icon_resource) {
    LoadIconFromResource(apps::mojom::IconCompression::kCompressed,
                         size_hint_in_dip, default_icon_resource,
                         is_placeholder_icon, icon_effects,
                         std::move(callback));
    return;
  }
  std::move(callback).Run(apps::mojom::IconValue::New());
}

// Like RunCallbackWithCompressedData, but calls "fallback(callback)" if the
// data is empty.
void RunCallbackWithCompressedDataWithFallback(
    int size_hint_in_dip,
    bool is_placeholder_icon,
    apps::IconEffects icon_effects,
    apps::mojom::Publisher::LoadIconCallback callback,
    base::OnceCallback<void(apps::mojom::Publisher::LoadIconCallback)> fallback,
    std::vector<uint8_t> data) {
  if (data.empty()) {
    std::move(fallback).Run(std::move(callback));
    return;
  }
  constexpr int default_icon_resource = 0;
  RunCallbackWithCompressedData(size_hint_in_dip, default_icon_resource,
                                is_placeholder_icon, icon_effects,
                                std::move(callback), std::move(data));
}

// Runs |callback| passing an IconValuePtr with an uncompressed image: a
// SkBitmap.
void RunCallbackWithUncompressedSkBitmap(
    int size_hint_in_dip,
    bool is_placeholder_icon,
    apps::IconEffects icon_effects,
    apps::mojom::Publisher::LoadIconCallback callback,
    const SkBitmap& bitmap) {
  apps::mojom::IconValuePtr iv = apps::mojom::IconValue::New();
  iv->icon_compression = apps::mojom::IconCompression::kUncompressed;
  iv->uncompressed = gfx::ImageSkia(gfx::ImageSkiaRep(bitmap, 0.0f));
  iv->is_placeholder_icon = is_placeholder_icon;
  if (icon_effects && !iv->uncompressed.isNull()) {
    ApplyIconEffects(icon_effects, size_hint_in_dip, &iv->uncompressed);
  }
  std::move(callback).Run(std::move(iv));
}

// Runs |callback| after converting (in a separate sandboxed process) from a
// std::vector<uint8_t> to a SkBitmap. It calls "fallback(callback)" if the
// data is empty.
void RunCallbackWithCompressedDataToUncompressWithFallback(
    int size_hint_in_dip,
    bool is_placeholder_icon,
    apps::IconEffects icon_effects,
    apps::mojom::Publisher::LoadIconCallback callback,
    base::OnceCallback<void(apps::mojom::Publisher::LoadIconCallback)> fallback,
    std::vector<uint8_t> data) {
  if (data.empty()) {
    std::move(fallback).Run(std::move(callback));
    return;
  }
  data_decoder::DecodeImage(
      content::GetSystemConnector(), data,
      data_decoder::mojom::ImageCodec::DEFAULT, false,
      data_decoder::kDefaultMaxSizeInBytes, gfx::Size(),
      base::BindOnce(&RunCallbackWithUncompressedSkBitmap, size_hint_in_dip,
                     is_placeholder_icon, icon_effects, std::move(callback)));
}

// Runs |callback| passing an IconValuePtr with an uncompressed image: an
// ImageSkia.
//
// It will fall back to the |default_icon_resource| if the image is null.
void RunCallbackWithUncompressedImageSkia(
    int size_hint_in_dip,
    int default_icon_resource,
    bool is_placeholder_icon,
    apps::IconEffects icon_effects,
    apps::mojom::Publisher::LoadIconCallback callback,
    const gfx::ImageSkia image) {
  if (!image.isNull()) {
    apps::mojom::IconValuePtr iv = apps::mojom::IconValue::New();
    iv->icon_compression = apps::mojom::IconCompression::kUncompressed;
    iv->uncompressed = image;
    iv->is_placeholder_icon = is_placeholder_icon;
    if (icon_effects && !iv->uncompressed.isNull()) {
      ApplyIconEffects(icon_effects, size_hint_in_dip, &iv->uncompressed);
    }
    std::move(callback).Run(std::move(iv));
    return;
  }
  if (default_icon_resource) {
    LoadIconFromResource(apps::mojom::IconCompression::kUncompressed,
                         size_hint_in_dip, default_icon_resource,
                         is_placeholder_icon, icon_effects,
                         std::move(callback));
    return;
  }
  std::move(callback).Run(apps::mojom::IconValue::New());
}

// Runs |callback| passing an IconValuePtr with an uncompressed image: an
// Image.
void RunCallbackWithUncompressedImage(
    int size_hint_in_dip,
    int default_icon_resource,
    bool is_placeholder_icon,
    apps::IconEffects icon_effects,
    apps::mojom::Publisher::LoadIconCallback callback,
    const gfx::Image& image) {
  RunCallbackWithUncompressedImageSkia(
      size_hint_in_dip, default_icon_resource, is_placeholder_icon,
      icon_effects, std::move(callback), image.AsImageSkia());
}

}  // namespace

namespace apps {

void LoadIconFromExtension(apps::mojom::IconCompression icon_compression,
                           int size_hint_in_dip,
                           content::BrowserContext* context,
                           const std::string& extension_id,
                           IconEffects icon_effects,
                           apps::mojom::Publisher::LoadIconCallback callback) {
  constexpr bool is_placeholder_icon = false;
  int size_hint_in_px = apps_util::ConvertDipToPx(size_hint_in_dip);

  // This is the default icon for AppType::kExtension. Other app types might
  // use a different default icon, such as IDR_LOGO_CROSTINI_DEFAULT_192.
  constexpr int default_icon_resource = IDR_APP_DEFAULT_ICON;

  const extensions::Extension* extension =
      extensions::ExtensionSystem::Get(context)
          ->extension_service()
          ->GetInstalledExtension(extension_id);
  if (extension) {
    extensions::ExtensionResource ext_resource =
        extensions::IconsInfo::GetIconResource(extension, size_hint_in_px,
                                               ExtensionIconSet::MATCH_BIGGER);

    switch (icon_compression) {
      case apps::mojom::IconCompression::kUnknown:
        break;

      case apps::mojom::IconCompression::kUncompressed: {
        extensions::ImageLoader::Get(context)->LoadImageAsync(
            extension, std::move(ext_resource),
            gfx::Size(size_hint_in_px, size_hint_in_px),
            base::BindOnce(&RunCallbackWithUncompressedImage, size_hint_in_dip,
                           default_icon_resource, is_placeholder_icon,
                           icon_effects, std::move(callback)));
        return;
      }

      case apps::mojom::IconCompression::kCompressed: {
        // Load some component extensions' icons from statically compiled
        // resources (built into the Chrome binary), and other extensions'
        // icons (whether component extensions or otherwise) from files on
        // disk.
        //
        // For the kUncompressed case above, RunCallbackWithUncompressedImage
        // calls extensions::ImageLoader::LoadImageAsync, which already handles
        // that distinction. We can't use LoadImageAsync here, because the
        // caller has asked for compressed icons (i.e. PNG-formatted data), not
        // uncompressed (i.e. a gfx::ImageSkia).
        if (extension->location() == extensions::Manifest::COMPONENT) {
          int resource_id = 0;
          const extensions::ComponentExtensionResourceManager* manager =
              extensions::ExtensionsBrowserClient::Get()
                  ->GetComponentExtensionResourceManager();
          if (manager && manager->IsComponentExtensionResource(
                             extension->path(), ext_resource.relative_path(),
                             &resource_id)) {
            base::StringPiece data =
                ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
                    resource_id);
            RunCallbackWithCompressedData(
                size_hint_in_dip, default_icon_resource, is_placeholder_icon,
                icon_effects, std::move(callback),
                std::vector<uint8_t>(data.begin(), data.end()));
            return;
          }
        }

        // Try and load data from the resource file.
        base::PostTaskWithTraitsAndReplyWithResult(
            FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
            base::BindOnce(&CompressedDataFromResource,
                           std::move(ext_resource)),
            base::BindOnce(&RunCallbackWithCompressedData, size_hint_in_dip,
                           default_icon_resource, is_placeholder_icon,
                           icon_effects, std::move(callback)));
        return;
      }
    }
  }

  // Fall back to the default_icon_resource.
  LoadIconFromResource(icon_compression, size_hint_in_dip,
                       default_icon_resource, is_placeholder_icon, icon_effects,
                       std::move(callback));
}

void LoadIconFromFileWithFallback(
    apps::mojom::IconCompression icon_compression,
    int size_hint_in_dip,
    const base::FilePath& path,
    IconEffects icon_effects,
    apps::mojom::Publisher::LoadIconCallback callback,
    base::OnceCallback<void(apps::mojom::Publisher::LoadIconCallback)>
        fallback) {
  constexpr bool is_placeholder_icon = false;
  switch (icon_compression) {
    case apps::mojom::IconCompression::kUnknown:
      break;

    case apps::mojom::IconCompression::kUncompressed: {
      base::PostTaskWithTraitsAndReplyWithResult(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
          base::BindOnce(&ReadFileAsCompressedData, path),
          base::BindOnce(&RunCallbackWithCompressedDataToUncompressWithFallback,
                         size_hint_in_dip, is_placeholder_icon, icon_effects,
                         std::move(callback), std::move(fallback)));

      return;
    }

    case apps::mojom::IconCompression::kCompressed: {
      base::PostTaskWithTraitsAndReplyWithResult(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
          base::BindOnce(&ReadFileAsCompressedData, path),
          base::BindOnce(&RunCallbackWithCompressedDataWithFallback,
                         size_hint_in_dip, is_placeholder_icon, icon_effects,
                         std::move(callback), std::move(fallback)));
      return;
    }
  }

  std::move(callback).Run(apps::mojom::IconValue::New());
}

void LoadIconFromResource(apps::mojom::IconCompression icon_compression,
                          int size_hint_in_dip,
                          int resource_id,
                          bool is_placeholder_icon,
                          IconEffects icon_effects,
                          apps::mojom::Publisher::LoadIconCallback callback) {
  // This must be zero, to avoid a potential infinite loop if the
  // RunCallbackWithXxx functions could otherwise call back into
  // LoadIconFromResource.
  constexpr int default_icon_resource = 0;

  if (resource_id != 0) {
    switch (icon_compression) {
      case apps::mojom::IconCompression::kUnknown:
        break;

      case apps::mojom::IconCompression::kUncompressed: {
        gfx::ImageSkia* unscaled =
            ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                resource_id);
        RunCallbackWithUncompressedImageSkia(
            size_hint_in_dip, default_icon_resource, is_placeholder_icon,
            icon_effects, std::move(callback),
            gfx::ImageSkiaOperations::CreateResizedImage(
                *unscaled, skia::ImageOperations::RESIZE_BEST,
                gfx::Size(size_hint_in_dip, size_hint_in_dip)));
        return;
      }

      case apps::mojom::IconCompression::kCompressed: {
        base::StringPiece data =
            ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
                resource_id);
        RunCallbackWithCompressedData(
            size_hint_in_dip, default_icon_resource, is_placeholder_icon,
            icon_effects, std::move(callback),
            std::vector<uint8_t>(data.begin(), data.end()));
        return;
      }
    }
  }

  std::move(callback).Run(apps::mojom::IconValue::New());
}

}  // namespace apps
