// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_STRINGS_WSTRING_EMBEDDED_NULLS_H_
#define CHROME_CHROME_CLEANER_STRINGS_WSTRING_EMBEDDED_NULLS_H_

#include "chrome/chrome_cleaner/strings/string16_embedded_nulls.h"

namespace chrome_cleaner {

// TODO(crbug.com/911896): Rename String16EmbeddedNulls to WStringEmbeddedNulls
// and get rid of this alias.
using WStringEmbeddedNulls = String16EmbeddedNulls;

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_STRINGS_WSTRING_EMBEDDED_NULLS_H_
