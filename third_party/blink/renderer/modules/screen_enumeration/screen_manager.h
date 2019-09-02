// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_ENUMERATION_SCREEN_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_ENUMERATION_SCREEN_MANAGER_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/screen_enumeration/screen_enumeration.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ScriptState;

// An interface for querying the state of the device's screen space.
//
// The interface is available in both window and service worker execution
// contexts.
class ScreenManager final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Creates a ScreenManager and binds it to the browser-side implementation.
  explicit ScreenManager(mojo::Remote<mojom::blink::ScreenEnumeration> backend);

  // Resolves to the list of |Display| objects in the device's screen space.
  ScriptPromise requestDisplays(ScriptState*, ExceptionState&);

  // Called if the backend is disconnected, e.g. during renderer shutdown.
  void OnBackendDisconnected();

 private:
  // Connection to the ScreenEnumeration implementation in the browser process.
  mojo::Remote<mojom::blink::ScreenEnumeration> backend_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_ENUMERATION_SCREEN_MANAGER_H_
