// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon_factory.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/apps/app_service/dip_px_util.h"
#include "chrome/browser/extensions/chrome_app_icon.h"
#include "chrome/browser/extensions/chrome_app_icon_loader.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_icon_manager.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/component_extension_resource_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/app_list/md_icon_normalizer.h"
#endif

namespace {

std::map<std::pair<int, int>, gfx::ImageSkia>& GetResourceIconCache() {
  static base::NoDestructor<std::map<std::pair<int, int>, gfx::ImageSkia>>
      cache;
  return *cache;
}

std::vector<uint8_t> ReadFileAsCompressedData(const base::FilePath path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::string data;
  base::ReadFileToString(path, &data);
  return std::vector<uint8_t>(data.begin(), data.end());
}

std::vector<uint8_t> CompressedDataFromResource(
    extensions::ExtensionResource resource) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  const base::FilePath& path = resource.GetFilePath();
  if (path.empty()) {
    return std::vector<uint8_t>();
  }
  return ReadFileAsCompressedData(path);
}

SkBitmap DecompressToSkBitmap(const unsigned char* data, size_t size) {
  base::AssertLongCPUWorkAllowed();
  SkBitmap decoded;
  bool success = gfx::PNGCodec::Decode(data, size, &decoded);
  DCHECK(success);
  return decoded;
}

gfx::ImageSkia SkBitmapToImageSkia(SkBitmap bitmap) {
  return gfx::ImageSkia(gfx::ImageSkiaRep(bitmap, 0.0f));
}

// Returns a callback that converts an SkBitmap to an ImageSkia.
base::OnceCallback<void(const SkBitmap&)> SkBitmapToImageSkiaCallback(
    base::OnceCallback<void(gfx::ImageSkia)> callback) {
  return base::BindOnce(
      [](base::OnceCallback<void(gfx::ImageSkia)> callback,
         const SkBitmap& bitmap) {
        if (bitmap.isNull()) {
          std::move(callback).Run(gfx::ImageSkia());
          return;
        }
        std::move(callback).Run(SkBitmapToImageSkia(bitmap));
      },
      std::move(callback));
}

// Returns a callback that converts compressed data to an ImageSkia.
base::OnceCallback<void(std::vector<uint8_t> compressed_data)>
CompressedDataToImageSkiaCallback(
    base::OnceCallback<void(gfx::ImageSkia)> callback) {
  return base::BindOnce(
      [](base::OnceCallback<void(gfx::ImageSkia)> callback,
         std::vector<uint8_t> compressed_data) {
        if (compressed_data.empty()) {
          std::move(callback).Run(gfx::ImageSkia());
          return;
        }
        // DecompressToSkBitmap is a CPU intensive task that must not run on the
        // UI thread, so post the processing over to the thread pool.
        base::ThreadPool::PostTaskAndReplyWithResult(
            FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
            base::BindOnce(
                [](std::vector<uint8_t> compressed_data) {
                  return SkBitmapToImageSkia(DecompressToSkBitmap(
                      compressed_data.data(), compressed_data.size()));
                },
                std::move(compressed_data)),
            std::move(callback));
      },
      std::move(callback));
}

// Returns a callback that converts a gfx::Image to an ImageSkia.
base::OnceCallback<void(const gfx::Image&)> ImageToImageSkia(
    base::OnceCallback<void(gfx::ImageSkia)> callback) {
  return base::BindOnce(
      [](base::OnceCallback<void(gfx::ImageSkia)> callback,
         const gfx::Image& image) {
        std::move(callback).Run(image.AsImageSkia());
      },
      std::move(callback));
}

base::OnceCallback<void(const favicon_base::FaviconRawBitmapResult&)>
FaviconResultToImageSkia(base::OnceCallback<void(gfx::ImageSkia)> callback) {
  return base::BindOnce(
      [](base::OnceCallback<void(gfx::ImageSkia)> callback,
         const favicon_base::FaviconRawBitmapResult& result) {
        if (!result.is_valid()) {
          std::move(callback).Run(gfx::ImageSkia());
          return;
        }
        // It would be nice to not do a memory copy here, but
        // DecodeImageIsolated requires a std::vector, and RefCountedMemory
        // doesn't supply that.
        std::move(CompressedDataToImageSkiaCallback(std::move(callback)))
            .Run(std::vector<uint8_t>(
                result.bitmap_data->front(),
                result.bitmap_data->front() + result.bitmap_data->size()));
      },
      std::move(callback));
}

