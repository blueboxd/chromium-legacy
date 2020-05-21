// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_ATTACHMENT_H_
#define CHROME_BROWSER_NEARBY_SHARING_ATTACHMENT_H_

#include <stdint.h>

// A single attachment to be sent by / received from a ShareTarget, can be
// either a file or text.
class Attachment {
 public:
  enum class Family { kFile, kText, kMaxValue = kText };

  virtual ~Attachment() = default;

  virtual int64_t size() const = 0;
  virtual Family family() const = 0;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_ATTACHMENT_H_
