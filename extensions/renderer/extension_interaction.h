// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSION_INTERACTION_H_
#define EXTENSIONS_RENDERER_EXTENSION_INTERACTION_H_

#include <memory>

#include "base/macros.h"

namespace blink {
class WebLocalFrame;
class WebScopedUserGesture;
}  // namespace blink

namespace extensions {

class ScriptContext;

// Provides user interaction related utilities for extensions, works for
// both RenderFrame based and Service Worker based extensions.
// Provides scoped interaction creation for a context and provides current
// interaction state retrieval method.
class ExtensionInteraction {
 public:
  ~ExtensionInteraction();

  // Creates a scoped interaction for a RenderFrame based extension.
  static std::unique_ptr<ExtensionInteraction> CreateScopeForMainThread(
      blink::WebLocalFrame* web_frame);
  // Creates a scoped interaction for a Service Worker based extension.
  static std::unique_ptr<ExtensionInteraction> CreateScopeForWorker();

  // Returns true if |context| has an active user interaction.
  static bool HasActiveInteraction(ScriptContext* context);

 private:
  explicit ExtensionInteraction(blink::WebLocalFrame* context);

  bool is_worker_thread_ = false;
  std::unique_ptr<blink::WebScopedUserGesture> main_thread_gesture_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInteraction);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSION_INTERACTION_H_
