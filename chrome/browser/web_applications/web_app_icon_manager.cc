// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_icon_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/browser_thread.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

namespace web_app {

namespace {

constexpr base::FilePath::CharType kTempDirectoryName[] =
    FILE_PATH_LITERAL("Temp");

constexpr base::FilePath::CharType kIconsDirectoryName[] =
    FILE_PATH_LITERAL("Icons");

base::FilePath GetAppDirectory(const base::FilePath& web_apps_directory,
                               const AppId& app_id) {
  return web_apps_directory.AppendASCII(app_id);
}

base::FilePath GetTempDir(FileUtilsWrapper* utils,
                          const base::FilePath& web_apps_dir) {
  // Create the temp directory as a sub-directory of the WebApps directory.
  // This guarantees it is on the same file system as the WebApp's eventual
  // install target.
  base::FilePath temp_path = web_apps_dir.Append(kTempDirectoryName);
  if (utils->PathExists(temp_path)) {
    if (!utils->DirectoryExists(temp_path)) {
      LOG(ERROR) << "Not a directory: " << temp_path.value();
      return base::FilePath();
    }
    if (!utils->PathIsWritable(temp_path)) {
      LOG(ERROR) << "Can't write to path: " << temp_path.value();
      return base::FilePath();
    }
    // This is a directory we can write to.
    return temp_path;
  }

  // Directory doesn't exist, so create it.
  if (!utils->CreateDirectory(temp_path)) {
    LOG(ERROR) << "Could not create directory: " << temp_path.value();
    return base::FilePath();
  }
  return temp_path;
}

bool WriteIcon(FileUtilsWrapper* utils,
               const base::FilePath& icons_dir,
               const SkBitmap& bitmap) {
  DCHECK_NE(bitmap.colorType(), kUnknown_SkColorType);
  DCHECK_EQ(bitmap.width(), bitmap.height());
  base::FilePath icon_file =
      icons_dir.AppendASCII(base::StringPrintf("%i.png", bitmap.width()));

  std::vector<unsigned char> image_data;
  const bool discard_transparency = false;
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, discard_transparency,
                                         &image_data)) {
    LOG(ERROR) << "Could not encode icon data for file " << icon_file;
    return false;
  }

  const char* image_data_ptr = reinterpret_cast<const char*>(&image_data[0]);
  int size = base::checked_cast<int>(image_data.size());
  if (utils->WriteFile(icon_file, image_data_ptr, size) != size) {
    LOG(ERROR) << "Could not write icon file: " << icon_file;
    return false;
  }

  return true;
}

bool WriteIcons(FileUtilsWrapper* utils,
                const base::FilePath& app_dir,
                const std::map<SquareSizePx, SkBitmap>& icon_bitmaps) {
  const base::FilePath icons_dir = app_dir.Append(kIconsDirectoryName);
  if (!utils->CreateDirectory(icons_dir)) {
    LOG(ERROR) << "Could not create icons directory.";
    return false;
  }

  for (const std::pair<SquareSizePx, SkBitmap>& icon_bitmap : icon_bitmaps) {
    if (!WriteIcon(utils, icons_dir, icon_bitmap.second))
      return false;
  }

  return true;
}

// Performs blocking I/O. May be called on another thread.
// Returns true if no errors occurred.
bool WriteDataBlocking(const std::unique_ptr<FileUtilsWrapper>& utils,
                       const base::FilePath& web_apps_directory,
                       const AppId& app_id,
                       const std::map<SquareSizePx, SkBitmap>& icons) {
  const base::FilePath temp_dir = GetTempDir(utils.get(), web_apps_directory);
  if (temp_dir.empty()) {
    LOG(ERROR)
        << "Could not get path to WebApps temporary directory in profile.";
    return false;
  }

  base::ScopedTempDir app_temp_dir;
  if (!app_temp_dir.CreateUniqueTempDirUnderPath(temp_dir)) {
    LOG(ERROR) << "Could not create temporary WebApp directory.";
    return false;
  }

  if (!WriteIcons(utils.get(), app_temp_dir.GetPath(), icons))
    return false;

  // Commit: move whole app data dir to final destination in one mv operation.
  const base::FilePath app_dir = GetAppDirectory(web_apps_directory, app_id);

  // Try to delete the destination. Needed for update.
  utils->DeleteFileRecursively(app_dir);

  if (!utils->Move(app_temp_dir.GetPath(), app_dir)) {
    LOG(ERROR) << "Could not move temp WebApp directory to final destination.";
    return false;
  }

  app_temp_dir.Take();
  return true;
}

// Performs blocking I/O. May be called on another thread.
// Returns true if no errors occurred.
bool DeleteDataBlocking(const std::unique_ptr<FileUtilsWrapper>& utils,
                        const base::FilePath& web_apps_directory,
                        const AppId& app_id) {
  const base::FilePath app_dir = GetAppDirectory(web_apps_directory, app_id);
  return utils->DeleteFileRecursively(app_dir);
}

