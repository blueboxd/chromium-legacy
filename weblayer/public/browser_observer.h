// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_BROWSER_OBSERVER_H_
#define WEBLAYER_PUBLIC_BROWSER_OBSERVER_H_

class GURL;

namespace weblayer {

class BrowserObserver {
 public:
  virtual ~BrowserObserver() {}

  // The URL bar should be updated to |url|.
  virtual void DisplayedURLChanged(const GURL& url) {}

  // Indicates that loading has started (|is_loading| is true) or is done
  // (|is_loading| is false). |to_different_document| will be true unless the
  // load is a fragment navigation, or triggered by
  // history.pushState/replaceState.
  virtual void LoadingStateChanged(bool is_loading,
                                   bool to_different_document) {}

  // Indicates that the load progress of the WebContents has changed. This
  // corresponds to WebContentsDelegate::LoadProgressChanged, meaning |progress|
  // ranges from 0.0 to 1.0.
  virtual void LoadProgressChanged(double progress) {}

  // This is fired after each navigation has completed, to indicate that the
  // first paint after a non-empty layout has finished.
  virtual void FirstContentfulPaint() {}
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_BROWSER_OBSERVER_H_