// Loads the compressed data of an icon at the requested size (or larger) for
// the given extension.
void LoadCompressedDataFromExtension(
    const extensions::Extension* extension,
    int size_hint_in_px,
    base::OnceCallback<void(std::vector<uint8_t>)> compressed_data_callback) {
  // Load some component extensions' icons from statically compiled
  // resources (built into the Chrome binary), and other extensions'
  // icons (whether component extensions or otherwise) from files on
  // disk.
  extensions::ExtensionResource ext_resource =
      extensions::IconsInfo::GetIconResource(extension, size_hint_in_px,
                                             ExtensionIconSet::MATCH_BIGGER);

  if (extension && extension->location() == extensions::Manifest::COMPONENT) {
    int resource_id = 0;
    const extensions::ComponentExtensionResourceManager* manager =
        extensions::ExtensionsBrowserClient::Get()
            ->GetComponentExtensionResourceManager();
    if (manager &&
        manager->IsComponentExtensionResource(
            extension->path(), ext_resource.relative_path(), &resource_id)) {
      base::StringPiece data =
          ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
              resource_id);
      std::move(compressed_data_callback)
          .Run(std::vector<uint8_t>(data.begin(), data.end()));
      return;
    }
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&CompressedDataFromResource, std::move(ext_resource)),
      std::move(compressed_data_callback));
}

// Encode the ImageSkia to the compressed PNG data with the image's 1.0f scale
// factor representation. Return the encoded PNG data.
//
// This function should not be called on the UI thread.
std::vector<uint8_t> EncodeImage(const gfx::ImageSkia image) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::vector<uint8_t> image_data;

  const gfx::ImageSkiaRep& image_skia_rep = image.GetRepresentation(1.0f);
  if (image_skia_rep.scale() != 1.0f) {
    return image_data;
  }

  const SkBitmap& bitmap = image_skia_rep.GetBitmap();
  if (bitmap.drawsNothing()) {
    return image_data;
  }

  base::AssertLongCPUWorkAllowed();
  constexpr bool discard_transparency = false;
  bool success = gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, discard_transparency,
                                                   &image_data);
  if (!success) {
    return std::vector<uint8_t>();
  }
  return image_data;
}

// This pipeline is meant to:
// * Simplify loading icons, as things like effects and compression are common
//   to all loading.
// * Allow the caller to halt the process by destructing the loader at any time,
// * Allow easy additions to the pipeline if necessary (like new effects or
// backups).
// Must be created & run from the UI thread.
class IconLoadingPipeline : public base::RefCounted<IconLoadingPipeline> {
 public:
  static const int kInvalidIconResource = 0;
  static const int kFaviconFallbackImagePx =
      extension_misc::EXTENSION_ICON_BITTY;

  IconLoadingPipeline(apps::mojom::IconCompression icon_compression,
                      int size_hint_in_dip,
                      bool is_placeholder_icon,
                      apps::IconEffects icon_effects,
                      int fallback_icon_resource,
                      apps::mojom::Publisher::LoadIconCallback callback)
      : IconLoadingPipeline(icon_compression,
                            size_hint_in_dip,
                            is_placeholder_icon,
                            icon_effects,
                            fallback_icon_resource,
                            base::OnceCallback<void(
                                apps::mojom::Publisher::LoadIconCallback)>(),
                            std::move(callback)) {}

  IconLoadingPipeline(
      apps::mojom::IconCompression icon_compression,
      int size_hint_in_dip,
      bool is_placeholder_icon,
      apps::IconEffects icon_effects,
      int fallback_icon_resource,
      base::OnceCallback<void(apps::mojom::Publisher::LoadIconCallback)>
          fallback,
      apps::mojom::Publisher::LoadIconCallback callback)
      : icon_compression_(icon_compression),
        size_hint_in_dip_(size_hint_in_dip),
        is_placeholder_icon_(is_placeholder_icon),
        icon_effects_(icon_effects),
        fallback_icon_resource_(fallback_icon_resource),
        callback_(std::move(callback)),
        fallback_callback_(std::move(fallback)) {}

  void LoadWebAppIcon(const std::string& web_app_id,
                      const GURL& launch_url,
                      const web_app::AppIconManager& icon_manager,
                      Profile* profile);

  void LoadExtensionIcon(const extensions::Extension* extension,
                         content::BrowserContext* context);

  // The image file must be compressed using the default encoding.
  void LoadCompressedIconFromFile(const base::FilePath& path);

