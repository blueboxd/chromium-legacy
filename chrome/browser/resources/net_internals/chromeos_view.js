// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays information on ChromeOS specific features.
 */
const CrosView = (function() {
  'use strict';

  /**
   *  @constructor
   *  @extends {DivView}
   */
  function CrosView() {
    assertFirstConstructorCall(CrosView);

    // Call superclass's constructor.
    DivView.call(this, CrosView.MAIN_BOX_ID);
  }

  CrosView.TAB_ID = 'tab-handle-chromeos';
  CrosView.TAB_NAME = 'ChromeOS';
  CrosView.TAB_HASH = '#chromeos';

  CrosView.MAIN_BOX_ID = 'chromeos-view-tab-content';

  cr.addSingletonGetter(CrosView);

  CrosView.prototype = {
    // Inherit from DivView.
    __proto__: DivView.prototype,
  };

  return CrosView;
})();
