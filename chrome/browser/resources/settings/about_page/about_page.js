// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-about-page' contains version and OS related
 * information.
 */

Polymer({
  is: 'settings-about-page',

  behaviors: [
    WebUIListenerBehavior,
    I18nBehavior,
  ],

  properties: {
    /** @private {?UpdateStatusChangedEvent} */
    currentUpdateStatusEvent_: {
      type: Object,
      value: {
        message: '',
        progress: 0,
        rollback: false,
        status: UpdateStatus.DISABLED
      },
    },

    /**
     * Whether the browser/ChromeOS is managed by their organization
     * through enterprise policies.
     * @private
     */
    isManaged_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('isManaged');
      },
    },

    // <if expr="_google_chrome and is_macosx">
    /** @private {!PromoteUpdaterStatus} */
    promoteUpdaterStatus_: Object,
    // </if>

    // <if expr="not chromeos">
    /** @private {!{obsolete: boolean, endOfLine: boolean}} */
    obsoleteSystemInfo_: {
      type: Object,
      value: function() {
        return {
          obsolete: loadTimeData.getBoolean('aboutObsoleteNowOrSoon'),
          endOfLine: loadTimeData.getBoolean('aboutObsoleteEndOfTheLine'),
        };
      },
    },

    /** @private */
    showUpdateStatus_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    showButtonContainer_: Boolean,

    /** @private */
    showRelaunch_: {
      type: Boolean,
      value: false,
    },
    // </if>
  },

  observers: [
    // <if expr="not chromeos">
    'updateShowUpdateStatus_(' +
        'obsoleteSystemInfo_, currentUpdateStatusEvent_)',
    'updateShowRelaunch_(currentUpdateStatusEvent_)',
    'updateShowButtonContainer_(showRelaunch_)',
    // </if>
  ],

  /** @private {?settings.AboutPageBrowserProxy} */
  aboutBrowserProxy_: null,

  /** @private {?settings.LifetimeBrowserProxy} */
  lifetimeBrowserProxy_: null,

  /** @override */
  attached: function() {
    this.aboutBrowserProxy_ = settings.AboutPageBrowserProxyImpl.getInstance();
    this.aboutBrowserProxy_.pageReady();

    this.lifetimeBrowserProxy_ =
        settings.LifetimeBrowserProxyImpl.getInstance();

    // <if expr="not chromeos">
    this.startListening_();
    if (settings.getQueryParameters().get('checkForUpdate') == 'true') {
      this.onUpdateStatusChanged_({status: UpdateStatus.CHECKING});
      this.aboutBrowserProxy_.requestUpdate();
    }
    // </if>
  },

  // <if expr="not chromeos">
  /** @private */
  startListening_: function() {
    this.addWebUIListener(
        'update-status-changed', this.onUpdateStatusChanged_.bind(this));
    // <if expr="_google_chrome and is_macosx">
    this.addWebUIListener(
        'promotion-state-changed',
        this.onPromoteUpdaterStatusChanged_.bind(this));
    // </if>
    this.aboutBrowserProxy_.refreshUpdateStatus();
  },

  /**
   * @param {!UpdateStatusChangedEvent} event
   * @private
   */
  onUpdateStatusChanged_: function(event) {
    this.currentUpdateStatusEvent_ = event;
  },
  // </if>

  // <if expr="_google_chrome and is_macosx">
  /**
   * @param {!PromoteUpdaterStatus} status
   * @private
   */
  onPromoteUpdaterStatusChanged_: function(status) {
    this.promoteUpdaterStatus_ = status;
  },

  /**
   * If #promoteUpdater isn't disabled, trigger update promotion.
   * @private
   */
  onPromoteUpdaterTap_: function() {
    // This is necessary because #promoteUpdater is not a button, so by default
    // disable doesn't do anything.
    if (this.promoteUpdaterStatus_.disabled) {
      return;
    }
    this.aboutBrowserProxy_.promoteUpdater();
  },
  // </if>

  /**
   * @param {!Event} event
   * @private
   */
  onLearnMoreTap_: function(event) {
    // Stop the propagation of events, so that clicking on links inside
    // actionable items won't trigger action.
    event.stopPropagation();
  },

  /** @private */
  onReleaseNotesTap_: function() {
    this.aboutBrowserProxy_.launchReleaseNotes();
  },

  /** @private */
  onHelpTap_: function() {
    this.aboutBrowserProxy_.openHelpPage();
  },

  /** @private */
  onRelaunchTap_: function() {
    this.lifetimeBrowserProxy_.relaunch();
  },

  // <if expr="not chromeos">
  /** @private */
  updateShowUpdateStatus_: function() {
    if (this.obsoleteSystemInfo_.endOfLine) {
      this.showUpdateStatus_ = false;
      return;
    }
    this.showUpdateStatus_ =
        this.currentUpdateStatusEvent_.status != UpdateStatus.DISABLED;
  },

  /**
   * Hide the button container if all buttons are hidden, otherwise the
   * container displays an unwanted border (see separator class).
   * @private
   */
  updateShowButtonContainer_: function() {
    this.showButtonContainer_ = this.showRelaunch_;
  },

  /** @private */
  updateShowRelaunch_: function() {
    this.showRelaunch_ = this.checkStatus_(UpdateStatus.NEARLY_UPDATED);
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowLearnMoreLink_: function() {
    return this.currentUpdateStatusEvent_.status == UpdateStatus.FAILED;
  },

  /**
   * @return {string}
   * @private
   */
  getUpdateStatusMessage_: function() {
    switch (this.currentUpdateStatusEvent_.status) {
      case UpdateStatus.CHECKING:
      case UpdateStatus.NEED_PERMISSION_TO_UPDATE:
        return this.i18nAdvanced('aboutUpgradeCheckStarted');
      case UpdateStatus.NEARLY_UPDATED:
        return this.i18nAdvanced('aboutUpgradeRelaunch');
      case UpdateStatus.UPDATED:
        return this.i18nAdvanced('aboutUpgradeUpToDate');
      case UpdateStatus.UPDATING:
        assert(typeof this.currentUpdateStatusEvent_.progress == 'number');
        const progressPercent = this.currentUpdateStatusEvent_.progress + '%';

        if (this.currentUpdateStatusEvent_.progress > 0) {
          // NOTE(dbeam): some platforms (i.e. Mac) always send 0% while
          // updating (they don't support incremental upgrade progress). Though
          // it's certainly quite possible to validly end up here with 0% on
          // platforms that support incremental progress, nobody really likes
          // seeing that they're 0% done with something.
          return this.i18nAdvanced('aboutUpgradeUpdatingPercent', {
            substitutions: [progressPercent],
          });
        }
        return this.i18nAdvanced('aboutUpgradeUpdating');
      default:
        function formatMessage(msg) {
          return parseHtmlSubset('<b>' + msg + '</b>', ['br', 'pre'])
              .firstChild.innerHTML;
        }
        let result = '';
        const message = this.currentUpdateStatusEvent_.message;
        if (message) {
          result += formatMessage(message);
        }
        const connectMessage = this.currentUpdateStatusEvent_.connectionTypes;
        if (connectMessage) {
          result += '<div>' + formatMessage(connectMessage) + '</div>';
        }
        return result;
    }
  },

  /**
   * @return {?string}
   * @private
   */
  getUpdateStatusIcon_: function() {
    // If this platform has reached the end of the line, display an error icon
    // and ignore UpdateStatus.
    if (this.obsoleteSystemInfo_.endOfLine) {
      return 'cr:error';
    }

    switch (this.currentUpdateStatusEvent_.status) {
      case UpdateStatus.DISABLED_BY_ADMIN:
        return 'cr20:domain';
      case UpdateStatus.FAILED:
        return 'cr:error';
      case UpdateStatus.UPDATED:
      case UpdateStatus.NEARLY_UPDATED:
        return 'settings:check-circle';
      default:
        return null;
    }
  },

  /**
   * @return {?string}
   * @private
   */
  getThrobberSrcIfUpdating_: function() {
    if (this.obsoleteSystemInfo_.endOfLine) {
      return null;
    }

    switch (this.currentUpdateStatusEvent_.status) {
      case UpdateStatus.CHECKING:
      case UpdateStatus.UPDATING:
        return 'chrome://resources/images/throbber_small.svg';
      default:
        return null;
    }
  },
  // </if>

  /**
   * @param {!UpdateStatus} status
   * @return {boolean}
   * @private
   */
  checkStatus_: function(status) {
    return this.currentUpdateStatusEvent_.status == status;
  },

  /** @private */
  onManagementPageTap_: function() {
    window.location.href = 'chrome://management';
  },

  // <if expr="chromeos">
  /**
   * @return {string}
   * @private
   */
  getUpdateOsSettingsLink_: function() {
    // Note: This string contains raw HTML and thus requires i18nAdvanced().
    // Since the i18n template syntax (e.g., $i18n{}) does not include an
    // "advanced" version, it's not possible to inline this link directly in the
    // HTML.
    return this.i18nAdvanced('aboutUpdateOsSettingsLink');
  },
  // </if>

  /** @private */
  onProductLogoTap_: function() {
    this.$['product-logo'].animate(
        {
          transform: ['none', 'rotate(-10turn)'],
        },
        {
          duration: 500,
          easing: 'cubic-bezier(1, 0, 0, 1)',
        });
  },

  // <if expr="_google_chrome">
  /** @private */
  onReportIssueTap_: function() {
    this.aboutBrowserProxy_.openFeedbackDialog();
  },
  // </if>

  /**
   * @return {boolean}
   * @private
   */
  shouldShowIcons_: function() {
    // <if expr="not chromeos">
    if (this.obsoleteSystemInfo_.endOfLine) {
      return true;
    }
    // </if>
    return this.showUpdateStatus_;
  },
});
