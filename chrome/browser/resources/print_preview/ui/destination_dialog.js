// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-destination-dialog',

  behaviors: [I18nBehavior, ListPropertyUpdateBehavior],

  properties: {
    /** @type {?print_preview.DestinationStore} */
    destinationStore: {
      type: Object,
      observer: 'onDestinationStoreSet_',
    },

    /** @type {?print_preview.InvitationStore} */
    invitationStore: {
      type: Object,
      observer: 'onInvitationStoreSet_',
    },

    activeUser: {
      type: String,
      observer: 'onActiveUserChange_',
    },

    currentDestinationAccount: String,

    /** @type {!Array<string>} */
    users: Array,

    /** @private {?print_preview.Invitation} */
    invitation_: {
      type: Object,
      value: null,
    },

    cloudPrintDisabled: Boolean,

    /** @private */
    cloudPrintPromoDismissed_: {
      type: Boolean,
      value: false,
    },

    /** @private {!Array<!print_preview.Destination>} */
    destinations_: {
      type: Array,
      value: [],
    },

    /** @private {boolean} */
    loadingDestinations_: {
      type: Boolean,
      value: false,
    },

    /** @private {?RegExp} */
    searchQuery_: {
      type: Object,
      value: null,
    },

    /** @private {boolean} */
    shouldShowCloudPrintPromo_: {
      type: Boolean,
      computed: 'computeShouldShowCloudPrintPromo_(' +
          'cloudPrintDisabled, activeUser, cloudPrintPromoDismissed_)',
      observer: 'onShouldShowCloudPrintPromoChanged_',
    },
  },

  listeners: {
    'keydown': 'onKeydown_',
  },

  /** @private {!EventTracker} */
  tracker_: new EventTracker(),

  /** @private {!print_preview.MetricsContext} */
  metrics_: print_preview.MetricsContext.destinationSearch(),

  // <if expr="chromeos">
  /** @private {?print_preview.Destination} */
  destinationInConfiguring_: null,
  // </if>

  /** @private {boolean} */
  initialized_: false,

  /** @override */
  ready: function() {
    this.$$('.promo-text').innerHTML =
        this.i18nAdvanced('cloudPrintPromotion', {
          substitutions: ['<a is="action-link" class="sign-in">', '</a>'],
          attrs: {
            'is': (node, v) => v == 'action-link',
            'class': (node, v) => v == 'sign-in',
            'tabindex': (node, v) => v == '0',
            'role': (node, v) => v == 'link',
          },
        });
  },

  /** @override */
  attached: function() {
    this.tracker_.add(
        assert(this.$$('.sign-in')), 'click', this.onSignInClick_.bind(this));
  },

  /** @override */
  detached: function() {
    this.tracker_.removeAll();
  },

  /**
   * @param {!KeyboardEvent} e Event containing the key
   * @private
   */
  onKeydown_: function(e) {
    e.stopPropagation();
    const searchInput = this.$.searchBox.getSearchInput();
    if (e.key == 'Escape' &&
        (e.composedPath()[0] !== searchInput || !searchInput.value.trim())) {
      this.$.dialog.cancel();
      e.preventDefault();
    }
  },

  /** @private */
  onDestinationStoreSet_: function() {
    assert(this.destinations_.length == 0);
    const destinationStore = assert(this.destinationStore);
    this.tracker_.add(
        destinationStore,
        print_preview.DestinationStore.EventType.DESTINATIONS_INSERTED,
        this.updateDestinations_.bind(this));
    this.tracker_.add(
        destinationStore,
        print_preview.DestinationStore.EventType.DESTINATION_SEARCH_DONE,
        this.updateDestinationsAndInvitations_.bind(this));
    this.initialized_ = true;
  },

  /** @private */
  onInvitationStoreSet_: function() {
    const invitationStore = assert(this.invitationStore);
    this.tracker_.add(
        invitationStore,
        print_preview.InvitationStore.EventType.INVITATION_SEARCH_DONE,
        this.updateInvitations_.bind(this));
    this.tracker_.add(
        invitationStore,
        print_preview.InvitationStore.EventType.INVITATION_PROCESSED,
        this.updateInvitations_.bind(this));
  },

  /** @private */
  onActiveUserChange_: function() {
    if (this.activeUser) {
      this.$$('select').value = this.activeUser;
    }

    this.updateDestinationsAndInvitations_();
  },

  /** @private */
  updateDestinationsAndInvitations_: function() {
    if (!this.initialized_) {
      return;
    }

    this.updateDestinations_();
    if (this.activeUser && !!this.invitationStore) {
      this.invitationStore.startLoadingInvitations(this.activeUser);
    }
  },

  /** @private */
  updateDestinations_: function() {
    if (this.destinationStore === undefined) {
      return;
    }

    this.updateList(
        'destinations_', destination => destination.key,
        this.destinationStore.destinations(this.activeUser));

    this.loadingDestinations_ =
        this.destinationStore.isPrintDestinationSearchInProgress;
  },

  /** @private */
  onCloseOrCancel_: function() {
    if (this.searchQuery_) {
      this.$.searchBox.setValue('');
    }
    const cancelled = this.$.dialog.getNative().returnValue !== 'success';
    this.metrics_.record(
        cancelled ? print_preview.Metrics.DestinationSearchBucket
                        .DESTINATION_CLOSED_UNCHANGED :
                    print_preview.Metrics.DestinationSearchBucket
                        .DESTINATION_CLOSED_CHANGED);
    if (this.currentDestinationAccount &&
        this.currentDestinationAccount !== this.activeUser) {
      this.fire('account-change', this.currentDestinationAccount);
    }
  },

  /** @private */
  onCancelButtonClick_: function() {
    this.$.dialog.cancel();
  },

  /**
   * @param {!CustomEvent<!PrintPreviewDestinationListItemElement>} e Event
   *     containing the selected destination list item element.
   * @private
   */
  onDestinationSelected_: function(e) {
    const listItem = e.detail;
    const destination = listItem.destination;

    // ChromeOS local destinations that don't have capabilities need to be
    // configured before selecting, and provisional destinations need to be
    // resolved. Other destinations can be selected.
    if (destination.readyForSelection) {
      this.selectDestination_(destination);
      return;
    }

    // Provisional destinations
    if (destination.isProvisional) {
      this.$.provisionalResolver.resolveDestination(destination)
          .then(this.selectDestination_.bind(this))
          .catch(function() {
            console.warn(
                'Failed to resolve provisional destination: ' + destination.id);
          })
          .then(() => {
            if (this.$.dialog.open && listItem && !listItem.hidden) {
              listItem.focus();
            }
          });
      return;
    }

    // <if expr="chromeos">
    // Destination must be a CrOS local destination that needs to be set up.
    // The user is only allowed to set up printer at one time.
    if (this.destinationInConfiguring_) {
      return;
    }

    // Show the configuring status to the user and resolve the destination.
    listItem.onConfigureRequestAccepted();
    this.destinationInConfiguring_ = destination;
    this.destinationStore.resolveCrosDestination(destination)
        .then(
            response => {
              this.destinationInConfiguring_ = null;
              listItem.onConfigureComplete(response.success);
              if (response.success) {
                destination.capabilities = response.capabilities;
                if (response.policies) {
                  destination.policies = response.policies;
                }
                this.selectDestination_(destination);
              }
            },
            () => {
              this.destinationInConfiguring_ = null;
              listItem.onConfigureComplete(false);
            });
    // </if>
  },

  /**
   * @param {!print_preview.Destination} destination The destination to select.
   * @private
   */
  selectDestination_: function(destination) {
    this.destinationStore.selectDestination(destination);
    this.$.dialog.close();
  },

  show: function() {
    this.$.dialog.showModal();
    // Note: Manually focusing here instead of using autofocus, as it is
    // currently not possible to validate refocusing of the search input if
    // autofocus is used. See https://crbug.com/985637 and
    // https://crbug.com/985636. Autofocus can be restored when one or both of
    // these issues are resolved.
    this.$.searchBox.focus();
    this.loadingDestinations_ = this.destinationStore === undefined ||
        this.destinationStore.isPrintDestinationSearchInProgress;
    this.metrics_.record(
        print_preview.Metrics.DestinationSearchBucket.DESTINATION_SHOWN);
    if (this.activeUser) {
      Polymer.RenderStatus.beforeNextRender(assert(this.$$('select')), () => {
        this.$$('select').value = this.activeUser;
      });
    }
    this.$.printList.forceIronResize();
  },

  /** @return {boolean} Whether the dialog is open. */
  isOpen: function() {
    return this.$.dialog.hasAttribute('open');
  },

  /** @private */
  onSignInClick_: function() {
    this.metrics_.record(
        print_preview.Metrics.DestinationSearchBucket.SIGNIN_TRIGGERED);
    print_preview.NativeLayer.getInstance().signIn(false);
  },

  /** @private */
  onCloudPrintPromoDismissed_: function() {
    this.cloudPrintPromoDismissed_ = true;
  },

  /**
   * Updates printer sharing invitations UI.
   * @private
   */
  updateInvitations_: function() {
    const invitations = this.activeUser ?
        this.invitationStore.invitations(this.activeUser) :
        [];
    if (this.invitation_ != invitations[0]) {
      this.metrics_.record(
          print_preview.Metrics.DestinationSearchBucket.INVITATION_AVAILABLE);
    }
    this.invitation_ = invitations.length > 0 ? invitations[0] : null;
  },

  /**
   * @return {string} The text show show on the "accept" button in the
   *     invitation promo. 'Accept', 'Accept for group', or empty if there is no
   *     invitation.
   * @private
   */
  getAcceptButtonText_: function() {
    if (!this.invitation_) {
      return '';
    }

    return this.invitation_.asGroupManager ? this.i18n('acceptForGroup') :
                                             this.i18n('accept');
  },

  /**
   * @return {string} The formatted text to show for the invitation promo.
   * @private
   */
  getInvitationText_: function() {
    if (!this.invitation_) {
      return '';
    }

    if (this.invitation_.asGroupManager) {
      return this.i18nAdvanced('groupPrinterSharingInviteText', {
        substitutions: [
          this.invitation_.sender, this.invitation_.destination.displayName,
          this.invitation_.receiver
        ]
      });
    }

    return this.i18nAdvanced('printerSharingInviteText', {
      substitutions:
          [this.invitation_.sender, this.invitation_.destination.displayName]
    });
  },

  /** @private */
  onInvitationAcceptClick_: function() {
    this.metrics_.record(
        print_preview.Metrics.DestinationSearchBucket.INVITATION_ACCEPTED);
    this.invitationStore.processInvitation(assert(this.invitation_), true);
    this.updateInvitations_();
  },

  /** @private */
  onInvitationRejectClick_: function() {
    this.metrics_.record(
        print_preview.Metrics.DestinationSearchBucket.INVITATION_REJECTED);
    this.invitationStore.processInvitation(assert(this.invitation_), false);
    this.updateInvitations_();
  },

  /** @private */
  onUserChange_: function() {
    const select = this.$$('select');
    const account = select.value;
    if (account) {
      this.loadingDestinations_ = true;
      this.fire('account-change', account);
      this.metrics_.record(
          print_preview.Metrics.DestinationSearchBucket.ACCOUNT_CHANGED);
    } else {
      select.value = this.activeUser;
      print_preview.NativeLayer.getInstance().signIn(true);
      this.metrics_.record(
          print_preview.Metrics.DestinationSearchBucket.ADD_ACCOUNT_SELECTED);
    }
  },

  /**
   * @return {boolean} Whether to show the cloud print promo.
   * @private
   */
  computeShouldShowCloudPrintPromo_: function() {
    return !this.activeUser && !this.cloudPrintDisabled &&
        !this.cloudPrintPromoDismissed_;
  },

  /** @private */
  onShouldShowCloudPrintPromoChanged_: function() {
    if (this.shouldShowCloudPrintPromo_) {
      this.metrics_.record(
          print_preview.Metrics.DestinationSearchBucket.SIGNIN_PROMPT);
    } else {
      // Since the sign in link/dismiss promo button is disappearing, focus the
      // search box.
      this.$.searchBox.focus();
    }
  },

  /**
   * @return {boolean} Whether to show the footer.
   * @private
   */
  shouldShowFooter_: function() {
    return this.shouldShowCloudPrintPromo_ || !!this.invitation_;
  },

  /** @private */
  onOpenSettingsPrintPage_: function() {
    this.metrics_.record(
        print_preview.Metrics.DestinationSearchBucket.MANAGE_BUTTON_CLICKED);
    print_preview.NativeLayer.getInstance().openSettingsPrintPage();
  },
});
