// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  const Item = Polymer({
    is: 'downloads-item',

    behaviors: [
      cr.ui.FocusRowBehavior,
    ],

    /** Used by FocusRowBehavior. */
    overrideCustomEquivalent: true,

    properties: {
      /** @type {!downloads.Data} */
      data: Object,

      /** @private */
      completelyOnDisk_: {
        computed: 'computeCompletelyOnDisk_(' +
            'data.state, data.fileExternallyRemoved)',
        type: Boolean,
        value: true,
      },

      /** @private */
      controlledBy_: {
        computed: 'computeControlledBy_(data.byExtId, data.byExtName)',
        type: String,
        value: '',
      },

      /** @private */
      controlRemoveFromListAriaLabel_: {
        type: String,
        computed: 'computeControlRemoveFromListAriaLabel_(data.fileName)',
      },

      /** @private */
      isActive_: {
        computed: 'computeIsActive_(' +
            'data.state, data.fileExternallyRemoved)',
        type: Boolean,
        value: true,
      },

      /** @private */
      isDangerous_: {
        computed: 'computeIsDangerous_(data.state)',
        type: Boolean,
        value: false,
      },

      /** @private */
      isMalware_: {
        computed: 'computeIsMalware_(isDangerous_, data.dangerType)',
        type: Boolean,
        value: false,
      },

      /** @private */
      isInProgress_: {
        computed: 'computeIsInProgress_(data.state)',
        type: Boolean,
        value: false,
      },

      /** @private */
      pauseOrResumeText_: {
        computed: 'computePauseOrResumeText_(isInProgress_, data.resume)',
        type: String,
        observer: 'updatePauseOrResumeClass_',
      },

      /** @private */
      showCancel_: {
        computed: 'computeShowCancel_(data.state)',
        type: Boolean,
        value: false,
      },

      /** @private */
      showProgress_: {
        computed: 'computeShowProgress_(showCancel_, data.percent)',
        type: Boolean,
        value: false,
      },

      useFileIcon_: Boolean,
    },

    observers: [
      // TODO(dbeam): this gets called way more when I observe data.byExtId
      // and data.byExtName directly. Why?
      'observeControlledBy_(controlledBy_)',
      'observeIsDangerous_(isDangerous_, data)',
      'restoreFocusAfterCancelIfNeeded_(data)',
    ],

    /** @private {downloads.mojom.PageHandlerInterface} */
    mojoHandler_: null,

    /** @private {boolean} */
    restoreFocusAfterCancel_: false,

    /** @override */
    ready: function() {
      this.mojoHandler_ = downloads.BrowserProxy.getInstance().handler;
      this.content = this.$.content;
    },

    focusOnRemoveButton: function() {
      cr.ui.focusWithoutInk(this.$.remove);
    },

    /** Overrides FocusRowBehavior. */
    getCustomEquivalent: function(sampleElement) {
      if (sampleElement.getAttribute('focus-type') == 'cancel') {
        return this.$$('[focus-type="retry"]');
      }
      if (sampleElement.getAttribute('focus-type') == 'retry') {
        return this.$$('[focus-type="pauseOrResume"]');
      }
      return null;
    },

    /** @return {!HTMLElement} */
    getFileIcon: function() {
      return assert(this.$['file-icon']);
    },

    /**
     * @param {string} url
     * @return {string} A reasonably long URL.
     * @private
     */
    chopUrl_: function(url) {
      return url.slice(0, 300);
    },

    /** @private */
    computeClass_: function() {
      const classes = [];

      if (this.isActive_) {
        classes.push('is-active');
      }

      if (this.isDangerous_) {
        classes.push('dangerous');
      }

      if (this.showProgress_) {
        classes.push('show-progress');
      }

      return classes.join(' ');
    },

    /**
     * @return {boolean}
     * @private
     */
    computeCompletelyOnDisk_: function() {
      return this.data.state == downloads.States.COMPLETE &&
          !this.data.fileExternallyRemoved;
    },

    /**
     * @return {string}
     * @private
     */
    computeControlledBy_: function() {
      if (!this.data.byExtId || !this.data.byExtName) {
        return '';
      }

      const url = `chrome://extensions/?id=${this.data.byExtId}`;
      const name = this.data.byExtName;
      return loadTimeData.getStringF('controlledByUrl', url, HTMLEscape(name));
    },

    /**
     * @return {string}
     * @private
     */
    computeControlRemoveFromListAriaLabel_: function() {
      return loadTimeData.getStringF(
          'controlRemoveFromListAriaLabel', this.data.fileName);
    },

    /**
     * @return {string}
     * @private
     */
    computeDate_: function() {
      assert(typeof this.data.hideDate == 'boolean');
      if (this.data.hideDate) {
        return '';
      }
      return assert(this.data.sinceString || this.data.dateString);
    },

    /**
     * @return {string}
     * @private
     */
    computeDescription_: function() {
      const data = this.data;

      switch (data.state) {
        case downloads.States.COMPLETE:
          switch (data.dangerType) {
            case downloads.DangerType.DEEP_SCANNED_SAFE:
              return loadTimeData.getString('deepScannedSafeDesc');
            case downloads.DangerType.DEEP_SCANNED_OPENED_DANGEROUS:
              return loadTimeData.getString('deepScannedOpenedDangerousDesc');
          }
          break;

        case downloads.States.DANGEROUS:
          const fileName = data.fileName;
          switch (data.dangerType) {
            case downloads.DangerType.DANGEROUS_FILE:
              return loadTimeData.getString('dangerFileDesc');

            case downloads.DangerType.DANGEROUS_URL:
            case downloads.DangerType.DANGEROUS_CONTENT:
            case downloads.DangerType.DANGEROUS_HOST:
              return loadTimeData.getString('dangerDownloadDesc');

            case downloads.DangerType.UNCOMMON_CONTENT:
              return loadTimeData.getString('dangerUncommonDesc');

            case downloads.DangerType.POTENTIALLY_UNWANTED:
              return loadTimeData.getString('dangerSettingsDesc');

            case downloads.DangerType.SENSITIVE_CONTENT_WARNING:
              return loadTimeData.getString('sensitiveContentWarningDesc');
          }
          break;

        case downloads.States.IN_PROGRESS:
        case downloads.States.PAUSED:  // Fallthrough.
          return data.progressStatusText;

        case downloads.States.INTERRUPTED:
          switch (data.dangerType) {
            case downloads.DangerType.SENSITIVE_CONTENT_BLOCK:
              return loadTimeData.getString('sensitiveContentBlockedDesc');
            case downloads.DangerType.BLOCKED_TOO_LARGE:
              return loadTimeData.getString('blockedTooLargeDesc');
            case downloads.DangerType.BLOCKED_PASSWORD_PROTECTED:
              return loadTimeData.getString('blockedPasswordProtectedDesc');
          }
      }

      return '';
    },

    /**
     * @return {string}
     * @private
     */
    computeIcon_: function() {
      if (this.data) {
        const dangerType = this.data.dangerType;

        if ((loadTimeData.getBoolean('requestsApVerdicts') &&
             dangerType == downloads.DangerType.UNCOMMON_CONTENT) ||
            dangerType == downloads.DangerType.SENSITIVE_CONTENT_WARNING) {
          return 'cr:error';
        }

        const WARNING_TYPES = [
          downloads.DangerType.SENSITIVE_CONTENT_BLOCK,
          downloads.DangerType.BLOCKED_TOO_LARGE,
          downloads.DangerType.BLOCKED_PASSWORD_PROTECTED,
        ];
        if (WARNING_TYPES.includes(dangerType)) {
          return 'cr:warning';
        }
      }
      if (this.isDangerous_) {
        return 'cr:warning';
      }
      if (!this.useFileIcon_) {
        return 'cr:insert-drive-file';
      }
      return '';
    },

    /**
     * @return {boolean}
     * @private
     */
    computeIsActive_: function() {
      return this.data.state != downloads.States.CANCELLED &&
          this.data.state != downloads.States.INTERRUPTED &&
          !this.data.fileExternallyRemoved;
    },

    /**
     * @return {boolean}
     * @private
     */
    computeIsDangerous_: function() {
      return this.data.state == downloads.States.DANGEROUS;
    },

    /**
     * @return {boolean}
     * @private
     */
    computeIsInProgress_: function() {
      return this.data.state == downloads.States.IN_PROGRESS;
    },

    /**
     * @return {boolean}
     * @private
     */
    computeIsMalware_: function() {
      return this.isDangerous_ &&
          (this.data.dangerType == downloads.DangerType.DANGEROUS_CONTENT ||
           this.data.dangerType == downloads.DangerType.DANGEROUS_HOST ||
           this.data.dangerType == downloads.DangerType.DANGEROUS_URL ||
           this.data.dangerType == downloads.DangerType.POTENTIALLY_UNWANTED);
    },

    /** @private */
    toggleButtonClass_: function() {
      this.$$('#pauseOrResume')
          .classList.toggle(
              'action-button',
              this.pauseOrResumeText_ ===
                  loadTimeData.getString('controlResume'));
    },

    /** @private */
    updatePauseOrResumeClass_: function() {
      if (!this.pauseOrResumeText_) {
        return;
      }

      // Wait for dom-if to switch to true, in case the text has just changed
      // from empty.
      Polymer.RenderStatus.beforeNextRender(
          this, () => this.toggleButtonClass_());
    },

    /**
     * @return {string}
     * @private
     */
    computePauseOrResumeText_: function() {
      if (this.data === undefined) {
        return '';
      }

      if (this.isInProgress_) {
        return loadTimeData.getString('controlPause');
      }
      if (this.data.resume) {
        return loadTimeData.getString('controlResume');
      }
      return '';
    },

    /**
     * @return {string}
     * @private
     */
    computeRemoveStyle_: function() {
      const canDelete = loadTimeData.getBoolean('allowDeletingHistory');
      const hideRemove = this.isDangerous_ || this.showCancel_ || !canDelete;
      return hideRemove ? 'visibility: hidden' : '';
    },

    /**
     * @return {boolean}
     * @private
     */
    computeShowCancel_: function() {
      return this.data.state == downloads.States.IN_PROGRESS ||
          this.data.state == downloads.States.PAUSED;
    },

    /**
     * @return {boolean}
     * @private
     */
    computeShowProgress_: function() {
      return this.showCancel_ && this.data.percent >= -1;
    },

    /**
     * @return {string}
     * @private
     */
    computeTag_: function() {
      switch (this.data.state) {
        case downloads.States.CANCELLED:
          return loadTimeData.getString('statusCancelled');

        case downloads.States.INTERRUPTED:
          return this.data.lastReasonText;

        case downloads.States.COMPLETE:
          return this.data.fileExternallyRemoved ?
              loadTimeData.getString('statusRemoved') :
              '';
      }

      return '';
    },

    /**
     * @return {boolean}
     * @private
     */
    isIndeterminate_: function() {
      return this.data.percent == -1;
    },

    /** @private */
    observeControlledBy_: function() {
      this.$['controlled-by'].innerHTML = this.controlledBy_;
      if (this.controlledBy_) {
        const link = this.$$('#controlled-by a');
        link.setAttribute('focus-row-control', '');
        link.setAttribute('focus-type', 'controlledBy');
      }
    },

    /** @private */
    observeIsDangerous_: function() {
      if (!this.data) {
        return;
      }

      const OVERRIDDEN_ICON_TYPES = [
        downloads.DangerType.SENSITIVE_CONTENT_BLOCK,
        downloads.DangerType.BLOCKED_TOO_LARGE,
        downloads.DangerType.BLOCKED_PASSWORD_PROTECTED,
      ];

      if (this.isDangerous_) {
        this.$.url.removeAttribute('href');
        this.useFileIcon_ = false;
      } else if (OVERRIDDEN_ICON_TYPES.includes(this.data.dangerType)) {
        this.useFileIcon_ = false;
      } else {
        this.$.url.href = assert(this.data.url);
        const path = this.data.filePath;
        downloads.IconLoader.getInstance()
            .loadIcon(this.$['file-icon'], path)
            .then(success => {
              if (path == this.data.filePath) {
                this.useFileIcon_ = success;
              }
            });
      }
    },

    /** @private */
    onCancelTap_: function() {
      this.restoreFocusAfterCancel_ = true;
      this.mojoHandler_.cancel(this.data.id);
    },

    /** @private */
    onDiscardDangerousTap_: function() {
      this.mojoHandler_.discardDangerous(this.data.id);
    },

    /**
     * @private
     * @param {Event} e
     */
    onDragStart_: function(e) {
      e.preventDefault();
      this.mojoHandler_.drag(this.data.id);
    },

    /**
     * @param {Event} e
     * @private
     */
    onFileLinkTap_: function(e) {
      e.preventDefault();
      this.mojoHandler_.openFileRequiringGesture(this.data.id);
    },

    /** @private */
    onUrlTap_: function() {
      chrome.send('metricsHandler:recordAction',
        ['Downloads_OpenUrlOfDownloadedItem']);
    },

    /** @private */
    onPauseOrResumeTap_: function() {
      if (this.isInProgress_) {
        this.mojoHandler_.pause(this.data.id);
      } else {
        this.mojoHandler_.resume(this.data.id);
      }
    },

    /** @private */
    onRemoveTap_: function() {
      const pieces = loadTimeData.getSubstitutedStringPieces(
          loadTimeData.getString('toastRemovedFromList'), this.data.fileName);
      pieces.forEach(p => {
        // Make the file name collapsible.
        p.collapsible = !!p.arg;
      });
      cr.toastManager.getInstance().showForStringPieces(
          /**
           * @type {!Array<{collapsible: boolean,
           *                 value: string,
           *                 arg: (string|null)}>}
           */
          (pieces), true);
      this.mojoHandler_.remove(this.data.id);
    },

    /** @private */
    onRetryTap_: function() {
      this.mojoHandler_.retryDownload(this.data.id);
    },

    /** @private */
    onSaveDangerousTap_: function() {
      this.mojoHandler_.saveDangerousRequiringGesture(this.data.id);
    },

    /** @private */
    onShowTap_: function() {
      this.mojoHandler_.show(this.data.id);
    },

    /** @private */
    restoreFocusAfterCancelIfNeeded_: function() {
      if (!this.restoreFocusAfterCancel_) {
        return;
      }
      this.restoreFocusAfterCancel_ = false;
      setTimeout(() => {
        const element = this.getFocusRow().getFirstFocusable('retry');
        if (element) {
          element.focus();
        }
      });
    }
  });

  return {Item: Item};
});
