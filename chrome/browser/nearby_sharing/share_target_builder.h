// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_SHARE_TARGET_BUILDER_H_
#define CHROME_BROWSER_NEARBY_SHARING_SHARE_TARGET_BUILDER_H_

#include <string>
#include <vector>

#include "chrome/browser/nearby_sharing/file_attachment.h"
#include "chrome/browser/nearby_sharing/share_target.h"
#include "chrome/browser/nearby_sharing/text_attachment.h"

class ShareTargetBuilder {
 public:
  ShareTargetBuilder();
  ~ShareTargetBuilder();

  ShareTargetBuilder& set_device_name(const std::string& device_name);

  ShareTargetBuilder& set_is_incoming(bool is_incoming);

  ShareTargetBuilder& add_attachment(TextAttachment attachment);

  ShareTargetBuilder& add_attachment(FileAttachment attachment);

  ShareTarget build() const;

 private:
  std::string device_name_;
  bool is_incoming_ = false;
  std::vector<TextAttachment> text_attachments_;
  std::vector<FileAttachment> file_attachments_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_SHARE_TARGET_BUILDER_H_
