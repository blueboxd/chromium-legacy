// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'discover-redeem-offers-card',

  behaviors: [DiscoverModuleBehavior],

  onClick_() {
    window.open('http://www.google.com/chromebook/offers/', '_blank');
  },
});
