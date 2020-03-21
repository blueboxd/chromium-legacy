// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/font_access/navigator_fonts.h"

#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/workers/worker_navigator.h"
#include "third_party/blink/renderer/modules/font_access/font_manager.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

namespace {

template <typename T>
class NavigatorFontsImpl final : public GarbageCollected<NavigatorFontsImpl<T>>,
                                 public Supplement<T>,
                                 public NameClient {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorFontsImpl);

 public:
  static const char kSupplementName[];

  static NavigatorFontsImpl& From(T& navigator) {
    NavigatorFontsImpl* supplement = static_cast<NavigatorFontsImpl*>(
        Supplement<T>::template From<NavigatorFontsImpl>(navigator));
    if (!supplement) {
      supplement = MakeGarbageCollected<NavigatorFontsImpl>(navigator);
      Supplement<T>::ProvideTo(navigator, supplement);
    }
    return *supplement;
  }

  explicit NavigatorFontsImpl(T& navigator) : Supplement<T>(navigator) {}

  FontManager* GetFontManager() const {
    if (!font_manager_) {
      font_manager_ = MakeGarbageCollected<FontManager>();
    }
    return font_manager_.Get();
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(font_manager_);
    Supplement<T>::Trace(visitor);
  }

  const char* NameInHeapSnapshot() const override {
    return "NavigatorFontsImpl";
  }

 private:
  mutable Member<FontManager> font_manager_;
};

// static
template <typename T>
const char NavigatorFontsImpl<T>::kSupplementName[] = "NavigatorFontsImpl";

}  // namespace

FontManager* NavigatorFonts::fonts(ScriptState* script_state,
                                   Navigator& navigator,
                                   ExceptionState& exception_state) {
  DCHECK(ExecutionContext::From(script_state)->IsContextThread());
  return NavigatorFontsImpl<Navigator>::From(navigator).GetFontManager();
}

FontManager* NavigatorFonts::fonts(ScriptState* script_state,
                                   WorkerNavigator& navigator,
                                   ExceptionState& exception_state) {
  DCHECK(ExecutionContext::From(script_state)->IsContextThread());
  // TODO(https://crbug.com/1043348): Support FeaturePolicy when it's ready for
  // workers.
  return NavigatorFontsImpl<WorkerNavigator>::From(navigator).GetFontManager();
}

}  // namespace blink
