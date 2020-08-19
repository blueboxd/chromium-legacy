// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/common/file_icon_util.h"

#include <string>
#include <utility>

#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/file_manager/file_manager_resource_util.h"
#include "ui/file_manager/grit/file_manager_resources.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {

// Hex color: #796EEE
constexpr SkColor kFiletypeGsiteColor = SkColorSetRGB(121, 110, 238);

// Hex color: #FF7537
constexpr SkColor kFiletypePptColor = SkColorSetRGB(255, 117, 55);

// Hex color: #796EEE
constexpr SkColor kFiletypeSitesColor = SkColorSetRGB(121, 110, 238);

constexpr SkColor kWhiteBackgroundColor = SkColorSetRGB(255, 255, 255);

constexpr int kIconDipSize = 20;

}  // namespace

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

IconType GetIconTypeFromString(const std::string& icon_type_string) {
  static const base::NoDestructor<std::map<std::string, IconType>>
      type_string_to_icon_type({{"archive", IconType::ARCHIVE},
                                {"audio", IconType::AUDIO},
                                {"chart", IconType::CHART},
                                {"excel", IconType::EXCEL},
                                {"drive", IconType::DRIVE},
                                {"folder", IconType::FOLDER},
                                {"gdoc", IconType::GDOC},
                                {"gdraw", IconType::GDRAW},
                                {"generic", IconType::GENERIC},
                                {"gform", IconType::GFORM},
                                {"gmap", IconType::GMAP},
                                {"gsheet", IconType::GSHEET},
                                {"gsite", IconType::GSITE},
                                {"gslides", IconType::GSLIDE},
                                {"gtable", IconType::GTABLE},
                                {"image", IconType::IMAGE},
                                {"linux", IconType::LINUX},
                                {"pdf", IconType::PDF},
                                {"ppt", IconType::PPT},
                                {"script", IconType::SCRIPT},
                                {"shared", IconType::FOLDER_SHARED},
                                {"sites", IconType::SITES},
                                {"tini", IconType::TINI},
                                {"video", IconType::VIDEO},
                                {"word", IconType::WORD}});

  const auto& icon_it = type_string_to_icon_type->find(icon_type_string);
  if (icon_it != type_string_to_icon_type->end())
    return icon_it->second;
  return IconType::GENERIC;
}

