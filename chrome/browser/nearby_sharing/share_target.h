// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_SHARE_TARGET_H_
#define CHROME_BROWSER_NEARBY_SHARING_SHARE_TARGET_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "base/unguessable_token.h"
#include "chrome/browser/nearby_sharing/file_attachment.h"
#include "chrome/browser/nearby_sharing/text_attachment.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share.mojom-shared.h"
#include "url/gurl.h"

// A remote device.
struct ShareTarget {
 public:
  ShareTarget();
  ShareTarget(std::string device_name,
              GURL image_url,
              nearby_share::mojom::ShareTargetType type,
              std::vector<TextAttachment> text_attachments,
              std::vector<FileAttachment> file_attachments,
              bool is_incoming,
              base::Optional<std::string> full_name,
              bool is_known);
  ShareTarget(const ShareTarget&);
  ShareTarget(ShareTarget&&);
  ShareTarget& operator=(const ShareTarget&);
  ShareTarget& operator=(ShareTarget&&);
  ~ShareTarget();

  base::UnguessableToken id = base::UnguessableToken::Create();
  std::string device_name;
  // Uri that points to an image of the ShareTarget, if one exists.
  base::Optional<GURL> image_url;
  nearby_share::mojom::ShareTargetType type =
      nearby_share::mojom::ShareTargetType::kUnknown;
  std::vector<TextAttachment> text_attachments;
  std::vector<FileAttachment> file_attachments;
  bool is_incoming = false;
  base::Optional<std::string> full_name;
  // True if local device has the PublicCertificate this target is advertising.
  bool is_known = false;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_SHARE_TARGET_H_
