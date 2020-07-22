// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRERENDER_RENDERER_PRERENDER_OBSERVER_H_
#define COMPONENTS_PRERENDER_RENDERER_PRERENDER_OBSERVER_H_

#include "base/observer_list_types.h"

namespace prerender {

class PrerenderObserver : public base::CheckedObserver {
 public:
  // Set prerendering mode for the plugin.
  virtual void SetIsPrerendering(bool is_prerendering) = 0;

 protected:
  ~PrerenderObserver() override = default;
};

}  // namespace prerender

#endif  // COMPONENTS_PRERENDER_RENDERER_PRERENDER_OBSERVER_H_
