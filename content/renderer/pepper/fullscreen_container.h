// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_FULLSCREEN_CONTAINER_H_
#define CONTENT_RENDERER_PEPPER_FULLSCREEN_CONTAINER_H_

#include "base/memory/ref_counted.h"

namespace cc {
class Layer;
}

namespace ui {
class Cursor;
}

namespace content {

// This class is like a lightweight WebPluginContainer for fullscreen PPAPI
// plugins, that only handles painting.
class FullscreenContainer {
 public:
  // Destroys the fullscreen window. This also destroys the FullscreenContainer
  // instance.
  virtual void Destroy() = 0;

  // Notifies the container that the mouse cursor has changed.
  virtual void PepperDidChangeCursor(const ui::Cursor& cursor) = 0;

  virtual void SetLayer(scoped_refptr<cc::Layer> layer) = 0;

 protected:
  virtual ~FullscreenContainer() {}
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_FULLSCREEN_CONTAINER_H_