  void LoadIconFromResource(int icon_resource);

 private:
  friend class base::RefCounted<IconLoadingPipeline>;

  ~IconLoadingPipeline() {
    if (!callback_.is_null()) {
      std::move(callback_).Run(apps::mojom::IconValue::New());
    }
  }

  void MaybeApplyEffectsAndComplete(const gfx::ImageSkia image);

  void CompleteWithCompressed(std::vector<uint8_t> data);

  void CompleteWithUncompressed(gfx::ImageSkia image);

  void MaybeLoadFallbackOrCompleteEmpty();

  apps::mojom::IconCompression icon_compression_;
  int size_hint_in_dip_;
  bool is_placeholder_icon_;
  apps::IconEffects icon_effects_;

  // If |fallback_favicon_url_| is populated, then the favicon service is the
  // first fallback method attempted in MaybeLoadFallbackOrCompleteEmpty().
  // These members are only populated from LoadWebAppIcon or LoadExtensionIcon.
  GURL fallback_favicon_url_;
  Profile* profile_ = nullptr;

  // If |fallback_icon_resource_| is not |kInvalidIconResource|, then it is the
  // second fallback method attempted in MaybeLoadFallbackOrCompleteEmpty()
  // (after the favicon service).
  int fallback_icon_resource_;

  apps::mojom::Publisher::LoadIconCallback callback_;

  // A custom fallback operation to try.
  base::OnceCallback<void(apps::mojom::Publisher::LoadIconCallback)>
      fallback_callback_;

  base::CancelableTaskTracker cancelable_task_tracker_;
};

void IconLoadingPipeline::LoadWebAppIcon(
    const std::string& web_app_id,
    const GURL& launch_url,
    const web_app::AppIconManager& icon_manager,
    Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const int icon_size_in_px = apps_util::ConvertDipToPx(
      size_hint_in_dip_, /*quantize_to_supported_scale_factor=*/true);

  fallback_favicon_url_ = launch_url;
  profile_ = profile;
  if (icon_manager.HasSmallestIcon(web_app_id, icon_size_in_px)) {
    switch (icon_compression_) {
      case apps::mojom::IconCompression::kCompressed:
        if (icon_effects_ == apps::IconEffects::kNone) {
          icon_manager.ReadSmallestCompressedIcon(
              web_app_id, icon_size_in_px,
              base::BindOnce(&IconLoadingPipeline::CompleteWithCompressed,
                             base::WrapRefCounted(this)));
          return;
        }
        FALLTHROUGH;
      case apps::mojom::IconCompression::kUncompressed:
        // If |icon_effects| are requested, we must always load the
        // uncompressed image to apply the icon effects, and then re-encode the
        // image if the compressed icon is requested.
        icon_manager.ReadSmallestIcon(
            web_app_id, icon_size_in_px,
            SkBitmapToImageSkiaCallback(base::BindOnce(
                &IconLoadingPipeline::MaybeApplyEffectsAndComplete,
                base::WrapRefCounted(this))));
        return;
      case apps::mojom::IconCompression::kUnknown:
        break;
    }
  }
  MaybeLoadFallbackOrCompleteEmpty();
}

void IconLoadingPipeline::LoadExtensionIcon(
    const extensions::Extension* extension,
    content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!extension) {
    MaybeLoadFallbackOrCompleteEmpty();
    return;
  }

  fallback_favicon_url_ =
      extensions::AppLaunchInfo::GetFullLaunchURL(extension);
  profile_ = Profile::FromBrowserContext(context);
  switch (icon_compression_) {
    case apps::mojom::IconCompression::kCompressed:
      // For compressed icons with no |icon_effects|, serve the
      // already-compressed bytes.
      if (icon_effects_ == apps::IconEffects::kNone) {
        // For the kUncompressed case, RunCallbackWithUncompressedImage
        // calls extensions::ImageLoader::LoadImageAtEveryScaleFactorAsync,
        // which already handles that distinction. We can't use
        // LoadImageAtEveryScaleFactorAsync here, because the caller has asked
        // for compressed icons (i.e. PNG-formatted data), not uncompressed
        // (i.e. a gfx::ImageSkia).
        constexpr bool quantize_to_supported_scale_factor = true;
        int size_hint_in_px = apps_util::ConvertDipToPx(
            size_hint_in_dip_, quantize_to_supported_scale_factor);
        LoadCompressedDataFromExtension(
            extension, size_hint_in_px,
            base::BindOnce(&IconLoadingPipeline::CompleteWithCompressed,
                           base::WrapRefCounted(this)));
        return;
      }
      FALLTHROUGH;
    case apps::mojom::IconCompression::kUncompressed:
      // If |icon_effects| are requested, we must always load the
      // uncompressed image to apply the icon effects, and then re-encode
      // the image if the compressed icon is requested.
      extensions::ImageLoader::Get(context)->LoadImageAtEveryScaleFactorAsync(
          extension, gfx::Size(size_hint_in_dip_, size_hint_in_dip_),
          ImageToImageSkia(
              base::BindOnce(&IconLoadingPipeline::MaybeApplyEffectsAndComplete,
                             base::WrapRefCounted(this))));
      return;
    case apps::mojom::IconCompression::kUnknown:
      break;
  }

  MaybeLoadFallbackOrCompleteEmpty();
}

