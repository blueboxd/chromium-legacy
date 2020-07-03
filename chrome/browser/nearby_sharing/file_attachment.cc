// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/nearby_sharing/file_attachment.h"

FileAttachment::FileAttachment(std::string file_name,
                               Type type,
                               int64_t size,
                               base::Optional<base::FilePath> file_path,
                               std::string mime_type)
    : file_name_(std::move(file_name)),
      type_(type),
      size_(size),
      file_path_(std::move(file_path)),
      mime_type_(std::move(mime_type)) {}

FileAttachment::~FileAttachment() = default;
FileAttachment::FileAttachment(const FileAttachment&) = default;
FileAttachment& FileAttachment::operator=(const FileAttachment&) = default;

int64_t FileAttachment::size() const {
  return size_;
}

Attachment::Family FileAttachment::family() const {
  return Attachment::Family::kFile;
}
