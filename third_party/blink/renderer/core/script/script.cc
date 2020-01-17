// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/script.h"

#include "base/optional.h"

namespace blink {

base::Optional<mojom::ScriptType> Script::ParseScriptType(
    const String& script_type) {
  if (script_type == "classic")
    return mojom::ScriptType::kClassic;
  if (script_type == "module")
    return mojom::ScriptType::kModule;
  return base::nullopt;
}

}  // namespace blink