void IconLoadingPipeline::LoadCompressedIconFromFile(
    const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadFileAsCompressedData, path),
      CompressedDataToImageSkiaCallback(
          base::BindOnce(&IconLoadingPipeline::MaybeApplyEffectsAndComplete,
                         base::WrapRefCounted(this))));
}

void IconLoadingPipeline::LoadIconFromResource(int icon_resource) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (icon_resource == kInvalidIconResource) {
    MaybeLoadFallbackOrCompleteEmpty();
    return;
  }

  switch (icon_compression_) {
    case apps::mojom::IconCompression::kCompressed:
      // For compressed icons with no |icon_effects|, serve the
      // already-compressed bytes.
      if (icon_effects_ == apps::IconEffects::kNone) {
        base::StringPiece data =
            ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
                icon_resource);
        CompleteWithCompressed(std::vector<uint8_t>(data.begin(), data.end()));
        return;
      }
      FALLTHROUGH;
    case apps::mojom::IconCompression::kUncompressed: {
      // For compressed icons with |icon_effects|, or for uncompressed
      // icons, we load the uncompressed image, apply the icon effects, and
      // then re-encode the image if necessary.

      // Get the ImageSkia for the resource. The ui::ResourceBundle shared
      // instance already caches ImageSkia's, but caches the unscaled
      // versions. The |cache| here caches scaled versions, keyed by the
      // pair (resource_id, size_hint_in_dip).
      gfx::ImageSkia scaled;
      std::map<std::pair<int, int>, gfx::ImageSkia>& cache =
          GetResourceIconCache();
      const auto cache_key = std::make_pair(icon_resource, size_hint_in_dip_);
      const auto cache_iter = cache.find(cache_key);
      if (cache_iter != cache.end()) {
        scaled = cache_iter->second;
      } else {
        gfx::ImageSkia* unscaled =
            ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                icon_resource);
        scaled = gfx::ImageSkiaOperations::CreateResizedImage(
            *unscaled, skia::ImageOperations::RESIZE_BEST,
            gfx::Size(size_hint_in_dip_, size_hint_in_dip_));
        cache.insert(std::make_pair(cache_key, scaled));
      }

      // Apply icon effects, re-encode if necessary and run the callback.
      MaybeApplyEffectsAndComplete(scaled);
      return;
    }
    case apps::mojom::IconCompression::kUnknown:
      break;
  }
  MaybeLoadFallbackOrCompleteEmpty();
}

void IconLoadingPipeline::MaybeApplyEffectsAndComplete(
    const gfx::ImageSkia image) {
  if (image.isNull()) {
    MaybeLoadFallbackOrCompleteEmpty();
    return;
  }
  gfx::ImageSkia processed_image = image;

  // Apply the icon effects on the uncompressed data. If the caller requests
  // an uncompressed icon, return the uncompressed result; otherwise, encode
  // the icon to a compressed icon, return the compressed result.
  if (icon_effects_) {
    apps::ApplyIconEffects(icon_effects_, size_hint_in_dip_, &processed_image);
  }

  if (icon_compression_ == apps::mojom::IconCompression::kUncompressed) {
    CompleteWithUncompressed(processed_image);
    return;
  }

  processed_image.MakeThreadSafe();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&EncodeImage, processed_image),
      base::BindOnce(&IconLoadingPipeline::CompleteWithCompressed,
                     base::WrapRefCounted(this)));
}

