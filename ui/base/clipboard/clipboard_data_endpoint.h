// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_DATA_ENDPOINT_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_DATA_ENDPOINT_H_

#include "url/gurl.h"

namespace ui {

// ClipboardDataEndpoint can represent:
// - The source of the data in the clipboard.
// - The destination trying to access the clipboard data.
class COMPONENT_EXPORT(UI_BASE_CLIPBOARD) ClipboardDataEndpoint {
 public:
  explicit ClipboardDataEndpoint(const GURL& url);

  ClipboardDataEndpoint(const ClipboardDataEndpoint& other);
  ClipboardDataEndpoint(ClipboardDataEndpoint&& other);

  ClipboardDataEndpoint& operator=(const ClipboardDataEndpoint& other);
  ClipboardDataEndpoint& operator=(ClipboardDataEndpoint&& other);

  ~ClipboardDataEndpoint();

 private:
  GURL url_;
  // TODO(crbug.com/1103217): Add more formats to be supported in addition to
  // GURL.
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_DATA_ENDPOINT_H_
