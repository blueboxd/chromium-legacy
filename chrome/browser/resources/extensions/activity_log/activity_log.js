// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Subpages/views for the activity log. HISTORY shows extension activities
 * fetched from the activity log database with some fields such as args
 * omitted. STREAM displays extension activities in a more verbose format in
 * real time. NONE is used when user is away from the page.
 * @enum {number}
 */
const ActivityLogSubpage = {
  NONE: -1,
  HISTORY: 0,
  STREAM: 1
};

cr.define('extensions', function() {
  'use strict';

  /**
   * A struct used as a placeholder for chrome.developerPrivate.ExtensionInfo
   * for this component if the extensionId from the URL does not correspond to
   * installed extension.
   * @typedef {{
   *   id: string,
   *   isPlaceholder: boolean,
   * }}
   */
  let ActivityLogExtensionPlaceholder;

  const ActivityLog = Polymer({
    is: 'extensions-activity-log',

    behaviors: [
      CrContainerShadowBehavior,
      I18nBehavior,
      extensions.ItemBehavior,
    ],

    properties: {
      /**
       * The underlying ExtensionInfo for the details being displayed.
       * @type {!chrome.developerPrivate.ExtensionInfo|
       *        !extensions.ActivityLogExtensionPlaceholder}
       */
      extensionInfo: Object,

      /** @type {!extensions.ActivityLogDelegate} */
      delegate: Object,

      /** @private {!ActivityLogSubpage} */
      selectedSubpage_: {
        type: Number,
        value: ActivityLogSubpage.NONE,
        observer: 'onSelectedSubpageChanged_',
      },

      /** @private {Array<string>} */
      tabNames_: {
        type: Array,
        value: () => ([
          loadTimeData.getString('activityLogHistoryTabHeading'),
          loadTimeData.getString('activityLogStreamTabHeading'),
        ]),
      }
    },

    listeners: {
      'view-enter-start': 'onViewEnterStart_',
      'view-exit-finish': 'onViewExitFinish_',
    },

    /**
     * Focuses the back button when page is loaded and set the activie view to
     * be HISTORY when we navigate to the page.
     * @private
     */
    onViewEnterStart_: function() {
      this.selectedSubpage_ = ActivityLogSubpage.HISTORY;
      Polymer.RenderStatus.afterNextRender(
          this, () => cr.ui.focusWithoutInk(this.$.closeButton));
    },

    /**
     * Set |selectedSubpage_| to NONE to remove the active view from the DOM.
     * @private
     */
    onViewExitFinish_: function() {
      this.selectedSubpage_ = ActivityLogSubpage.NONE;
      // clear the stream if the user is exiting the activity log page.
      const activityLogStream = this.$$('activity-log-stream');
      if (activityLogStream) {
        activityLogStream.clearStream();
      }
    },

    /**
     * @private
     * @return {string}
     */
    getActivityLogHeading_: function() {
      const headingName = this.extensionInfo.isPlaceholder ?
          this.i18n('missingOrUninstalledExtension') :
          this.extensionInfo.name;
      return this.i18n('activityLogPageHeading', headingName);
    },

    /**
     * @private
     * @return {boolean}
     */
    isHistoryTabSelected_: function() {
      return this.selectedSubpage_ === ActivityLogSubpage.HISTORY;
    },

    /**
     * @private
     * @return {boolean}
     */
    isStreamTabSelected_: function() {
      return this.selectedSubpage_ === ActivityLogSubpage.STREAM;
    },

    /**
     * @private
     * @param {!ActivityLogSubpage} newTab
     * @param {!ActivityLogSubpage} oldTab
     */
    onSelectedSubpageChanged_: function(newTab, oldTab) {
      const activityLogStream = this.$$('activity-log-stream');
      if (activityLogStream) {
        if (newTab === ActivityLogSubpage.STREAM) {
          // Start the stream if the user is switching to the real-time tab.
          // This will not handle the first tab switch to the real-time tab as
          // the stream has not been attached to the DOM yet, and is handled
          // instead by the stream's |attached| method.
          activityLogStream.startStream();
        } else if (oldTab === ActivityLogSubpage.STREAM) {
          // Pause the stream if the user is navigating away from the real-time
          // tab.
          activityLogStream.pauseStream();
        }
      }
    },

    /** @private */
    onCloseButtonTap_: function() {
      if (this.extensionInfo.isPlaceholder) {
        extensions.navigation.navigateTo({page: extensions.Page.LIST});
      } else {
        extensions.navigation.navigateTo({
          page: extensions.Page.DETAILS,
          extensionId: this.extensionInfo.id
        });
      }
    },
  });

  return {
    ActivityLog: ActivityLog,
    ActivityLogExtensionPlaceholder: ActivityLogExtensionPlaceholder,
  };
});
