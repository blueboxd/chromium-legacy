// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './edu_login_welcome.js';
import './edu_login_parents.js';
import './edu_login_parent_signin.js';
import './edu_login_parent_info.js';
import './edu_login_signin.js';
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {EduLoginParams, ParentAccount} from './edu_login_util.js';

/** @enum {string} */
const Steps = {
  WELCOME: 'welcome',
  PARENTS: 'parents',
  PARENT_SIGNIN: 'parent-signin',
  PARENT_INFO: 'parent-info',
  EDU_LOGIN: 'edu-login'
};

/** @type {!Array<Steps>} */
const stepsArray = Object.values(Steps);

Polymer({
  is: 'edu-login-app',

  _template: html`{__html_template__}`,

  properties: {
    /** Mirroring the enum so that it can be used from HTML bindings. */
    Steps: {
      type: Object,
      value: Steps,
    },

    /**
     * Index of the current step displayed.
     * @private {number}
     */
    stepIndex_: {
      type: Number,
      value: 0,
    },

    /**
     * Selected parent account for approving EDU login flow.
     * @private {?ParentAccount}
     */
    selectedParent_: Object,

    /**
     * Login params containing obfuscated Gaia id and Reauth Proof Token of the
     * parent who is approving EDU login flow.
     * @private {?EduLoginParams}
     */
    loginParams_: Object,
  },

  listeners: {
    'go-next': 'onGoNext_',
    'go-back': 'onGoBack_',
  },

  /** @override */
  ready() {
    this.switchViewAtIndex_(this.stepIndex_);
  },

  /** Switches to the next view. */
  onGoNext_() {
    assert(this.stepIndex_ < stepsArray.length - 1);
    ++this.stepIndex_;
    this.switchViewAtIndex_(this.stepIndex_);
  },

  /** Switches to the previous view. */
  onGoBack_() {
    assert(this.stepIndex_ > 0);
    --this.stepIndex_;
    this.switchViewAtIndex_(this.stepIndex_);
  },

  /**
   * Switches to the specified step.
   * @param {number} index of the step to be shown.
   */
  switchViewAtIndex_(index) {
    /** @type {CrViewManagerElement} */ (this.$.viewManager)
        .switchView(stepsArray[index]);
  },
});
