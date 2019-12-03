// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import './strings.m.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';

/**
 * Enum for the state of `crostini-upgrader-app`.
 * @enum {string}
 */
const State = {
  PROMPT: 'prompt',
  BACKUP: 'backup',
  BACKUP_SUCCEEDED: 'backupSucceeded',
  UPGRADING: 'upgrading',
  ERROR: 'error',
  CANCELING: 'canceling',
  SUCCEEDED: 'succeeded',
};


Polymer({
  is: 'crostini-upgrader-app',

  _template: html`{__html_template__}`,

  properties: {
    /** @private {State} */
    state_: {
      type: String,
      value: State.PROMPT,
    },

    /** @private */
    backupCheckboxChecked_: {
      type: Boolean,
      value: true,
    },

    /** @private */
    backupProgress_: {
      type: Number,
    },

    /** @private */
    upgraderProgress_: {
      type: Number,
    },

    /** @private */
    progressMessages_: {
      type: Array,
    },

    /**
     * Enable the html template to use State.
     * @private
     */
    State: {
      type: Object,
      value: State,
    },
  },

  /** @override */
  attached: function() {
    const callbackRouter = BrowserProxy.getInstance().callbackRouter;

    this.listenerIds_ = [
      callbackRouter.onBackupProgress.addListener((percent) => {
        assert(this.state_ === State.BACKUP);
        this.backupProgress_ = percent;
      }),
      callbackRouter.onBackupSucceeded.addListener(() => {
        assert(this.state_ === State.BACKUP);
        this.state_ = State.BACKUP_SUCCEEDED;
        // We do a short (2 second) interstitial display of the backup success
        // message before continuing the upgrade.
        setTimeout(() => {
          this.startUpgrade_();
        }, 2000);
      }),
      callbackRouter.onBackupFailed.addListener(() => {
        assert(this.state_ === State.BACKUP);
        this.state_ = State.ERROR;
      }),
      callbackRouter.onUpgradeProgress.addListener((progressMessages) => {
        assert(this.state_ === State.UPGRADING);
        this.progressMessages_.push(...progressMessages);
      }),
      callbackRouter.onUpgradeSucceeded.addListener(() => {
        assert(this.state_ === State.UPGRADING);
        this.state_ = State.SUCCEEDED;
      }),
      callbackRouter.onUpgradeFailed.addListener(() => {
        assert(this.state_ === State.UPGRADING);
        this.state_ = State.ERROR;
      }),
      callbackRouter.onCanceled.addListener(() => {
        this.closeDialog_();
      }),
    ];

    document.addEventListener('keyup', event => {
      if (event.key == 'Escape') {
        this.onCancelButtonClick_();
        event.preventDefault();
      }
    });

    this.$$('.action-button').focus();
  },

  /** @override */
  detached: function() {
    const callbackRouter = BrowserProxy.getInstance().callbackRouter;
    this.listenerIds_.forEach(id => callbackRouter.removeListener(id));
  },

  /** @private */
  onActionButtonClick_: function() {
    switch (this.state_) {
      case State.SUCCEEDED:
        BrowserProxy.getInstance().handler.launch();
        this.closeDialog_();
        break;
      case State.PROMPT:
        if (this.backupCheckboxChecked_) {
          this.startBackup_();
        } else {
          this.startUpgrade_();
        }
        break;
    }
  },

  /** @private */
  onCancelButtonClick_: function() {
    switch (this.state_) {
      case State.PROMPT:
        BrowserProxy.getInstance().handler.cancelBeforeStart();
        break;
      case State.UPGRADING:
        this.state_ = State.CANCELING;
        BrowserProxy.getInstance().handler.cancel();
        break;
      case State.ERROR:
      case State.SUCCEEDED:
        this.closeDialog_();
        break;
      case State.CANCELING:
        // Although cancel button has been disabled, we can reach here if users
        // press <esc> key.
        break;
      default:
        assertNotReached();
    }
  },


  /** @private */
  startBackup_: function() {
    this.state_ = State.BACKUP;
    BrowserProxy.getInstance().handler.backup();
  },

  /** @private */
  startUpgrade_: function() {
    this.state_ = State.UPGRADING;
    BrowserProxy.getInstance().handler.upgrade();
  },

  /** @private */
  closeDialog_: function() {
    BrowserProxy.getInstance().handler.close();
  },

  /**
   * @param {State} state1
   * @param {State} state2
   * @return {boolean}
   * @private
   */
  isState_: function(state1, state2) {
    return state1 === state2;
  },

  /**
   * @param {State} state
   * @return {boolean}
   * @private
   */
  canDoAction_: function(state) {
    switch (state) {
      case State.PROMPT:
      case State.SUCCEEDED:
        return true;
    }
    return false;
  },

  /**
   * @param {State} state
   * @return {boolean}
   * @private
   */
  canCancel_: function(state) {
    switch (state) {
      case State.BACKUP:
      case State.BACKUP_SUCCEEDED:
      case State.CANCELING:
        return false;
    }
    return true;
  },

  /**
   * @return {string}
   * @private
   */
  getTitle_: function() {
    let titleId;
    switch (this.state_) {
      case State.PROMPT:
        titleId = 'promptTitle';
        break;
      case State.BACKUP:
        titleId = 'backingUpTitle';
        break;
      case State.BACKUP_SUCCEEDED:
        titleId = 'backupSucceededTitle';
        break;
      case State.UPGRADING:
        titleId = 'upgradingTitle';
        break;
      case State.ERROR:
        titleId = 'errorTitle';
        break;
      case State.CANCELING:
        titleId = 'cancelingTitle';
        break;
      case State.SUCCEEDED:
        titleId = 'succeededTitle';
        break;
      default:
        assertNotReached();
    }
    return loadTimeData.getString(/** @type {string} */ (titleId));
  },

  /**
   * @param {State} state
   * @return {string}
   * @private
   */
  getActionButtonLabel_: function(state) {
    switch (state) {
      case State.PROMPT:
        return loadTimeData.getString('upgrade');
      case State.ERROR:
        return loadTimeData.getString('cancel');
      case State.SUCCEEDED:
        return loadTimeData.getString('launch');
    }
    return '';
  },

  /**
   * @param {State} state
   * @return {string}
   * @private
   */
  getCancelButtonLabel_: function(state) {
    switch (state) {
      case State.SUCCEEDED:
        return loadTimeData.getString('close');
      default:
        return loadTimeData.getString('cancel');
    }
  },

  /**
   * @param {State} state
   * @return {string}
   * @private
   */
  getProgressMessage_: function(state) {
    let messageId = null;
    switch (state) {
      case State.PROMPT:
        messageId = 'promptMessage';
        break;
      case State.BACKUP:
        messageId = 'backingUpMessage';
        break;
      case State.BACKUP_SUCCEEDED:
        messageId = 'backupSucceededMessage';
        break;
      case State.UPGRADING:
        messageId = 'upgradingMessage';
        break;
      case State.SUCCEEDED:
        messageId = 'succeededMessage';
    }
    return messageId ? loadTimeData.getString(messageId) : '';
  },

  /**
   * @param {State} state
   * @return {string}
   * @private
   */
  getErrorMessage_: function(state) {
    // TODO(nverne): Surface error messages once we have better details.
    let messageId = null;
    return messageId ? loadTimeData.getString(messageId) : '';
  },

  /**
   * @param {State} state
   * @return {string}
   * @private
   */
  getIllustrationStyle_: function(state) {
    switch (state) {
      case State.BACKUP_SUCCEEDED:
      case State.ERROR:
        return 'img-square-illustration';
    }
    return 'img-rect-illustration';
  },

  /**
   * @param {State} state
   * @return {string}
   * @private
   */
  getIllustrationURI_: function(state) {
    switch (state) {
      case State.BACKUP_SUCCEEDED:
        return 'images/success_illustration.png';
      case State.ERROR:
        return 'images/error_illustration.png';
    }
    return 'images/linux_illustration.png';
  },

});
