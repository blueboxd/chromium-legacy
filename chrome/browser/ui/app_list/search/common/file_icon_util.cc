// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/common/file_icon_util.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/file_manager/file_manager_resource_util.h"
#include "ui/file_manager/grit/file_manager_resources.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

#include "chrome/browser/ui/app_list/search/launcher_search/launcher_search_icon_image_loader.h"

namespace app_list {
namespace internal {

IconType GetIconTypeForPath(const base::FilePath& filepath) {
  static const base::NoDestructor<base::flat_map<std::string, IconType>>
      // Changes to this map should be reflected in
      // ui/file_manager/file_manager/common/js/file_type.js.
      extension_to_icon({
          // Image
          {".JPEG", IconType::IMAGE},
          {".JPG", IconType::IMAGE},
          {".BMP", IconType::IMAGE},
          {".GIF", IconType::IMAGE},
          {".ICO", IconType::IMAGE},
          {".PNG", IconType::IMAGE},
          {".WEBP", IconType::IMAGE},
          {".TIFF", IconType::IMAGE},
          {".TIF", IconType::IMAGE},
          {".SVG", IconType::IMAGE},

          // Raw
          {".ARW", IconType::IMAGE},
          {".CR2", IconType::IMAGE},
          {".DNG", IconType::IMAGE},
          {".NEF", IconType::IMAGE},
          {".NRW", IconType::IMAGE},
          {".ORF", IconType::IMAGE},
          {".RAF", IconType::IMAGE},
          {".RW2", IconType::IMAGE},

          // Video
          {".3GP", IconType::VIDEO},
          {".3GPP", IconType::VIDEO},
          {".AVI", IconType::VIDEO},
          {".MOV", IconType::VIDEO},
          {".MKV", IconType::VIDEO},
          {".MP4", IconType::VIDEO},
          {".M4V", IconType::VIDEO},
          {".MPG", IconType::VIDEO},
          {".MPEG", IconType::VIDEO},
          {".MPG4", IconType::VIDEO},
          {".MPEG4", IconType::VIDEO},
          {".OGM", IconType::VIDEO},
          {".OGV", IconType::VIDEO},
          {".OGX", IconType::VIDEO},
          {".WEBM", IconType::VIDEO},

          // Audio
          {".AMR", IconType::AUDIO},
          {".FLAC", IconType::AUDIO},
          {".MP3", IconType::AUDIO},
          {".M4A", IconType::AUDIO},
          {".OGA", IconType::AUDIO},
          {".OGG", IconType::AUDIO},
          {".WAV", IconType::AUDIO},

          // Text
          {".TXT", IconType::GENERIC},

          // Archive
          {".ZIP", IconType::ARCHIVE},
          {".RAR", IconType::ARCHIVE},
          {".TAR", IconType::ARCHIVE},
          {".TAR.BZ2", IconType::ARCHIVE},
          {".TBZ", IconType::ARCHIVE},
          {".TBZ2", IconType::ARCHIVE},
          {".TAR.GZ", IconType::ARCHIVE},
          {".TGZ", IconType::ARCHIVE},

          // Hosted doc
          {".GDOC", IconType::GDOC},
          {".GSHEET", IconType::GSHEET},
          {".GSLIDES", IconType::GSLIDE},
          {".GDRAW", IconType::GDRAW},
          {".GTABLE", IconType::GTABLE},
          {".GLINK", IconType::GENERIC},
          {".GFORM", IconType::GFORM},
          {".GMAPS", IconType::GMAP},
          {".GSITE", IconType::GSITE},

          // Other
          {".PDF", IconType::PDF},
          {".HTM", IconType::GENERIC},
          {".HTML", IconType::GENERIC},
          {".MHT", IconType::GENERIC},
          {".MHTM", IconType::GENERIC},
          {".MHTML", IconType::GENERIC},
          {".SHTML", IconType::GENERIC},
          {".XHT", IconType::GENERIC},
          {".XHTM", IconType::GENERIC},
          {".XHTML", IconType::GENERIC},
          {".DOC", IconType::WORD},
          {".DOCX", IconType::WORD},
          {".PPT", IconType::PPT},
          {".PPTX", IconType::PPT},
          {".XLS", IconType::EXCEL},
          {".XLSX", IconType::EXCEL},
          {".TINI", IconType::TINI},
      });

  const auto& icon_it =
      extension_to_icon->find(base::ToUpperASCII(filepath.Extension()));
  if (icon_it != extension_to_icon->end()) {
    return icon_it->second;
  } else {
    return IconType::GENERIC;
  }
}

int GetResourceIdForIconType(IconType icon) {
  // Changes to this map should be reflected in
  // ui/file_manager/file_manager/common/js/file_type.js.
  static const base::NoDestructor<base::flat_map<IconType, int>>
      icon_to_2x_resource_id({
          {IconType::ARCHIVE,
           IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_ARCHIVE},
          {IconType::AUDIO, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_AUDIO},
          {IconType::CHART, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_CHART},
          // TODO(crbug.com/1088395):  we're missing a generic square drive
          // file icon.
          {IconType::DRIVE, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_GENERIC},
          {IconType::EXCEL, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_EXCEL},
          {IconType::FOLDER, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_FOLDER},
          // TODO(crbug.com/1088395): we're missing a square shared folder icon.
          {IconType::FOLDER_SHARED,
           IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_FOLDER},
          {IconType::GDOC, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_GDOC},
          {IconType::GDRAW, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_GDRAW},
          {IconType::GENERIC,
           IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_GENERIC},
          {IconType::GFORM, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_GFORM},
          {IconType::GMAP, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_GMAP},
          {IconType::GSHEET, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_GSHEET},
          {IconType::GSITE, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_GSITE},
          {IconType::GSLIDE, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_GSLIDES},
          {IconType::GTABLE, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_GTABLE},
          {IconType::IMAGE, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_IMAGE},
          {IconType::LINUX, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_GENERIC},
          {IconType::PDF, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_PDF},
          {IconType::PPT, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_PPT},
          {IconType::SCRIPT, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_SCRIPT},
          {IconType::SITES, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_SITES},
          {IconType::TINI, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_TINI},
          {IconType::VIDEO, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_VIDEO},
          {IconType::WORD, IDR_FILE_MANAGER_IMG_LAUNCHER_FILETYPE_2X_WORD},
      });

  const auto& id_it = icon_to_2x_resource_id->find(icon);
  DCHECK(id_it != icon_to_2x_resource_id->end());
  return id_it->second;
}

int GetChipResourceIdForIconType(IconType icon) {
  static const base::NoDestructor<base::flat_map<IconType, int>>
      icon_to_chip_resource_id({
          {IconType::ARCHIVE, IDR_LAUNCHER_CHIP_ICON_ARCHIVE},
          {IconType::AUDIO, IDR_LAUNCHER_CHIP_ICON_AUDIO},
          {IconType::CHART, IDR_LAUNCHER_CHIP_ICON_CHART},
          {IconType::DRIVE, IDR_LAUNCHER_CHIP_ICON_DRIVE},
          {IconType::EXCEL, IDR_LAUNCHER_CHIP_ICON_EXCEL},
          {IconType::FOLDER, IDR_LAUNCHER_CHIP_ICON_FOLDER},
          {IconType::FOLDER_SHARED, IDR_LAUNCHER_CHIP_ICON_FOLDER_SHARED},
          {IconType::GDOC, IDR_LAUNCHER_CHIP_ICON_GDOC},
          {IconType::GDRAW, IDR_LAUNCHER_CHIP_ICON_GDRAW},
          {IconType::GENERIC, IDR_LAUNCHER_CHIP_ICON_GENERIC},
          {IconType::GFORM, IDR_LAUNCHER_CHIP_ICON_GFORM},
          {IconType::GMAP, IDR_LAUNCHER_CHIP_ICON_GMAP},
          {IconType::GSHEET, IDR_LAUNCHER_CHIP_ICON_GSHEET},
          {IconType::GSITE, IDR_LAUNCHER_CHIP_ICON_GSITE},
          {IconType::GSLIDE, IDR_LAUNCHER_CHIP_ICON_GSLIDE},
          {IconType::GTABLE, IDR_LAUNCHER_CHIP_ICON_GTABLE},
          {IconType::IMAGE, IDR_LAUNCHER_CHIP_ICON_IMAGE},
          {IconType::LINUX, IDR_LAUNCHER_CHIP_ICON_LINUX},
          {IconType::PDF, IDR_LAUNCHER_CHIP_ICON_PDF},
          {IconType::PPT, IDR_LAUNCHER_CHIP_ICON_PPT},
          {IconType::SCRIPT, IDR_LAUNCHER_CHIP_ICON_SCRIPT},
          {IconType::SITES, IDR_LAUNCHER_CHIP_ICON_SITES},
          {IconType::TINI, IDR_LAUNCHER_CHIP_ICON_TINI},
          {IconType::VIDEO, IDR_LAUNCHER_CHIP_ICON_VIDEO},
          {IconType::WORD, IDR_LAUNCHER_CHIP_ICON_WORD},
      });

  const auto& id_it = icon_to_chip_resource_id->find(icon);
  DCHECK(id_it != icon_to_chip_resource_id->end());
  return id_it->second;
}

}  // namespace internal

gfx::ImageSkia GetIconForPath(const base::FilePath& filepath) {
  return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      internal::GetResourceIdForIconType(
          internal::GetIconTypeForPath(filepath)));
}

gfx::ImageSkia GetChipIconForPath(const base::FilePath& filepath) {
  return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      internal::GetChipResourceIdForIconType(
          internal::GetIconTypeForPath(filepath)));
}

}  // namespace app_list