void IconLoadingPipeline::CompleteWithCompressed(std::vector<uint8_t> data) {
  DCHECK_EQ(icon_compression_, apps::mojom::IconCompression::kCompressed);
  if (data.empty()) {
    MaybeLoadFallbackOrCompleteEmpty();
    return;
  }
  apps::mojom::IconValuePtr iv = apps::mojom::IconValue::New();
  iv->icon_compression = apps::mojom::IconCompression::kCompressed;
  iv->compressed = std::move(data);
  iv->is_placeholder_icon = is_placeholder_icon_;
  std::move(callback_).Run(std::move(iv));
}

void IconLoadingPipeline::CompleteWithUncompressed(gfx::ImageSkia image) {
  DCHECK_EQ(icon_compression_, apps::mojom::IconCompression::kUncompressed);
  if (image.isNull()) {
    MaybeLoadFallbackOrCompleteEmpty();
    return;
  }
  apps::mojom::IconValuePtr iv = apps::mojom::IconValue::New();
  iv->icon_compression = apps::mojom::IconCompression::kUncompressed;
  iv->uncompressed = std::move(image);
  iv->is_placeholder_icon = is_placeholder_icon_;
  std::move(callback_).Run(std::move(iv));
}

void IconLoadingPipeline::MaybeLoadFallbackOrCompleteEmpty() {
  const int icon_size_in_px = apps_util::ConvertDipToPx(
      size_hint_in_dip_, /*quantize_to_supported_scale_factor=*/true);
  if (fallback_favicon_url_.is_valid() &&
      icon_size_in_px == kFaviconFallbackImagePx) {
    GURL favicon_url = fallback_favicon_url_;
    // Reset to avoid infinite loops.
    fallback_favicon_url_ = GURL();
    favicon::FaviconService* favicon_service =
        FaviconServiceFactory::GetForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS);
    if (favicon_service) {
      favicon_service->GetRawFaviconForPageURL(
          favicon_url, {favicon_base::IconType::kFavicon}, gfx::kFaviconSize,
          /*fallback_to_host=*/false,
          FaviconResultToImageSkia(
              base::BindOnce(&IconLoadingPipeline::MaybeApplyEffectsAndComplete,
                             base::WrapRefCounted(this))),
          &cancelable_task_tracker_);
      return;
    }
  }

  if (fallback_callback_) {
    // Wrap the result of |fallback_callback_| in another callback instead of
    // passing it to |callback_| directly so we can catch failures and try other
    // things.
    apps::mojom::Publisher::LoadIconCallback fallback_adaptor = base::BindOnce(
        [](scoped_refptr<IconLoadingPipeline> pipeline,
           apps::mojom::IconValuePtr ptr) {
          if (!ptr.is_null()) {
            std::move(pipeline->callback_).Run(std::move(ptr));
          } else {
            pipeline->MaybeLoadFallbackOrCompleteEmpty();
          }
        },
        base::WrapRefCounted(this));

    // Wrap this to ensure the fallback callback doesn't forget to call it.
    fallback_adaptor = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
        std::move(fallback_adaptor), nullptr);

    std::move(fallback_callback_).Run(std::move(fallback_adaptor));
    // |fallback_callback_| is null at this point, so if we get reinvoked then
    // we won't try this fallback again.

    return;
  }

  if (fallback_icon_resource_ != kInvalidIconResource) {
    int icon_resource = fallback_icon_resource_;
    // Resetting default icon resource to ensure no infinite loops.
    fallback_icon_resource_ = kInvalidIconResource;
    LoadIconFromResource(icon_resource);
    return;
  }

  std::move(callback_).Run(apps::mojom::IconValue::New());
}

}  // namespace

