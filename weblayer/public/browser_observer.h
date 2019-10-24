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
  virtual void DisplayedUrlChanged(const GURL& url) {}
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_BROWSER_OBSERVER_H_
