// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_features.h"

namespace features {

// Use the passthrough command decoder by default.  This can be overridden with
// the --use-cmd-decoder=passthrough or --use-cmd-decoder=validating flags.
// Feature lives in ui/gl because it affects the GL binding initialization on
// platforms that would otherwise not default to using EGL bindings.
const base::Feature kDefaultPassthroughCommandDecoder{
    "DefaultPassthroughCommandDecoder", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features