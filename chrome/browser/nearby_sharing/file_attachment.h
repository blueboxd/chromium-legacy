// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_FILE_ATTACHMENT_H_
#define CHROME_BROWSER_NEARBY_SHARING_FILE_ATTACHMENT_H_

#include <string>

#include "base/files/file_path.h"
#include "base/optional.h"
#include "chrome/browser/nearby_sharing/attachment.h"

// A single attachment to be sent by / received from a |ShareTarget|, can be
// either a file or text.
class FileAttachment : public Attachment {
 public:
  // Different types are used to offer richer experiences on Receiver side,
  // mainly for: 1. displaying notification of attachment types, 2. opening
  // different types with different apps. Remember to update Notifications,
  // ShareTarget, etc once more types are introduced here.
  enum class Type {
    kUnknown,
    kImage,
    kVideo,
    kApp,
    kAudio,
    kMaxValue = kAudio
  };

  FileAttachment(std::string file_name,
                 Type type,
                 int64_t size,
                 base::Optional<base::FilePath> file_path,
                 std::string mime_type);
  ~FileAttachment() override;

  // Attachment:
  int64_t size() const override;
  Attachment::Family family() const override;

  const std::string& file_name() const { return file_name_; }
  Type type() const { return type_; }
  const base::Optional<base::FilePath>& file_path() const { return file_path_; }
  const std::string& mime_type() const { return mime_type_; }

 private:
  std::string file_name_;
  Type type_;
  int64_t size_;
  base::Optional<base::FilePath> file_path_;
  std::string mime_type_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_FILE_ATTACHMENT_H_
