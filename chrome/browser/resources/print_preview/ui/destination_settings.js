// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview');

/** @enum {number} */
print_preview.DestinationState = {
  INIT: 0,
  SELECTED: 1,
  SET: 2,
  UPDATED: 3,
  ERROR: 4,
};

(function() {
'use strict';

/** @type {number} Number of recent destinations to save. */
const NUM_PERSISTED_DESTINATIONS = 3;

Polymer({
  is: 'print-preview-destination-settings',

  behaviors: [
    I18nBehavior,
    SettingsBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    appKioskMode: Boolean,

    /** @type {cloudprint.CloudPrintInterface} */
    cloudPrintInterface: {
      type: Object,
      observer: 'onCloudPrintInterfaceSet_',
    },

    dark: Boolean,

    /** @type {?print_preview.Destination} */
    destination: {
      type: Object,
      notify: true,
      value: null,
    },

    /** @private {!print_preview.DestinationState} */
    destinationState: {
      type: Number,
      notify: true,
      value: print_preview.DestinationState.INIT,
      observer: 'updateDestinationSelect_',
    },

    disabled: Boolean,

    /** @type {!print_preview.Error} */
    error: {
      type: Number,
      notify: true,
      observer: 'onErrorChanged_',
    },

    firstLoad: Boolean,

    /** @type {!print_preview.State} */
    state: Number,

    /** @private {string} */
    activeUser_: {
      type: String,
      observer: 'onActiveUserChanged_',
    },

    /** @private {boolean} */
    cloudPrintDisabled_: Boolean,

    /** @private {?print_preview.DestinationStore} */
    destinationStore_: {
      type: Object,
      value: null,
    },

    // <if expr="chromeos">
    hasPinSetting_: {
      type: Boolean,
      computed: 'computeHasPinSetting_(settings.pin.available)',
      reflectToAttribute: true,
    },
    // </if>

    /** @private {?print_preview.InvitationStore} */
    invitationStore_: {
      type: Object,
      value: null,
    },

    /** @private {boolean} */
    isDialogOpen_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    noDestinations_: {
      type: Boolean,
      value: false,
    },

    /** @private {!Array<!print_preview.RecentDestination>} */
    displayedDestinations_: Array,

    /** @private */
    shouldHideSpinner_: {
      type: Boolean,
      computed: 'computeShouldHideSpinner_(destinationState, destination)',
    },

    /** @private {string} */
    statusText_: {
      type: String,
      computed: 'computeStatusText_(destination)',
    },

    /** @private {!Array<string>} */
    users_: Array,
  },

  /** @private {string} */
  lastUser_: '',

  /** @private {!EventTracker} */
  tracker_: new EventTracker(),

  /** @override */
  attached: function() {
    this.destinationStore_ =
        new print_preview.DestinationStore(this.addWebUIListener.bind(this));
    this.invitationStore_ = new print_preview.InvitationStore();
    this.tracker_.add(
        this.destinationStore_,
        print_preview.DestinationStore.EventType.DESTINATION_SELECT,
        this.onDestinationSelect_.bind(this));
    this.tracker_.add(
        this.destinationStore_,
        print_preview.DestinationStore.EventType
            .SELECTED_DESTINATION_CAPABILITIES_READY,
        this.onDestinationCapabilitiesReady_.bind(this));
    this.tracker_.add(
        this.destinationStore_, print_preview.DestinationStore.EventType.ERROR,
        this.onDestinationError_.bind(this));
    // Need to update the recent list when the destination store inserts
    // destinations, in case any recent destinations have been added to the
    // store. At startup, recent destinations can be in the sticky settings,
    // but they should not be displayed in the dropdown until they have been
    // fetched by the DestinationStore, to ensure that they still exist.
    this.tracker_.add(
        assert(this.destinationStore_),
        print_preview.DestinationStore.EventType.DESTINATIONS_INSERTED,
        this.updateDropdownDestinations_.bind(this));
  },

  /** @override */
  detached: function() {
    this.invitationStore_.resetTracker();
    this.destinationStore_.resetTracker();
    this.tracker_.removeAll();
  },

  /** @private */
  onCloudPrintInterfaceSet_: function() {
    const cloudPrintInterface = assert(this.cloudPrintInterface);
    this.destinationStore_.setCloudPrintInterface(cloudPrintInterface);
    this.invitationStore_.setCloudPrintInterface(cloudPrintInterface);
  },

  /** @private */
  onActiveUserChanged_: function() {
    // Re-filter the dropdown destinations for the new account.
    if (!this.isDialogOpen_) {
      // Don't update the destination settings UI while the dialog is open in
      // front of it.
      this.updateDropdownDestinations_();
    }

    if (!this.destination ||
        this.destination.origin !== print_preview.DestinationOrigin.COOKIES) {
      // Active user changing doesn't impact non-cookie based destinations.
      return;
    }

    if (this.destination.account === this.activeUser_) {
      // If the current destination belongs to the new account and the dialog
      // was waiting for sign in, update the state.
      if (this.destinationState === print_preview.DestinationState.SELECTED) {
        this.destinationState = this.destination.capabilities ?
            print_preview.DestinationState.UPDATED :
            print_preview.DestinationState.SET;
      }
      return;
    }

    if (this.isDialogOpen_) {
      // Do not update the selected destination if the dialog is open, as this
      // will change the destination settings UI behind the dialog, and the user
      // may be selecting a new destination in the dialog anyway. Wait for the
      // user to select a destination or cancel.
      return;
    }

    // Destination belongs to a different account. Reset the destination to the
    // most recent destination associated with the new account, or the default.
    const recent = this.displayedDestinations_.find(d => {
      return d.origin !== print_preview.DestinationOrigin.COOKIES ||
          d.account === this.activeUser_;
    });
    if (recent) {
      const success = this.destinationStore_.selectRecentDestinationByKey(
          print_preview.createRecentDestinationKey(recent),
          this.displayedDestinations_);
      if (success) {
        return;
      }
    }
    this.destinationStore_.selectDefaultDestination();
  },

  /**
   * @param {string} defaultPrinter The system default printer ID.
   * @param {string} serializedDefaultDestinationRulesStr String with rules for
   *     selecting a default destination.
   * @param {?Array<string>} userAccounts The signed in user accounts.
   * @param {boolean} syncAvailable Whether sync is available. Used to determine
   *     whether to wait for user info updates from the handler, or to always
   *     send requests to the Google Cloud Print server.
   */
  init: function(
      defaultPrinter, serializedDefaultDestinationRulesStr, userAccounts,
      syncAvailable) {
    this.$.userManager.initUserAccounts(userAccounts, syncAvailable);
    this.destinationStore_.init(
        this.appKioskMode, defaultPrinter, serializedDefaultDestinationRulesStr,
        /** @type {!Array<print_preview.RecentDestination>} */
        (this.getSettingValue('recentDestinations')));
  },

  /** @private */
  onDestinationSelect_: function() {
    // If the user selected a destination in the dialog after changing the
    // active user, do the UI updates that were previously deferred.
    if (this.isDialogOpen_ && this.lastUser_ !== this.activeUser_) {
      this.updateDropdownDestinations_();
    }

    if (this.state === print_preview.State.FATAL_ERROR) {
      // Don't let anything reset if there is a fatal error.
      return;
    }

    const destination = this.destinationStore_.selectedDestination;
    if (!!this.activeUser_ ||
        destination.origin !== print_preview.DestinationOrigin.COOKIES) {
      this.destinationState = print_preview.DestinationState.SET;
    } else {
      this.destinationState = print_preview.DestinationState.SELECTED;
    }
    // Notify observers that the destination is set only after updating the
    // destinationState.
    this.destination = destination;
    this.updateRecentDestinations_();
  },

  /** @private */
  onDestinationCapabilitiesReady_: function() {
    this.notifyPath('destination.capabilities');
    this.updateRecentDestinations_();
    if (this.destinationState === print_preview.DestinationState.SET) {
      this.destinationState = print_preview.DestinationState.UPDATED;
    }
  },

  /**
   * @param {!CustomEvent<!print_preview.DestinationErrorType>} e
   * @private
   */
  onDestinationError_: function(e) {
    let errorType = print_preview.Error.NONE;
    switch (e.detail) {
      case print_preview.DestinationErrorType.INVALID:
        errorType = print_preview.Error.INVALID_PRINTER;
        break;
      case print_preview.DestinationErrorType.UNSUPPORTED:
        errorType = print_preview.Error.UNSUPPORTED_PRINTER;
        break;
      // <if expr="chromeos">
      case print_preview.DestinationErrorType.NO_DESTINATIONS:
        errorType = print_preview.Error.NO_DESTINATIONS;
        this.noDestinations_ = true;
        break;
      // </if>
      default:
        break;
    }
    this.error = errorType;
  },

  /** @private */
  onErrorChanged_: function() {
    if (this.error == print_preview.Error.INVALID_PRINTER ||
        this.error == print_preview.Error.UNSUPPORTED_PRINTER ||
        this.error == print_preview.Error.NO_DESTINATIONS) {
      this.destinationState = print_preview.DestinationState.ERROR;
    }
  },

  /**
   * @param {!print_preview.RecentDestination} destination
   * @return {boolean} Whether the destination is Save as PDF or Save to Drive.
   */
  destinationIsDriveOrPdf_: function(destination) {
    return destination.id ===
        print_preview.Destination.GooglePromotedId.SAVE_AS_PDF ||
        destination.id === print_preview.Destination.GooglePromotedId.DOCS;
  },

  /** @private */
  updateRecentDestinations_: function() {
    if (!this.destination) {
      return;
    }

    // Determine if this destination is already in the recent destinations,
    // and where in the array it is located.
    const newDestination =
        print_preview.makeRecentDestination(assert(this.destination));
    const recentDestinations =
        /** @type {!Array<!print_preview.RecentDestination>} */ (
            this.getSettingValue('recentDestinations'));
    let indexFound = recentDestinations.findIndex(function(recent) {
      return (
          newDestination.id == recent.id &&
          newDestination.origin == recent.origin);
    });

    // No change
    if (indexFound == 0 &&
        recentDestinations[0].capabilities == newDestination.capabilities) {
      return;
    }
    const isNew = indexFound == -1;

    // Shift the array so that the nth most recent destination is located at
    // index n.
    if (isNew && recentDestinations.length == NUM_PERSISTED_DESTINATIONS) {
      indexFound = NUM_PERSISTED_DESTINATIONS - 1;
    }
    if (indexFound != -1) {
      this.setSettingSplice('recentDestinations', indexFound, 1, null);
    }

    // Add the most recent destination
    this.setSettingSplice('recentDestinations', 0, 0, newDestination);
    if (!this.destinationIsDriveOrPdf_(newDestination) && isNew) {
      this.updateDropdownDestinations_();
    }
  },

  /** @private */
  updateDropdownDestinations_: function() {
    this.displayedDestinations_ =
        /** @type {!Array<!print_preview.RecentDestination>} */ (
            this.getSettingValue('recentDestinations'))
            .filter(d => {
              return !this.destinationIsDriveOrPdf_(d) &&
                  (d.origin !== print_preview.DestinationOrigin.COOKIES ||
                   d.account === this.activeUser_);
            });
  },

  /**
   * @return {boolean} Whether the destinations dropdown should be disabled.
   * @private
   */
  shouldDisableDropdown_: function() {
    return this.state === print_preview.State.FATAL_ERROR ||
        (this.destinationState === print_preview.DestinationState.UPDATED &&
         this.disabled && this.state !== print_preview.State.NOT_READY);
  },

  /** @private */
  computeShouldHideSpinner_: function() {
    return this.destinationState === print_preview.DestinationState.ERROR ||
        this.destinationState === print_preview.DestinationState.UPDATED ||
        (this.destinationState === print_preview.DestinationState.SET &&
         !!this.destination &&
         (!!this.destination.capabilities ||
          this.destination.id ===
              print_preview.Destination.GooglePromotedId.SAVE_AS_PDF));
  },

  /**
   * @return {string} The connection status text to display.
   * @private
   */
  computeStatusText_: function() {
    // |destination| can be either undefined, or null here.
    if (!this.destination) {
      return '';
    }

    return this.destination.shouldShowInvalidCertificateError ?
        this.i18n('noLongerSupportedFragment') :
        this.destination.connectionStatusText;
  },

  // <if expr="chromeos">
  /**
   * @return {boolean}
   * @private
   */
  computeHasPinSetting_: function() {
    return this.getSetting('pin').available;
  },
  // </if>

  /**
   * @param {!CustomEvent<string>} e Event containing the key of the recent
   *     destination that was selected, or "seeMore".
   * @private
   */
  onSelectedDestinationOptionChange_: function(e) {
    const value = e.detail;
    if (value === 'seeMore') {
      this.destinationStore_.startLoadAllDestinations();
      if (this.activeUser_) {
        this.invitationStore_.startLoadingInvitations(this.activeUser_);
      }
      this.$.destinationDialog.get().show();
      this.lastUser_ = this.activeUser_;
      this.isDialogOpen_ = true;
    } else {
      const success = this.destinationStore_.selectRecentDestinationByKey(
          value, this.displayedDestinations_);
      if (!success) {
        this.error = print_preview.Error.INVALID_PRINTER;
      }
    }
  },

  /**
   * @param {!CustomEvent<string>} e Event containing the new active user
   *     account.
   * @private
   */
  onAccountChange_: function(e) {
    this.$.userManager.updateActiveUser(e.detail, true);
  },

  /** @private */
  onDialogClose_: function() {
    // Reset the select value if the user dismissed the dialog without
    // selecting a new destination.
    if (this.lastUser_ != this.activeUser_) {
      this.updateDropdownDestinations_();
    }
    this.updateDestinationSelect_();
    this.isDialogOpen_ = false;
  },

  /** @private */
  updateDestinationSelect_: function() {
    // <if expr="chromeos">
    if (this.destinationState === print_preview.DestinationState.ERROR &&
        !this.destination) {
      return;
    }
    // </if>

    if (this.destinationState === print_preview.DestinationState.INIT ||
        this.destinationState === print_preview.DestinationState.SELECTED) {
      return;
    }

    const shouldFocus =
        this.destinationState !== print_preview.DestinationState.SET &&
        !this.firstLoad;
    Polymer.RenderStatus.beforeNextRender(this.$.destinationSelect, () => {
      this.$.destinationSelect.updateDestination();
      if (shouldFocus) {
        this.$.destinationSelect.focus();
      }
    });
  },
});
})();
