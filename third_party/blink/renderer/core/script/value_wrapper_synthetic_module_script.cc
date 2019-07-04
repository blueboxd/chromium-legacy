// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/value_wrapper_synthetic_module_script.h"

#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "v8/include/v8.h"

namespace blink {

ValueWrapperSyntheticModuleScript::ValueWrapperSyntheticModuleScript(
    Modulator* settings_object,
    ModuleRecord record,
    const KURL& source_url,
    const KURL& base_url,
    const ScriptFetchOptions& fetch_options,
    v8::Local<v8::Value> value,
    const TextPosition& start_position)
    : ModuleScript(settings_object,
                   record,
                   source_url,
                   base_url,
                   fetch_options),
      export_value_(v8::Isolate::GetCurrent(), value) {}

String ValueWrapperSyntheticModuleScript::InlineSourceTextForCSP() const {
  // We don't construct a ValueWrapperSyntheticModuleScript with the original
  // source, but instead construct it from the originally parsed
  // text. If a need arises for the original module source to be used later,
  // ValueWrapperSyntheticModuleScript will need to be modified such that its
  // constructor takes this source text as an additional parameter and stashes
  // it on the ValueWrapperSyntheticModuleScript.
  NOTREACHED();
  return "";
}

void ValueWrapperSyntheticModuleScript::Trace(Visitor* visitor) {
  visitor->Trace(export_value_);
  ModuleScript::Trace(visitor);
}

}  // namespace blink
