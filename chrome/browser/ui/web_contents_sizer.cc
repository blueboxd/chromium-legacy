// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_contents_sizer.h"

#include "build/build_config.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/size.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#elif defined(OS_ANDROID)
#include "content/public/browser/render_widget_host_view.h"
#include "ui/android/view_android.h"
#endif

void ResizeWebContents(content::WebContents* web_contents,
                       const gfx::Rect& new_bounds) {
#if defined(USE_AURA)
  aura::Window* window = web_contents->GetNativeView();
  window->SetBounds(gfx::Rect(window->bounds().origin(), new_bounds.size()));
#elif defined(OS_ANDROID)
  content::RenderWidgetHostView* view = web_contents->GetRenderWidgetHostView();
  if (view)
    view->SetBounds(new_bounds);
#else
// The Mac implementation is in web_contents_sizer.mm.
#error "ResizeWebContents not implemented for this platform"
#endif
}

gfx::Size GetWebContentsSize(content::WebContents* web_contents) {
#if defined(USE_AURA)
  aura::Window* window = web_contents->GetNativeView();
  return window->bounds().size();
#elif defined(OS_ANDROID)
  ui::ViewAndroid* view_android = web_contents->GetNativeView();
  return view_android->bounds().size();
#else
// The Mac implementation is in web_contents_sizer.mm.
#error "GetWebContentsSize not implemented for this platform"
#endif
}
