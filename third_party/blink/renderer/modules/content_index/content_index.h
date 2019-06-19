// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_INDEX_CONTENT_INDEX_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_INDEX_CONTENT_INDEX_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ContentDescription;
class ScriptState;

class ContentIndex final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ContentIndex();
  ~ContentIndex() override;

  // Web-exposed function defined in the IDL file.
  ScriptPromise add(ScriptState* script_state,
                    const ContentDescription* description);
  ScriptPromise deleteDescription(ScriptState* script_state, const String& id);
  ScriptPromise getDescriptions(ScriptState* script_state);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_INDEX_CONTENT_INDEX_H_
