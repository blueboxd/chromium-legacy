// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_UTIL_H_
#define COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_UTIL_H_

#include <string>
#include <vector>

#include "base/feature_list.h"

class GURL;

namespace translate {

// Controls whether translation applies to sub frames as well as the
// main frame.
extern const base::Feature kTranslateSubFrames;

// Isolated world sets following security-origin by default.
extern const char kSecurityOrigin[];

// Gets Security origin with which Translate runs. This is used both for
// language checks and to obtain the list of available languages.
GURL GetTranslateSecurityOrigin();

// Return whether sub frame translation is enabled.
bool IsSubFrameTranslationEnabled();

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_COMMON_TRANSLATE_UTIL_H_
