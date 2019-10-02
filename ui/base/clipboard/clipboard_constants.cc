// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_constants.h"

namespace ui {

const char kMimeTypeText[] = "text/plain";
const char kMimeTypeTextUtf8[] = "text/plain;charset=utf-8";
const char kMimeTypeURIList[] = "text/uri-list";
const char kMimeTypeMozillaURL[] = "text/x-moz-url";
const char kMimeTypeDownloadURL[] = "downloadurl";
const char kMimeTypeHTML[] = "text/html";
const char kMimeTypeRTF[] = "text/rtf";
const char kMimeTypePNG[] = "image/png";
// TODO(dcheng): This name is temporary. See crbug.com/106449.
const char kMimeTypeWebCustomData[] = "chromium/x-web-custom-data";
const char kMimeTypeWebkitSmartPaste[] = "chromium/x-webkit-paste";
const char kMimeTypePepperCustomData[] = "chromium/x-pepper-custom-data";

}  // namespace ui
