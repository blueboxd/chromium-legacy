// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_format_type.h"

#include <shlobj.h>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"

namespace ui {

// ClipboardFormatType implementation.
ClipboardFormatType::ClipboardFormatType() = default;

ClipboardFormatType::ClipboardFormatType(UINT native_format)
    : ClipboardFormatType(native_format, -1) {}

ClipboardFormatType::ClipboardFormatType(UINT native_format, LONG index)
    : ClipboardFormatType(native_format, index, TYMED_HGLOBAL) {}

// In C++ 20, we can use designated initializers.
ClipboardFormatType::ClipboardFormatType(UINT native_format,
                                         LONG index,
                                         DWORD tymed)
    : data_{/* .cfFormat */ static_cast<CLIPFORMAT>(native_format),
            /* .ptd */ nullptr, /* .dwAspect */ DVASPECT_CONTENT,
            /* .lindex */ index, /* .tymed*/ tymed} {}

ClipboardFormatType::~ClipboardFormatType() = default;

std::string ClipboardFormatType::Serialize() const {
  return base::NumberToString(data_.cfFormat);
}

// static
ClipboardFormatType ClipboardFormatType::Deserialize(
    const std::string& serialization) {
  int clipboard_format = -1;
  if (!base::StringToInt(serialization, &clipboard_format)) {
    NOTREACHED();
    return ClipboardFormatType();
  }
  return ClipboardFormatType(clipboard_format);
}

bool ClipboardFormatType::operator<(const ClipboardFormatType& other) const {
  return data_.cfFormat < other.data_.cfFormat;
}

bool ClipboardFormatType::Equals(const ClipboardFormatType& other) const {
  return data_.cfFormat == other.data_.cfFormat;
}

// Predefined ClipboardFormatTypes.

// static
ClipboardFormatType ClipboardFormatType::GetType(
    const std::string& format_string) {
  return ClipboardFormatType(
      ::RegisterClipboardFormat(base::ASCIIToUTF16(format_string).c_str()));
}

// The following formats can be referenced by ClipboardUtilWin::GetPlainText.
// For reasons (COM), they must be initialized in a thread-safe manner.
// TODO(dcheng): We probably need to make static initialization of "known"
// ClipboardFormatTypes thread-safe on all platforms.
#define CR_STATIC_UI_CLIPBOARD_FORMAT_TYPE(name, ...)                        \
  struct ClipboardFormatTypeArgumentForwarder : public ClipboardFormatType { \
    ClipboardFormatTypeArgumentForwarder()                                   \
        : ClipboardFormatType(__VA_ARGS__) {}                                \
  };                                                                         \
  static base::LazyInstance<ClipboardFormatTypeArgumentForwarder>::Leaky     \
      name = LAZY_INSTANCE_INITIALIZER

// static
const ClipboardFormatType& ClipboardFormatType::GetUrlType() {
  CR_STATIC_UI_CLIPBOARD_FORMAT_TYPE(type,
                                     ::RegisterClipboardFormat(CFSTR_INETURLA));
  return type.Get();
}

// static
const ClipboardFormatType& ClipboardFormatType::GetUrlWType() {
  CR_STATIC_UI_CLIPBOARD_FORMAT_TYPE(type,
                                     ::RegisterClipboardFormat(CFSTR_INETURLW));
  return type.Get();
}

// static
const ClipboardFormatType& ClipboardFormatType::GetMozUrlType() {
  CR_STATIC_UI_CLIPBOARD_FORMAT_TYPE(
      type, ::RegisterClipboardFormat(L"text/x-moz-url"));
  return type.Get();
}

// static
const ClipboardFormatType& ClipboardFormatType::GetPlainTextType() {
  CR_STATIC_UI_CLIPBOARD_FORMAT_TYPE(type, CF_TEXT);
  return type.Get();
}

// static
const ClipboardFormatType& ClipboardFormatType::GetPlainTextWType() {
  CR_STATIC_UI_CLIPBOARD_FORMAT_TYPE(type, CF_UNICODETEXT);
  return type.Get();
}

// static
const ClipboardFormatType& ClipboardFormatType::GetFilenameType() {
  CR_STATIC_UI_CLIPBOARD_FORMAT_TYPE(
      type, ::RegisterClipboardFormat(CFSTR_FILENAMEA));
  return type.Get();
}

// static
const ClipboardFormatType& ClipboardFormatType::GetFilenameWType() {
  CR_STATIC_UI_CLIPBOARD_FORMAT_TYPE(
      type, ::RegisterClipboardFormat(CFSTR_FILENAMEW));
  return type.Get();
}

// MS HTML Format

// static
const ClipboardFormatType& ClipboardFormatType::GetHtmlType() {
  CR_STATIC_UI_CLIPBOARD_FORMAT_TYPE(type,
                                     ::RegisterClipboardFormat(L"HTML Format"));
  return type.Get();
}

// MS RTF Format

// static
const ClipboardFormatType& ClipboardFormatType::GetRtfType() {
  CR_STATIC_UI_CLIPBOARD_FORMAT_TYPE(
      type, ::RegisterClipboardFormat(L"Rich Text Format"));
  return type.Get();
}

// static
const ClipboardFormatType& ClipboardFormatType::GetBitmapType() {
  CR_STATIC_UI_CLIPBOARD_FORMAT_TYPE(type, CF_BITMAP);
  return type.Get();
}

// Firefox text/html

// static
const ClipboardFormatType& ClipboardFormatType::GetTextHtmlType() {
  CR_STATIC_UI_CLIPBOARD_FORMAT_TYPE(type,
                                     ::RegisterClipboardFormat(L"text/html"));
  return type.Get();
}

// static
const ClipboardFormatType& ClipboardFormatType::GetCFHDropType() {
  CR_STATIC_UI_CLIPBOARD_FORMAT_TYPE(type, CF_HDROP);
  return type.Get();
}

// Nothing prevents the drag source app from using the CFSTR_FILEDESCRIPTORA
// ANSI format (e.g., it could be that it doesn't support Unicode). So need to
// register both the ANSI and Unicode file group descriptors.
// static
const ClipboardFormatType& ClipboardFormatType::GetFileDescriptorType() {
  CR_STATIC_UI_CLIPBOARD_FORMAT_TYPE(
      type, ::RegisterClipboardFormat(CFSTR_FILEDESCRIPTORA));
  return type.Get();
}

// static
const ClipboardFormatType& ClipboardFormatType::GetFileDescriptorWType() {
  CR_STATIC_UI_CLIPBOARD_FORMAT_TYPE(
      type, ::RegisterClipboardFormat(CFSTR_FILEDESCRIPTORW));
  return type.Get();
}

// static
const ClipboardFormatType& ClipboardFormatType::GetFileContentZeroType() {
  // Note this uses a storage media type of TYMED_HGLOBAL, which is not commonly
  // used with CFSTR_FILECONTENTS (but used in Chromium--see
  // OSExchangeDataProviderWin::SetFileContents). Use GetFileContentAtIndexType
  // if TYMED_ISTREAM and TYMED_ISTORAGE are needed.
  // TODO(https://crbug.com/950756): Should TYMED_ISTREAM / TYMED_ISTORAGE be
  // used instead of TYMED_HGLOBAL in
  // OSExchangeDataProviderWin::SetFileContents.
  CR_STATIC_UI_CLIPBOARD_FORMAT_TYPE(
      type, ::RegisterClipboardFormat(CFSTR_FILECONTENTS), 0);
  return type.Get();
}

// static
std::map<LONG, ClipboardFormatType>&
ClipboardFormatType::GetFileContentTypeMap() {
  static base::NoDestructor<std::map<LONG, ClipboardFormatType>>
      index_to_type_map;
  return *index_to_type_map;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetFileContentAtIndexType(
    LONG index) {
  auto& index_to_type_map = GetFileContentTypeMap();

  auto insert_or_assign_result = index_to_type_map.insert(
      {index,
       ClipboardFormatType(::RegisterClipboardFormat(CFSTR_FILECONTENTS), index,
                           TYMED_HGLOBAL | TYMED_ISTREAM | TYMED_ISTORAGE)});
  return insert_or_assign_result.first->second;
}

// static
const ClipboardFormatType& ClipboardFormatType::GetIDListType() {
  CR_STATIC_UI_CLIPBOARD_FORMAT_TYPE(
      type, ::RegisterClipboardFormat(CFSTR_SHELLIDLIST));
  return type.Get();
}

// static
const ClipboardFormatType& ClipboardFormatType::GetWebKitSmartPasteType() {
  CR_STATIC_UI_CLIPBOARD_FORMAT_TYPE(
      type, ::RegisterClipboardFormat(L"WebKit Smart Paste Format"));
  return type.Get();
}

// static
const ClipboardFormatType& ClipboardFormatType::GetWebCustomDataType() {
  // TODO(dcheng): This name is temporary. See http://crbug.com/106449.
  CR_STATIC_UI_CLIPBOARD_FORMAT_TYPE(
      type, ::RegisterClipboardFormat(L"Chromium Web Custom MIME Data Format"));
  return type.Get();
}

// static
const ClipboardFormatType& ClipboardFormatType::GetPepperCustomDataType() {
  CR_STATIC_UI_CLIPBOARD_FORMAT_TYPE(
      type, ::RegisterClipboardFormat(L"Chromium Pepper MIME Data Format"));
  return type.Get();
}
#undef CR_STATIC_UI_CLIPBOARD_FORMAT_TYPE

}  // namespace ui