base::FilePath GetIconFileName(const base::FilePath& web_apps_directory,
                               const AppId& app_id,
                               int icon_size_px) {
  const base::FilePath app_dir = GetAppDirectory(web_apps_directory, app_id);
  const base::FilePath icons_dir = app_dir.Append(kIconsDirectoryName);

  return icons_dir.AppendASCII(base::StringPrintf("%i.png", icon_size_px));
}

// Performs blocking I/O. May be called on another thread.
// Returns empty SkBitmap if any errors occurred.
SkBitmap ReadIconBlocking(const std::unique_ptr<FileUtilsWrapper>& utils,
                          const base::FilePath& web_apps_directory,
                          const AppId& app_id,
                          int icon_size_px) {
  base::FilePath icon_file =
      GetIconFileName(web_apps_directory, app_id, icon_size_px);

  auto icon_data = base::MakeRefCounted<base::RefCountedString>();

  if (!utils->ReadFileToString(icon_file, &icon_data->data())) {
    LOG(ERROR) << "Could not read icon file: " << icon_file;
    return SkBitmap();
  }

  SkBitmap bitmap;

  if (!gfx::PNGCodec::Decode(icon_data->front(), icon_data->size(), &bitmap)) {
    LOG(ERROR) << "Could not decode icon data for file " << icon_file;
    return SkBitmap();
  }

  return bitmap;
}

// Performs blocking I/O. May be called on another thread.
// Returns empty map if any errors occurred.
std::map<SquareSizePx, SkBitmap> ReadIconAndResizeBlocking(
    const std::unique_ptr<FileUtilsWrapper>& utils,
    const base::FilePath& web_apps_directory,
    const AppId& app_id,
    SquareSizePx source_icon_size_px,
    SquareSizePx target_icon_size_px) {
  std::map<SquareSizePx, SkBitmap> result;

  SkBitmap source =
      ReadIconBlocking(utils, web_apps_directory, app_id, source_icon_size_px);
  if (source.empty())
    return result;

  SkBitmap target;

  if (source_icon_size_px != target_icon_size_px) {
    target = skia::ImageOperations::Resize(
        source, skia::ImageOperations::RESIZE_BEST, target_icon_size_px,
        target_icon_size_px);
  } else {
    target = source;
  }

  result[target_icon_size_px] = target;
  return result;
}

// Performs blocking I/O. May be called on another thread.
std::map<SquareSizePx, SkBitmap> ReadIconsBlocking(
    const std::unique_ptr<FileUtilsWrapper>& utils,
    const base::FilePath& web_apps_directory,
    const AppId& app_id,
    const std::vector<SquareSizePx>& icon_sizes) {
  std::map<SquareSizePx, SkBitmap> result;

  for (SquareSizePx icon_size_px : icon_sizes) {
    SkBitmap bitmap =
        ReadIconBlocking(utils, web_apps_directory, app_id, icon_size_px);
    if (!bitmap.empty())
      result[icon_size_px] = bitmap;
  }

  return result;
}

// Performs blocking I/O. May be called on another thread.
// Returns empty vector if any errors occurred.
std::vector<uint8_t> ReadCompressedIconBlocking(
    const std::unique_ptr<FileUtilsWrapper>& utils,
    const base::FilePath& web_apps_directory,
    const AppId& app_id,
    SquareSizePx icon_size_px) {
  base::FilePath icon_file =
      GetIconFileName(web_apps_directory, app_id, icon_size_px);

  std::string icon_data;

  if (!utils->ReadFileToString(icon_file, &icon_data)) {
    LOG(ERROR) << "Could not read icon file: " << icon_file;
    return std::vector<uint8_t>{};
  }

  // Copy data: we can't std::move std::string into std::vector.
  return std::vector<uint8_t>(icon_data.begin(), icon_data.end());
}

constexpr base::TaskTraits kTaskTraits = {
    base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE,
    base::TaskShutdownBehavior::BLOCK_SHUTDOWN};

}  // namespace

WebAppIconManager::WebAppIconManager(Profile* profile,
                                     WebAppRegistrar& registrar,
                                     std::unique_ptr<FileUtilsWrapper> utils)
    : registrar_(registrar), utils_(std::move(utils)) {
  web_apps_directory_ = GetWebAppsDirectory(profile);
}

WebAppIconManager::~WebAppIconManager() = default;

void WebAppIconManager::WriteData(AppId app_id,
                                  std::map<SquareSizePx, SkBitmap> icons,
                                  WriteDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(WriteDataBlocking, utils_->Clone(), web_apps_directory_,
                     std::move(app_id), std::move(icons)),
      std::move(callback));
}

void WebAppIconManager::DeleteData(AppId app_id, WriteDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(DeleteDataBlocking, utils_->Clone(), web_apps_directory_,
                     std::move(app_id)),
      std::move(callback));
}

