// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/cr_slider/cr_slider.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import './strings.m.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';

/**
 * Enum for the state of `crostini-installer-app`. Not to confused with
 * `installerState`.
 * @enum {string}
 */
const State = {
  PROMPT: 'prompt',
  CONFIGURE: 'configure',
  INSTALLING: 'installing',
  ERROR: 'error',
  CANCELING: 'canceling',
};

const MAX_USERNAME_LENGTH = 32;
const InstallerState = crostini.mojom.InstallerState;
const InstallerError = crostini.mojom.InstallerError;

const UNAVAILABLE_USERNAMES = [
  'root',
  'daemon',
  'bin',
  'sys',
  'sync',
  'games',
  'man',
  'lp',
  'mail',
  'news',
  'uucp',
  'proxy',
  'www-data',
  'backup',
  'list',
  'irc',
  'gnats',
  'nobody',
  '_apt',
  'systemd-timesync',
  'systemd-network',
  'systemd-resolve',
  'systemd-bus-proxy',
  'messagebus',
  'sshd',
  'rtkit',
  'pulse',
  'android-root',
  'chronos-access',
  'android-everybody'
];

Polymer({
  is: 'crostini-installer-app',

  _template: html`{__html_template__}`,

  properties: {
    /** @private {!State} */
    state_: {
      type: String,
      value: State.PROMPT,
    },

    /** @private */
    installerState_: {
      type: Number,
    },

    /** @private */
    installerProgress_: {
      type: Number,
    },

    /** @private */
    error_: {
      type: Number,
    },

    /**
     * Enable the html template to use State.
     * @private
     */
    State: {
      type: Object,
      value: State,
    },

    /**
     * @private
     */
    minDisk_: {
      type: String,
    },

    /**
     * @private
     */
    maxDisk_: {
      type: String,
    },

    /**
     * @private
     */
    defaultDiskSizeTick_: {
      type: Number,
    },

    diskSizeTicks_: {
      type: Array,
    },

    chosenDiskSize_: {
      type: Number,
    },

    username_: {
      type: String,
      value: loadTimeData.getString('defaultContainerUsername')
                 .substring(0, MAX_USERNAME_LENGTH),
      observer: 'onUsernameChanged_',
    },

    usernameError_: {
      type: String,
    },

    /* Enable the html template to access the length */
    MAX_USERNAME_LENGTH: {type: Number, value: MAX_USERNAME_LENGTH},
  },

  /** @override */
  attached() {
    const callbackRouter = BrowserProxy.getInstance().callbackRouter;

    this.listenerIds_ = [
      callbackRouter.onProgressUpdate.addListener(
          (installerState, progressFraction) => {
            this.installerState_ = installerState;
            this.installerProgress_ = progressFraction * 100;
          }),
      callbackRouter.onInstallFinished.addListener(error => {
        if (error === InstallerError.kNone) {
          // Install succeeded.
          this.closeDialog_();
        } else {
          assert(this.state_ === State.INSTALLING);
          this.error_ = error;
          this.state_ = State.ERROR;
        }
      }),
      callbackRouter.onCanceled.addListener(() => this.closeDialog_()),
      callbackRouter.onAmountOfFreeDiskSpace.addListener(
          (ticks, defaultIndex) => {
            if (ticks.length === 0) {
              // Error getting the data we need for the slider e.g. unable to
              // get the amount of free space.
              // TODO(crbug/1043838): Handle this e.g. show an error to the
              // user.
            } else {
              this.defaultDiskSizeTick_ = defaultIndex;
              this.diskSizeTicks_ = ticks;

              this.minDisk_ = ticks[0].label;
              this.maxDisk_ = ticks[ticks.length - 1].label;
            }
          }),
    ];

    document.addEventListener('keyup', event => {
      if (event.key == 'Escape') {
        this.onCancelButtonClick_();
        event.preventDefault();
      }
    });

    BrowserProxy.getInstance().handler.requestAmountOfFreeDiskSpace();
    this.$$('.action-button:not([hidden])').focus();
  },

  /** @override */
  detached() {
    const callbackRouter = BrowserProxy.getInstance().callbackRouter;
    this.listenerIds_.forEach(id => callbackRouter.removeListener(id));
  },

  /** @private */
  onNextButtonClick_() {
    assert(this.state_ === State.PROMPT);
    this.state_ = State.CONFIGURE;
    // Focus the username input and move the cursor to the end.
    this.$.username.select(this.username_.length, this.username_.length);
  },

  /** @private */
  onInstallButtonClick_() {
    assert(this.showInstallButton_(this.state_));
    var diskSize = 0;
    if (loadTimeData.getBoolean('diskResizingEnabled')) {
      diskSize = this.diskSizeTicks_[this.$.diskSlider.value].value;
    }
    this.installerState_ = InstallerState.kStart;
    this.installerProgress_ = 0;
    this.state_ = State.INSTALLING;
    BrowserProxy.getInstance().handler.install(diskSize, this.username_);
  },

  /** @private */
  onCancelButtonClick_() {
    switch (this.state_) {
      case State.PROMPT:
        BrowserProxy.getInstance().handler.cancelBeforeStart();
        this.closeDialog_();
        break;
      case State.CONFIGURE:
        this.state_ = State.PROMPT;
        break;
      case State.INSTALLING:
        this.state_ = State.CANCELING;
        BrowserProxy.getInstance().handler.cancel();
        break;
      case State.ERROR:
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
  closeDialog_() {
    BrowserProxy.getInstance().handler.close();
  },

  /**
   * @param {State} state
   * @returns {string}
   * @private
   */
  getTitle_(state) {
    let titleId;
    switch (state) {
      case State.PROMPT:
      case State.CONFIGURE:
        titleId = 'promptTitle';
        break;
      case State.INSTALLING:
        titleId = 'installingTitle';
        break;
      case State.ERROR:
        titleId = 'errorTitle';
        break;
      case State.CANCELING:
        titleId = 'cancelingTitle';
        break;
      default:
        assertNotReached();
    }
    return loadTimeData.getString(/** @type {string} */ (titleId));
  },

  /**
   * @param {*} value1
   * @param {*} value2
   * @returns {boolean}
   * @private
   */
  eq_(value1, value2) {
    return value1 === value2;
  },

  /**
   * @param {State} state
   * @returns {boolean}
   * @private
   */
  showInstallButton_(state) {
    if (this.configurePageAccessible_()) {
      return state === State.CONFIGURE || state === State.ERROR;
    } else {
      return state === State.PROMPT || state === State.ERROR;
    }
  },

  /**
   * @param {State} state
   * @param {string} username
   * @param {string} usernameError
   * @returns {boolean}
   * @private
   */
  disableInstallButton_(state, username, usernameError) {
    if (state === State.CONFIGURE) {
      return !username || !!usernameError;
    }
    return false;
  },

  /**
   * @param {State} state
   * @returns {boolean}
   * @private
   */
  showNextButton_(state) {
    return this.configurePageAccessible_() && state === State.PROMPT;
  },

  /**
   * @param {State} state
   * @returns {string}
   * @private
   */
  getInstallButtonLabel_(state) {
    if (!this.configurePageAccessible_() && state === State.PROMPT) {
      return loadTimeData.getString('install');
    }
    switch (state) {
      case State.CONFIGURE:
        return loadTimeData.getString('install');
      case State.ERROR:
        return loadTimeData.getString('retry');
      default:
        return '';
    }
  },

  /**
   * @param {InstallerState} installerState
   * @returns {string}
   * @private
   */
  getProgressMessage_(installerState) {
    let messageId = null;
    switch (installerState) {
      case InstallerState.kStart:
        break;
      case InstallerState.kInstallImageLoader:
        messageId = 'loadTerminaMessage';
        break;
      case InstallerState.kStartConcierge:
        messageId = 'startConciergeMessage';
        break;
      case InstallerState.kCreateDiskImage:
        messageId = 'createDiskImageMessage';
        break;
      case InstallerState.kStartTerminaVm:
        messageId = 'startTerminaVmMessage';
        break;
      case InstallerState.kCreateContainer:
        // TODO(crbug.com/1015722): we are using the same message as for
        // |START_CONTAINER|, which is weird because user is going to see
        // message "start container" then "setup container" and then "start
        // container" again.
        messageId = 'startContainerMessage';
        break;
      case InstallerState.kSetupContainer:
        messageId = 'setupContainerMessage';
        break;
      case InstallerState.kStartContainer:
        messageId = 'startContainerMessage';
        break;
      case InstallerState.kConfigureContainer:
        messageId = 'configureContainerMessage';
        break;
      case InstallerState.kFetchSshKeys:
        messageId = 'fetchSshKeysMessage';
        break;
      case InstallerState.kMountContainer:
        messageId = 'mountContainerMessage';
        break;
      default:
        assertNotReached();
    }

    return messageId ? loadTimeData.getString(messageId) : '';
  },

  /**
   * @param {InstallerError} error
   * @returns {string}
   * @private
   */
  getErrorMessage_(error) {
    let messageId = null;
    switch (error) {
      case InstallerError.kErrorLoadingTermina:
        messageId = 'loadTerminaError';
        break;
      case InstallerError.kErrorStartingConcierge:
        messageId = 'startConciergeError';
        break;
      case InstallerError.kErrorCreatingDiskImage:
        messageId = 'createDiskImageError';
        break;
      case InstallerError.kErrorStartingTermina:
        messageId = 'startTerminaVmError';
        break;
      case InstallerError.kErrorStartingContainer:
        messageId = 'startContainerError';
        break;
      case InstallerError.kErrorConfiguringContainer:
        messageId = 'configureContainerError';
        break;
      case InstallerError.kErrorOffline:
        messageId = 'offlineError';
        break;
      case InstallerError.kErrorFetchingSshKeys:
        messageId = 'fetchSshKeysError';
        break;
      case InstallerError.kErrorMountingContainer:
        messageId = 'mountContainerError';
        break;
      case InstallerError.kErrorSettingUpContainer:
        messageId = 'setupContainerError';
        break;
      case InstallerError.kErrorInsufficientDiskSpace:
        messageId = 'insufficientDiskError';
        break;
      case InstallerError.kErrorCreateContainer:
        messageId = 'setupContainerError';
        break;
      case InstallerError.kErrorUnknown:
        messageId = 'unknownError';
        break;
      default:
        assertNotReached();
    }

    return messageId ? loadTimeData.getString(messageId) : '';
  },

  /**
   * @private
   */
  configurePageAccessible_() {
    return this.showDiskResizing_() || this.showUsernameSelection_();
  },

  /**
   * @private
   */
  showDiskResizing_() {
    return loadTimeData.getBoolean('diskResizingEnabled');
  },

  /**
   * @private
   */
  showUsernameSelection_() {
    return loadTimeData.getBoolean('crostiniCustomUsername');
  },

  /**
   * @private
   */
  getConfigureMessageTitle_() {
    // If the flags only allow username config, then we show a username specific
    // subtitle instead of a generic configure subtitle.
    if (this.showUsernameSelection_() && !this.showDiskResizing_())
      return loadTimeData.getString('usernameMessage');
    return loadTimeData.getString('configureMessage');
  },

  /** @private */
  onUsernameChanged_(username, oldUsername) {
    if (!username) {
      this.usernameError_ = '';
    } else if (UNAVAILABLE_USERNAMES.includes(username)) {
      this.usernameError_ =
          loadTimeData.getStringF('usernameNotAvailableError', username);
    } else if (!/^[a-z_]/.test(username)) {
      this.usernameError_ =
          loadTimeData.getString('usernameInvalidFirstCharacterError');
    } else if (!/^[a-z0-9_-]*$/.test(username)) {
      this.usernameError_ =
          loadTimeData.getString('usernameInvalidCharactersError');
    } else {
      this.usernameError_ = '';
    }
  },

  /** @private */
  getCancelButtonLabel_(state) {
    return loadTimeData.getString(
        state === State.CONFIGURE ? 'back' : 'cancel');
  }
});
