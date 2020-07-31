// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_helper.h"

#include <string>
#include <vector>

#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/strings/grit/ui_strings.h"

namespace ash {
namespace clipboard {
namespace helper {

namespace {

constexpr char kFileSystemSourcesType[] = "fs/sources";

// Private ---------------------------------------------------------------------

// Returns true if |data| contains the specified |format|.
bool ContainsFormat(const ui::ClipboardData& data,
                    ui::ClipboardInternalFormat format) {
  return data.format() & static_cast<int>(format);
}

// Returns the localized string for the specified |resource_id|.
base::string16 GetLocalizedString(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
      resource_id);
}

// TODO(crbug/1108902): Handle fallback case.
// Returns the label to display for the custom data contained within |data|.
base::string16 GetLabelForCustomData(const ui::ClipboardData& data) {
  DCHECK(ContainsFormat(data, ui::ClipboardInternalFormat::kCustom));

  // Attempt to read file system sources in the custom data.
  base::string16 sources;
  ui::ReadCustomDataForType(
      data.custom_data_data().c_str(), data.custom_data_data().size(),
      base::UTF8ToUTF16(kFileSystemSourcesType), &sources);

  if (sources.empty())
    return base::UTF8ToUTF16("<Custom Data>");

  // Split sources into a list.
  std::vector<base::StringPiece16> source_list =
      base::SplitStringPiece(sources, base::UTF8ToUTF16("\n"),
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Strip path information, so all that's left are file names.
  for (auto it = source_list.begin(); it != source_list.end(); ++it)
    *it = it->substr(it->find_last_of(base::UTF8ToUTF16("/")) + 1);

  // Join file names, unescaping encoded character sequences for display. This
  // ensures that "My%20File.txt" will display as "My File.txt".
  return base::UTF8ToUTF16(base::UnescapeURLComponent(
      base::UTF16ToUTF8(base::JoinString(source_list, base::UTF8ToUTF16(", "))),
      base::UnescapeRule::SPACES));
}

}  // namespace

// Public ----------------------------------------------------------------------

base::string16 GetLabel(const ui::ClipboardData& data) {
  if (ContainsFormat(data, ui::ClipboardInternalFormat::kBitmap))
    return GetLocalizedString(IDS_CLIPBOARD_MENU_IMAGE);
  if (ContainsFormat(data, ui::ClipboardInternalFormat::kText))
    return base::UTF8ToUTF16(data.text());
  if (ContainsFormat(data, ui::ClipboardInternalFormat::kHtml))
    return base::UTF8ToUTF16(data.markup_data());
  if (ContainsFormat(data, ui::ClipboardInternalFormat::kRtf))
    return GetLocalizedString(IDS_CLIPBOARD_MENU_RTF_CONTENT);
  if (ContainsFormat(data, ui::ClipboardInternalFormat::kBookmark))
    return base::UTF8ToUTF16(data.bookmark_title());
  if (ContainsFormat(data, ui::ClipboardInternalFormat::kWeb))
    return GetLocalizedString(IDS_CLIPBOARD_MENU_WEB_SMART_PASTE);
  if (ContainsFormat(data, ui::ClipboardInternalFormat::kCustom))
    return GetLabelForCustomData(data);
  NOTREACHED();
  return base::string16();
}

}  // namespace helper
}  // namespace clipboard
}  // namespace ash
