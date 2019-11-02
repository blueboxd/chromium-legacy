// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview');

(function() {
'use strict';

/**
 * @typedef {{ activeUser: string,
 *             users: (!Array<string> | undefined) }}
 */
let UpdateUsersPayload;

Polymer({
  is: 'print-preview-user-manager',

  behaviors: [WebUIListenerBehavior],

  properties: {
    activeUser: {
      type: String,
      notify: true,
    },

    appKioskMode: Boolean,

    cloudPrintDisabled: {
      type: Boolean,
      value: true,
      notify: true,
    },

    /** @type {?cloudprint.CloudPrintInterface} */
    cloudPrintInterface: {
      type: Object,
      observer: 'onCloudPrintInterfaceSet_',
    },

    /** @type {?print_preview.DestinationStore} */
    destinationStore: Object,

    /** @type {?print_preview.InvitationStore} */
    invitationStore: Object,

    shouldReloadCookies: Boolean,

    /** @type {!Array<string>} */
    users: {
      type: Array,
      notify: true,
      value: function() {
        return [];
      },
    },
  },

  /** @private {boolean} */
  initialized_: false,

  /** @private {!EventTracker} */
  tracker_: new EventTracker(),

  /** @override */
  detached: function() {
    this.tracker_.removeAll();
    this.initialized_ = false;
  },

  /**
   * @param {?Array<string>} userAccounts
   * @param {boolean} syncAvailable
   */
  initUserAccounts: function(userAccounts, syncAvailable) {
    assert(!this.initialized_);
    this.initialized_ = true;

    if (!userAccounts) {
      assert(this.cloudPrintDisabled);
      this.activeUser = '';
      return;
    }

    // If cloud print is enabled, listen for account changes.
    assert(!this.cloudPrintDisabled);
    if (syncAvailable) {
      this.addWebUIListener(
          'user-accounts-updated', this.updateUsers_.bind(this));
      this.updateUsers_(userAccounts);
    } else {
      // Request the Google Docs destination from the Google Cloud Print server
      // directly. We have to do this in incognito mode in order to get the
      // user's login state.
      this.destinationStore.startLoadGoogleDrive();
      this.addWebUIListener('check-for-account-update', () => {
        this.destinationStore.startLoadCloudDestinations(
            print_preview.DestinationOrigin.COOKIES);
      });
    }
  },

  /** @private */
  onCloudPrintInterfaceSet_: function() {
    this.tracker_.add(
        this.cloudPrintInterface.getEventTarget(),
        cloudprint.CloudPrintInterfaceEventType.UPDATE_USERS,
        this.onCloudPrintUpdateUsers_.bind(this));
    [cloudprint.CloudPrintInterfaceEventType.SEARCH_FAILED,
     cloudprint.CloudPrintInterfaceEventType.PRINTER_FAILED,
    ].forEach(eventType => {
      this.tracker_.add(
          this.cloudPrintInterface.getEventTarget(), eventType,
          this.checkCloudPrintStatus_.bind(this));
    });
    if (this.users.length > 0) {
      this.cloudPrintInterface.setUsers(this.users);
    }
    assert(this.cloudPrintDisabled);
    this.cloudPrintDisabled = false;
  },

  /**
   * Updates the cloud print status to NOT_SIGNED_IN if there is an
   * authentication error.
   * @param {!CustomEvent<!cloudprint.CloudPrintInterfaceErrorEventDetail>}
   *     event Contains the error status
   * @private
   */
  checkCloudPrintStatus_: function(event) {
    if (event.detail.status != 403 || this.appKioskMode) {
      return;
    }

    // Should not have sent a message to Cloud Print if cloud print is
    // disabled.
    assert(!this.cloudPrintDisabled);
    this.updateActiveUser('');
    console.warn('Google Cloud Print Error: HTTP status 403');
  },

  /**
   * @param {!CustomEvent<!UpdateUsersPayload>} e Event containing the new
   *     active user and users.
   * @private
   */
  onCloudPrintUpdateUsers_: function(e) {
    this.updateActiveUser(e.detail.activeUser);
    if (e.detail.users) {
      this.updateUsers_(e.detail.users);
    }
  },

  /**
   * @param {!Array<string>} users The full list of signed in users.
   * @private
   */
  updateUsers_: function(users) {
    const updateActiveUser = (users.length > 0 && this.users.length === 0) ||
        !users.includes(this.activeUser);
    this.users = users;
    if (this.cloudPrintInterface) {
      this.cloudPrintInterface.setUsers(users);
    }
    if (updateActiveUser) {
      this.updateActiveUser(this.users[0] || '');
    }
  },

  /** @param {string} user The new active user. */
  updateActiveUser: function(user) {
    if (user === this.activeUser) {
      return;
    }

    this.destinationStore.setActiveUser(user);
    this.activeUser = user;

    if (!this.shouldReloadCookies || !user) {
      return;
    }

    this.destinationStore.reloadUserCookieBasedDestinations(user);
    this.invitationStore.startLoadingInvitations(user);
  },
});
})();