gfx::ImageSkia GetVectorIconFromIconType(IconType icon, bool is_chip_icon) {
  // Changes to this map should be reflected in
  // ui/file_manager/file_manager/common/js/file_type.js.
  static const base::NoDestructor<std::map<IconType, gfx::IconDescription>>
      icon_type_to_icon_description(
          {{IconType::ARCHIVE,
            gfx::IconDescription(ash::kFiletypeArchiveIcon, kIconDipSize,
                                 gfx::kGoogleGrey700)},
           {IconType::AUDIO,
            gfx::IconDescription(ash::kFiletypeAudioIcon, kIconDipSize,
                                 gfx::kGoogleRed500)},
           {IconType::CHART,
            gfx::IconDescription(ash::kFiletypeChartIcon, kIconDipSize,
                                 gfx::kGoogleGreen500)},
           {IconType::DRIVE,
            gfx::IconDescription(ash::kFiletypeTeamDriveIcon, kIconDipSize,
                                 gfx::kGoogleGrey700)},
           {IconType::EXCEL,
            gfx::IconDescription(ash::kFiletypeExcelIcon, kIconDipSize,
                                 gfx::kGoogleGreen500)},
           {IconType::FOLDER,
            gfx::IconDescription(ash::kFiletypeFolderIcon, kIconDipSize,
                                 gfx::kGoogleGrey700)},
           {IconType::FOLDER_SHARED,
            gfx::IconDescription(ash::kFiletypeSharedIcon, kIconDipSize,
                                 gfx::kGoogleGrey700)},
           {IconType::GDOC,
            gfx::IconDescription(ash::kFiletypeGdocIcon, kIconDipSize,
                                 gfx::kGoogleBlue500)},
           {IconType::GDRAW,
            gfx::IconDescription(ash::kFiletypeGdrawIcon, kIconDipSize,
                                 gfx::kGoogleRed500)},
           {IconType::GENERIC,
            gfx::IconDescription(ash::kFiletypeGenericIcon, kIconDipSize,
                                 gfx::kGoogleGrey700)},
           {IconType::GFORM,
            gfx::IconDescription(ash::kFiletypeGformIcon, kIconDipSize,
                                 gfx::kGoogleGreen500)},
           {IconType::GMAP,
            gfx::IconDescription(ash::kFiletypeGmapIcon, kIconDipSize,
                                 gfx::kGoogleRed500)},
           {IconType::GSHEET,
            gfx::IconDescription(ash::kFiletypeGsheetIcon, kIconDipSize,
                                 gfx::kGoogleGreen500)},
           {IconType::GSITE,
            gfx::IconDescription(ash::kFiletypeGsiteIcon, kIconDipSize,
                                 kFiletypeGsiteColor)},
           {IconType::GSLIDE,
            gfx::IconDescription(ash::kFiletypeGslidesIcon, kIconDipSize,
                                 gfx::kGoogleYellow500)},
           {IconType::GTABLE,
            gfx::IconDescription(ash::kFiletypeGtableIcon, kIconDipSize,
                                 gfx::kGoogleGreen500)},
           {IconType::IMAGE,
            gfx::IconDescription(ash::kFiletypeImageIcon, kIconDipSize,
                                 gfx::kGoogleRed500)},
           {IconType::LINUX,
            gfx::IconDescription(ash::kFiletypeLinuxIcon, kIconDipSize,
                                 gfx::kGoogleGrey700)},
           {IconType::PDF,
            gfx::IconDescription(ash::kFiletypePdfIcon, kIconDipSize,
                                 gfx::kGoogleRed500)},
           {IconType::PPT,
            gfx::IconDescription(ash::kFiletypePptIcon, kIconDipSize,
                                 kFiletypePptColor)},
           {IconType::SCRIPT,
            gfx::IconDescription(ash::kFiletypeScriptIcon, kIconDipSize,
                                 gfx::kGoogleBlue500)},
           {IconType::SITES,
            gfx::IconDescription(ash::kFiletypeSitesIcon, kIconDipSize,
                                 kFiletypeSitesColor)},
           {IconType::TINI,
            gfx::IconDescription(ash::kFiletypeTiniIcon, kIconDipSize,
                                 gfx::kGoogleBlue500)},
           {IconType::VIDEO,
            gfx::IconDescription(ash::kFiletypeVideoIcon, kIconDipSize,
                                 gfx::kGoogleRed500)},
           {IconType::WORD,
            gfx::IconDescription(ash::kFiletypeWordIcon, kIconDipSize,
                                 gfx::kGoogleBlue500)}});

  const auto& id_it = icon_type_to_icon_description->find(icon);
  DCHECK(id_it != icon_type_to_icon_description->end());

  // If it is a launcher chip icon, we need to draw 2 icons: a white circle
  // background icon (kFiletypeChipBackgroundIcon) and the icon of the file.
  if (is_chip_icon) {
    return gfx::ImageSkiaOperations::CreateSuperimposedImage(
        gfx::CreateVectorIcon(ash::kFiletypeChipBackgroundIcon, kIconDipSize,
                              kWhiteBackgroundColor),
        gfx::CreateVectorIcon(id_it->second));
  }
  return gfx::CreateVectorIcon(id_it->second);
}

}  // namespace internal

gfx::ImageSkia GetIconForPath(const base::FilePath& filepath) {
  return internal::GetVectorIconFromIconType(
      internal::GetIconTypeForPath(filepath));
}

gfx::ImageSkia GetChipIconForPath(const base::FilePath& filepath) {
  return internal::GetVectorIconFromIconType(
      internal::GetIconTypeForPath(filepath), /*is_chip_icon=*/true);
}

gfx::ImageSkia GetIconFromType(const std::string& icon_type) {
  return GetVectorIconFromIconType(internal::GetIconTypeFromString(icon_type));
}

}  // namespace app_list
