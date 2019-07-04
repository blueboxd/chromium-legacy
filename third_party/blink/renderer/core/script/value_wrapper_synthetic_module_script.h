// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_VALUE_WRAPPER_SYNTHETIC_MODULE_SCRIPT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_VALUE_WRAPPER_SYNTHETIC_MODULE_SCRIPT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/script/module_script.h"

namespace WTF {
class TextPosition;
}  // namespace WTF

namespace blink {

class KURL;
class Modulator;

// ValueWrapperSyntheticModuleScript is a module script
// (https://html.spec.whatwg.org/C/#module-script) that default-exports a single
// v8::Value, for example JSON Module Script:
// https://html.spec.whatwg.org/multipage/webappapis.html#json-module-script
class CORE_EXPORT ValueWrapperSyntheticModuleScript final
    : public ModuleScript {
 public:
  ValueWrapperSyntheticModuleScript(Modulator* settings_object,
                                    ModuleRecord record,
                                    const KURL& source_url,
                                    const KURL& base_url,
                                    const ScriptFetchOptions& fetch_options,
                                    v8::Local<v8::Value> value,
                                    const TextPosition& start_position);

  String InlineSourceTextForCSP() const override;
  void Trace(blink::Visitor* visitor) override;

 private:
  TraceWrapperV8Reference<v8::Value> export_value_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_VALUE_WRAPPER_SYNTHETIC_MODULE_SCRIPT_H_
