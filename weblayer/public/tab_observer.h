// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_TAB_OBSERVER_H_
#define WEBLAYER_PUBLIC_TAB_OBSERVER_H_

class GURL;

namespace weblayer {

class TabObserver {
 public:
  // The URL bar should be updated to |url|.
  virtual void DisplayedUrlChanged(const GURL& url) {}

 protected:
  virtual ~TabObserver() {}
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_TAB_OBSERVER_H_
