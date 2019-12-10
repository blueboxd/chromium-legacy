// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_ENUMERATION_GLOBAL_SCREEN_ENUMERATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_ENUMERATION_GLOBAL_SCREEN_ENUMERATION_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ExceptionState;
class LocalDOMWindow;
class ScriptPromise;
class ScriptState;

// A proposed interface for querying the state of the device's screen space.
// https://github.com/spark008/screen-enumeration/blob/master/EXPLAINER.md
class GlobalScreenEnumeration {
  STATIC_ONLY(GlobalScreenEnumeration);

 public:
  // Resolves to the list of |Screen| objects in the device's screen space.
  static ScriptPromise getScreens(ScriptState*,
                                  LocalDOMWindow&,
                                  ExceptionState&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_ENUMERATION_GLOBAL_SCREEN_ENUMERATION_H_
