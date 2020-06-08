// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/switches.h"

namespace blink {
namespace switches {

// Allows processing of input before a frame has been committed.
// TODO(schenney): crbug.com/987626. Used by headless. Look for a way not
// involving a command line switch.
const char kAllowPreCommitInput[] = "allow-pre-commit-input";

}  // namespace switches
}  // namespace blink
