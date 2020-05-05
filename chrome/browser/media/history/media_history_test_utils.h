// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_TEST_UTILS_H_
#define CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_TEST_UTILS_H_

#include "base/optional.h"
#include "base/unguessable_token.h"

namespace media_history {

class MediaHistoryKeyedService;

namespace test {

base::Optional<base::UnguessableToken> GetResetTokenSync(
    MediaHistoryKeyedService* service,
    const int64_t feed_id);

}  // namespace test
}  // namespace media_history

#endif  // CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_TEST_UTILS_H_
