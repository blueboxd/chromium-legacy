// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.m.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import './profile_picker_main_view.js';
import './profile_picker_shared_css.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {NavigationBehavior, Routes} from './navigation_behavior.js';
import {ensureLazyLoaded} from './profile_creation_flow/ensure_lazy_loaded.js';

Polymer({
  is: 'profile-picker-app',

  _template: html`{__html_template__}`,

  behaviors: [NavigationBehavior],

  /** @private {?Routes} */
  currentRoute_: null,

  /**
   * @param {Routes} route
   * @param {string} step
   * @private
   */
  onRouteChange(route, step) {
    const setStep = () => {
      this.$.viewManager.switchView(step, 'fade-in', 'no-animation');
    };

    // If the route changed, initialize modules for that route.
    if (this.currentRoute_ !== route) {
      this.currentRoute_ = route;
      this.initializeModules_().then(setStep);
    } else {
      setStep();
    }
  },

  /**
   * @return {!Promise}
   * @private
   */
  initializeModules_() {
    switch (this.currentRoute_) {
      case Routes.MAIN:
        return Promise.resolve();
      case Routes.NEW_PROFILE:
        return ensureLazyLoaded();
      default:
        // |this.currentRoute_| should be set by now.
        assertNotReached();
    }
  },
});
