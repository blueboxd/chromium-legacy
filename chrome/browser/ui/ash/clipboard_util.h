// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CLIPBOARD_UTIL_H_
#define CHROME_BROWSER_UI_ASH_CLIPBOARD_UTIL_H_

namespace base {
class FilePath;
}  // namespace base

namespace clipboard_util {

// Reads a local file and then copies that file to the system clipboard. This
// should not be run on the UI Thread as it performs blocking IO.
void ReadFileAndCopyToClipboardLocal(const base::FilePath& local_file);

}  // namespace clipboard_util

#endif  // CHROME_BROWSER_UI_ASH_CLIPBOARD_UTIL_H_