bool WebAppIconManager::HasIcons(
    const AppId& app_id,
    const std::vector<SquareSizePx>& icon_sizes_in_px) const {
  const WebApp* web_app = registrar_.GetAppById(app_id);
  if (!web_app)
    return false;

  return base::STLIncludes(web_app->downloaded_icon_sizes(), icon_sizes_in_px);
}

bool WebAppIconManager::HasSmallestIcon(const AppId& app_id,
                                        SquareSizePx icon_size_in_px) const {
  return FindDownloadedSizeInPxMatchBigger(app_id, icon_size_in_px).has_value();
}

void WebAppIconManager::ReadIcons(
    const AppId& app_id,
    const std::vector<SquareSizePx>& icon_sizes_in_px,
    ReadIconsCallback callback) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(HasIcons(app_id, icon_sizes_in_px));

  base::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(ReadIconsBlocking, utils_->Clone(), web_apps_directory_,
                     app_id, icon_sizes_in_px),
      std::move(callback));
}

void WebAppIconManager::ReadAllIcons(const AppId& app_id,
                                     ReadIconsCallback callback) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const WebApp* web_app = registrar_.GetAppById(app_id);
  if (!web_app) {
    std::move(callback).Run(std::map<SquareSizePx, SkBitmap>());
    return;
  }

  base::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(ReadIconsBlocking, utils_->Clone(), web_apps_directory_,
                     app_id, web_app->downloaded_icon_sizes()),
      std::move(callback));
}

void WebAppIconManager::ReadSmallestIcon(const AppId& app_id,
                                         SquareSizePx icon_size_in_px,
                                         ReadIconCallback callback) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::Optional<SquareSizePx> best_size_in_px =
      FindDownloadedSizeInPxMatchBigger(app_id, icon_size_in_px);
  DCHECK(best_size_in_px.has_value());

  base::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(ReadIconBlocking, utils_->Clone(), web_apps_directory_,
                     app_id, best_size_in_px.value()),
      std::move(callback));
}

void WebAppIconManager::ReadSmallestCompressedIcon(
    const AppId& app_id,
    SquareSizePx icon_size_in_px,
    ReadCompressedIconCallback callback) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::Optional<SquareSizePx> best_size_in_px =
      FindDownloadedSizeInPxMatchBigger(app_id, icon_size_in_px);
  DCHECK(best_size_in_px.has_value());

  base::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(ReadCompressedIconBlocking, utils_->Clone(),
                     web_apps_directory_, app_id, best_size_in_px.value()),
      std::move(callback));
}

bool WebAppIconManager::HasIconToResize(const AppId& app_id,
                                        SquareSizePx desired_icon_size) const {
  return FindDownloadedSizeInPxMatchBigger(app_id, desired_icon_size)
             .has_value() ||
         FindDownloadedSizeInPxMatchSmaller(app_id, desired_icon_size)
             .has_value();
}

void WebAppIconManager::ReadIconAndResize(const AppId& app_id,
                                          SquareSizePx desired_icon_size,
                                          ReadIconsCallback callback) const {
  DCHECK(HasIconToResize(app_id, desired_icon_size));

  base::Optional<SquareSizePx> best_downloaded_size =
      FindDownloadedSizeInPxMatchBigger(app_id, desired_icon_size);
  if (!best_downloaded_size) {
    best_downloaded_size =
        FindDownloadedSizeInPxMatchSmaller(app_id, desired_icon_size);
  }

  DCHECK(best_downloaded_size.has_value());

  base::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(ReadIconAndResizeBlocking, utils_->Clone(),
                     web_apps_directory_, app_id, best_downloaded_size.value(),
                     desired_icon_size),
      std::move(callback));
}

base::Optional<SquareSizePx>
WebAppIconManager::FindDownloadedSizeInPxMatchBigger(
    const AppId& app_id,
    SquareSizePx desired_size) const {
  const WebApp* web_app = registrar_.GetAppById(app_id);
  if (!web_app)
    return base::nullopt;

  DCHECK(base::STLIsSorted(web_app->downloaded_icon_sizes()));
  for (auto it = web_app->downloaded_icon_sizes().begin();
       it != web_app->downloaded_icon_sizes().end(); ++it) {
    if (*it >= desired_size)
      return base::make_optional(*it);
  }

  return base::nullopt;
}

base::Optional<SquareSizePx>
WebAppIconManager::FindDownloadedSizeInPxMatchSmaller(
    const AppId& app_id,
    SquareSizePx desired_size) const {
  const WebApp* web_app = registrar_.GetAppById(app_id);
  if (!web_app)
    return base::nullopt;

  DCHECK(base::STLIsSorted(web_app->downloaded_icon_sizes()));
  for (auto it = web_app->downloaded_icon_sizes().rbegin();
       it != web_app->downloaded_icon_sizes().rend(); ++it) {
    if (*it <= desired_size)
      return base::make_optional(*it);
  }

  return base::nullopt;
}

}  // namespace web_app
