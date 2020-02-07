// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_BROWSER_ANDROID_PAINT_PREVIEW_UTILS_H_
#define COMPONENTS_PAINT_PREVIEW_BROWSER_ANDROID_PAINT_PREVIEW_UTILS_H_

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/optional.h"

namespace content {
class WebContents;
}  // namespace content

namespace paint_preview {

using FinishedCallback =
    base::OnceCallback<void(const base::Optional<base::FilePath>&)>;

// Captures a paint preview of |contents|. On completion returns the path of the
// zip archive in which the paint preview is stored via |finished| (or an empty
// path if the capture failed). The zip archive will exist only if |keep_zip| is
// true.
void Capture(content::WebContents* contents,
             FinishedCallback finished,
             bool keep_zip);

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_BROWSER_ANDROID_PAINT_PREVIEW_UTILS_H_
