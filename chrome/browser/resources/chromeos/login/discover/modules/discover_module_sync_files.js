// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'discover-sync-files-card',

  behaviors: [DiscoverModuleBehavior],

  onClick_() {
    window.open('https://www.google.com/chromebook/switch/', '_blank');
  },
});