namespace apps {

void ApplyIconEffects(IconEffects icon_effects,
                      int size_hint_in_dip,
                      gfx::ImageSkia* image_skia) {
  extensions::ChromeAppIcon::ResizeFunction resize_function;
#if defined(OS_CHROMEOS)
  if (icon_effects & IconEffects::kResizeAndPad) {
    // TODO(crbug.com/826982): MD post-processing is not always applied: "See
    // legacy code:
    // https://cs.chromium.org/search/?q=ChromeAppIconLoader&type=cs In one
    // cases MD design is used in another not."
    resize_function =
        base::BindRepeating(&app_list::MaybeResizeAndPadIconForMd);
  }
#endif

  const bool from_bookmark = icon_effects & IconEffects::kRoundCorners;

  bool app_launchable = true;
  // Only one badge can be visible at a time.
  // Priority in which badges are applied (from the highest): Blocked > Paused >
  // Chrome. This means than when apps are disabled or paused app type
  // distinction information (Chrome vs Android) is lost.
  extensions::ChromeAppIcon::Badge badge_type =
      extensions::ChromeAppIcon::Badge::kNone;
  if (icon_effects & IconEffects::kBlocked) {
    badge_type = extensions::ChromeAppIcon::Badge::kBlocked;
    app_launchable = false;
  } else if (icon_effects & IconEffects::kPaused) {
    badge_type = extensions::ChromeAppIcon::Badge::kPaused;
    app_launchable = false;
  } else if (icon_effects & IconEffects::kChromeBadge) {
    badge_type = extensions::ChromeAppIcon::Badge::kChrome;
  }

  extensions::ChromeAppIcon::ApplyEffects(size_hint_in_dip, resize_function,
                                          app_launchable, from_bookmark,
                                          badge_type, image_skia);

  if (icon_effects & IconEffects::kPendingLocalLaunch) {
    color_utils::HSL shift = {-1, 0, 0.6};
    *image_skia =
        gfx::ImageSkiaOperations::CreateHSLShiftedImage(*image_skia, shift);
  }
}

void LoadIconFromExtension(apps::mojom::IconCompression icon_compression,
                           int size_hint_in_dip,
                           content::BrowserContext* context,
                           const std::string& extension_id,
                           IconEffects icon_effects,
                           apps::mojom::Publisher::LoadIconCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  constexpr bool is_placeholder_icon = false;
  scoped_refptr<IconLoadingPipeline> icon_loader =
      base::MakeRefCounted<IconLoadingPipeline>(
          icon_compression, size_hint_in_dip, is_placeholder_icon, icon_effects,
          IDR_APP_DEFAULT_ICON, std::move(callback));
  icon_loader->LoadExtensionIcon(
      extensions::ExtensionRegistry::Get(context)->GetInstalledExtension(
          extension_id),
      context);
}

void LoadIconFromWebApp(content::BrowserContext* context,
                        apps::mojom::IconCompression icon_compression,
                        int size_hint_in_dip,
                        const std::string& web_app_id,
                        IconEffects icon_effects,
                        apps::mojom::Publisher::LoadIconCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(context);
  web_app::WebAppProvider* web_app_provider =
      web_app::WebAppProvider::Get(Profile::FromBrowserContext(context));

  DCHECK(web_app_provider);
  constexpr bool is_placeholder_icon = false;
  scoped_refptr<IconLoadingPipeline> icon_loader =
      base::MakeRefCounted<IconLoadingPipeline>(
          icon_compression, size_hint_in_dip, is_placeholder_icon, icon_effects,
          IDR_APP_DEFAULT_ICON, std::move(callback));
  icon_loader->LoadWebAppIcon(
      web_app_id, web_app_provider->registrar().GetAppLaunchURL(web_app_id),
      web_app_provider->icon_manager(), Profile::FromBrowserContext(context));
}

void LoadIconFromFileWithFallback(
    apps::mojom::IconCompression icon_compression,
    int size_hint_in_dip,
    const base::FilePath& path,
    IconEffects icon_effects,
    apps::mojom::Publisher::LoadIconCallback callback,
    base::OnceCallback<void(apps::mojom::Publisher::LoadIconCallback)>
        fallback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  constexpr bool is_placeholder_icon = false;

  scoped_refptr<IconLoadingPipeline> icon_loader =
      base::MakeRefCounted<IconLoadingPipeline>(
          icon_compression, size_hint_in_dip, is_placeholder_icon, icon_effects,
          IDR_APP_DEFAULT_ICON, std::move(fallback), std::move(callback));
  icon_loader->LoadCompressedIconFromFile(path);
}

void LoadIconFromResource(apps::mojom::IconCompression icon_compression,
                          int size_hint_in_dip,
                          int resource_id,
                          bool is_placeholder_icon,
                          IconEffects icon_effects,
                          apps::mojom::Publisher::LoadIconCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // There is no fallback icon for a resource.
  constexpr int fallback_icon_resource = 0;

  scoped_refptr<IconLoadingPipeline> icon_loader =
      base::MakeRefCounted<IconLoadingPipeline>(
          icon_compression, size_hint_in_dip, is_placeholder_icon, icon_effects,
          fallback_icon_resource, std::move(callback));
  icon_loader->LoadIconFromResource(resource_id);
}

}  // namespace apps
