// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_UI_HELPERS_H_
#define CHROME_BROWSER_UI_VIEWS_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_UI_HELPERS_H_

#include <memory>

namespace base {
class FilePath;
}

namespace url {
class Origin;
}

namespace views {
class View;
}

namespace native_file_system_ui_helper {

// Creates and returns a label where the place holder is replaced with |origin|,
// while formatting the origin as emphasized text.
std::unique_ptr<views::View> CreateOriginLabel(int message_id,
                                               const url::Origin& origin,
                                               int text_context);

// Creates and returns a label where the place holders are replaced with
// |origin| and |path|, while formatting the origin and path as emphasized text.
std::unique_ptr<views::View> CreateOriginPathLabel(int message_id,
                                                   const url::Origin& origin,
                                                   const base::FilePath& path,
                                                   int text_context);

}  // namespace native_file_system_ui_helper

#endif  // CHROME_BROWSER_UI_VIEWS_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_UI_HELPERS_H_
