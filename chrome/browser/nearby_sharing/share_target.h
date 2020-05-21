// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_SHARE_TARGET_H_
#define CHROME_BROWSER_NEARBY_SHARING_SHARE_TARGET_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "chrome/browser/nearby_sharing/file_attachment.h"
#include "chrome/browser/nearby_sharing/text_attachment.h"
#include "url/gurl.h"

// A remote device.
class ShareTarget {
 public:
  enum class Type { kUnknown, kPhone, kTablet, kLaptop, kMaxValue = kLaptop };

  ShareTarget(std::string device_name,
              GURL image_url,
              Type type,
              std::vector<TextAttachment> text_attachments,
              std::vector<FileAttachment> file_attachments,
              bool is_incoming,
              base::Optional<std::string> full_name,
              bool is_known);
  ~ShareTarget();

  int id() { return id_; }
  const std::string& device_name() { return device_name_; }

  // Returns a Uri that points to an image of the ShareTarget, if one exists.
  const base::Optional<GURL>& image_url() { return image_url_; }

  Type type() { return type_; }
  const std::vector<TextAttachment>& text_attachments() {
    return text_attachments_;
  }
  const std::vector<FileAttachment>& file_attachments() {
    return file_attachments_;
  }
  bool is_incoming() { return is_incoming_; }
  const base::Optional<std::string>& full_name() { return full_name_; }

  // Returns True if local device has the PublicCertificate the remote device is
  // advertising.
  bool is_known() { return is_known_; }

 private:
  int id_;
  std::string device_name_;
  base::Optional<GURL> image_url_;
  Type type_;
  std::vector<TextAttachment> text_attachments_;
  std::vector<FileAttachment> file_attachments_;
  bool is_incoming_;
  base::Optional<std::string> full_name_;
  bool is_known_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_SHARE_TARGET_H_
