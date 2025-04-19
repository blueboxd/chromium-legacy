// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_MAC_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_MAC_H_

#include <stddef.h>
#include <stdint.h>

#include "base/apple/foundation_util.h"
#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "ui/base/clipboard/clipboard.h"

@class NSPasteboard;

namespace ui {

// Documentation on the underlying MacOS API this ultimately abstracts is
// available at https://developer.apple.com/documentation/appkit/nspasteboard
// and
// https://developer.apple.com/library/archive/documentation/General/Conceptual/Devpedia-CocoaApp/Pasteboard.html.
class COMPONENT_EXPORT(UI_BASE_CLIPBOARD) ClipboardMac : public Clipboard {
 public:
  ClipboardMac(const ClipboardMac&) = delete;
  ClipboardMac& operator=(const ClipboardMac&) = delete;

 private:
  friend class Clipboard;
  friend class ClipboardMacTest;

  ClipboardMac();
  ~ClipboardMac() override;

  // Clipboard overrides:
  void OnPreShutdown() override;
  std::optional<DataTransferEndpoint> GetSource(
      ClipboardBuffer buffer) const override;
  const ClipboardSequenceNumberToken& GetSequenceNumber(
      ClipboardBuffer buffer) const override;
  std::vector<std::u16string> GetStandardFormats(
      ClipboardBuffer buffer,
      const DataTransferEndpoint* data_dst) const override;
  bool IsFormatAvailable(const ClipboardFormatType& format,
                         ClipboardBuffer buffer,
                         const DataTransferEndpoint* data_dst) const override;
  bool IsMarkedByOriginatorAsConfidential() const override;
  void MarkAsConfidential() override;
  void Clear(ClipboardBuffer buffer) override;
  void ReadAvailableTypes(ClipboardBuffer buffer,
                          const DataTransferEndpoint* data_dst,
                          std::vector<std::u16string>* types) const override;
  void ReadText(ClipboardBuffer buffer,
                const DataTransferEndpoint* data_dst,
                std::u16string* result) const override;
  void ReadAsciiText(ClipboardBuffer buffer,
                     const DataTransferEndpoint* data_dst,
                     std::string* result) const override;
  void ReadHTML(ClipboardBuffer buffer,
                const DataTransferEndpoint* data_dst,
                std::u16string* markup,
                std::string* src_url,
                uint32_t* fragment_start,
                uint32_t* fragment_end) const override;
  void ReadSvg(ClipboardBuffer buffer,
               const DataTransferEndpoint* data_dst,
               std::u16string* result) const override;
  void ReadRTF(ClipboardBuffer buffer,
               const DataTransferEndpoint* data_dst,
               std::string* result) const override;
  void ReadPng(ClipboardBuffer buffer,
               const DataTransferEndpoint* data_dst,
               ReadPngCallback callback) const override;
  void ReadDataTransferCustomData(ClipboardBuffer buffer,
                                  const std::u16string& type,
                                  const DataTransferEndpoint* data_dst,
                                  std::u16string* result) const override;
  void ReadFilenames(ClipboardBuffer buffer,
                     const DataTransferEndpoint* data_dst,
                     std::vector<ui::FileInfo>* result) const override;
  void ReadBookmark(const DataTransferEndpoint* data_dst,
                    std::u16string* title,
                    std::string* url) const override;
  void ReadData(const ClipboardFormatType& format,
                const DataTransferEndpoint* data_dst,
                std::string* result) const override;
  void WritePortableAndPlatformRepresentations(
      ClipboardBuffer buffer,
      const ObjectMap& objects,
      std::vector<Clipboard::PlatformRepresentation> platform_representations,
      std::unique_ptr<DataTransferEndpoint> data_src) override;
  void WriteText(const char* text_data, size_t text_len) override;
  void WriteHTML(const char* markup_data,
                 size_t markup_len,
                 const char* url_data,
                 size_t url_len) override;
  void WriteUnsanitizedHTML(const char* markup_data,
                            size_t markup_len,
                            const char* url_data,
                            size_t url_len) override;
  void WriteSvg(const char* markup_data, size_t markup_len) override;
  void WriteRTF(const char* rtf_data, size_t data_len) override;
  void WriteFilenames(std::vector<ui::FileInfo> filenames) override;
  void WriteBookmark(const char* title_data,
                     size_t title_len,
                     const char* url_data,
                     size_t url_len) override;
  void WriteWebSmartPaste() override;
  void WriteBitmap(const SkBitmap& bitmap) override;
  void WriteData(const ClipboardFormatType& format,
                 const char* data_data,
                 size_t data_len) override;

  void WriteBitmapInternal(const SkBitmap& bitmap, NSPasteboard* pasteboard);
  void ReadPngInternal(ClipboardBuffer buffer,
                       NSPasteboard* pasteboard,
                       ReadPngCallback callback) const;
  std::optional<DataTransferEndpoint> GetSourceInternal(
      ClipboardBuffer buffer,
      NSPasteboard* pasteboard) const;
  void ClearInternal(ClipboardBuffer buffer, NSPasteboard* pasteboard);
  void WritePortableAndPlatformRepresentationsInternal(
      ClipboardBuffer buffer,
      const ObjectMap& objects,
      std::vector<Clipboard::PlatformRepresentation> platform_representations,
      std::unique_ptr<DataTransferEndpoint> data_src,
      NSPasteboard* pasteboard);

  // Mapping of OS-provided sequence number to a unique token.
  mutable struct {
    NSInteger sequence_number;
    ClipboardSequenceNumberToken token;
  } clipboard_sequence_;
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_MAC_H_
