// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-contact-visibility' component allows users to
 * set the preferred visibility to contacts for Nearby Share. This component is
 * embedded in the nearby_visibility_page as well as the settings pop-up dialog.
 */

'use strict';
(function() {

/**
 * Maps visibility string to the mojo enum
 * @param {?string} visibilityString
 * @return {?nearbyShare.mojom.Visibility}
 */
const visibilityStringToValue = function(visibilityString) {
  switch (visibilityString) {
    case 'all':
      return nearbyShare.mojom.Visibility.kAllContacts;
    case 'some':
      return nearbyShare.mojom.Visibility.kSelectedContacts;
    case 'none':
      return nearbyShare.mojom.Visibility.kNoOne;
    default:
      return null;
  }
};

/**
 * Maps visibility mojo enum to a string for the radio button selection
 * @param {?nearbyShare.mojom.Visibility} visibility
 * @return {?string}
 */
const visibilityValueToString = function(visibility) {
  switch (visibility) {
    case nearbyShare.mojom.Visibility.kAllContacts:
      return 'all';
    case nearbyShare.mojom.Visibility.kSelectedContacts:
      return 'some';
    case nearbyShare.mojom.Visibility.kNoOne:
      return 'none';
    default:
      return null;
  }
};

/**
 * @typedef {{
 *            name:string,
 *            email:string,
 *            checked:boolean,
 *          }}
 */
/* #export */ let NearbyVisibilityContact;

Polymer({
  is: 'nearby-contact-visibility',

  behaviors: [I18nBehavior],

  properties: {
    /** @type {?nearby_share.NearbySettings} */
    settings: {
      type: Object,
      notify: true,
    },

    /**
     * @type {?string} Which of visibility setting is selected as a string or
     *      null for no selection. ('all', 'some', 'none', null).
     */
    selectedVisibility: {
      type: String,
      value: null,
      notify: true,
    },

    /**
     *  @type {Array<NearbyVisibilityContact>} The user's GAIA contacts
     *      formatted for binding.
     */
    contacts: {
      type: Array,
      value: [
        {
          name: 'Jane Doe',
          email: 'jane@google.com',
          checked: true,
        },
        {
          name: 'John Smith',
          email: 'smith@gmail.com',
          checked: false,
        },
        {
          name: 'Testy Tester',
          email: 'test@test.com',
          checked: true,
        }
      ],
    },
  },

  observers: [
    'settingsChanged_(settings.visibility)',
    'selectedVisibilityChanged_(selectedVisibility)',
  ],

  /** @type {ResizeObserver} used to observer size changes to this element */
  resizeObserver_: null,

  /** @override */
  attached() {
    // This is a required work around to get the iron-list to display on first
    // view. Currently iron-list won't generate item elements on attach if the
    // element is not visible. Because we are hosted in a cr-view-manager for
    // on-boarding, this component is not visible when the items are bound. To
    // fix this issue, we listen for resize events (which happen when display is
    // switched from none to block by the view manager) and manually call
    // notifyResize on the iron-list
    this.resizeObserver_ = new ResizeObserver(entries => {
      const contactList =
          /** @type {IronListElement} */ (this.$$('#contactList'));
      if (contactList) {
        contactList.notifyResize();
      }
    });
    this.resizeObserver_.observe(this);
  },

  /** @override */
  detached() {
    this.resizeObserver_.disconnect();
  },

  /**
   * Used to show/hide parts of the UI based on current visibility selection.
   * @param {?string} selectedVisibility
   * @return {boolean} Returns true when the current selectedVisibility has a
   *     value other than null.
   * @private
   */
  isVisibilitySelected_(selectedVisibility) {
    return this.selectedVisibility !== null;
  },

  /**
   * Used to show/hide parts of the UI based on current visibility selection.
   * @param {?string} selectedVisibility
   * @param {string} visibilityString
   * @return {boolean} returns true when the current selectedVisibility equals
   *     the passed arguments.
   * @private
   */
  isVisibility_(selectedVisibility, visibilityString) {
    return this.selectedVisibility === visibilityString;
  },

  /**
   * Used to show/hide parts of the UI based on current visibility selection.
   * @param {?string} selectedVisibility
   * @return {boolean} returns true when checkboxes should be shown for
   *     contacts.
   * @private
   */
  showContactCheckBoxes_(selectedVisibility) {
    return this.selectedVisibility === 'some' ||
        this.selectedVisibility === 'none';
  },

  /**
   * Used to show/hide parts of the UI based on the number of contacts
   * @param {?Array} contacts the currently bound contact list
   * @return {boolean} returns true when checkboxes should be shown for
   *     contacts.
   * @private
   */
  showContactsList_(contacts) {
    return this.contacts.length > 0;
  },

  /**
   * @param {?nearby_share.NearbySettings} settings
   * @private
   */
  settingsChanged_(settings) {
    if (this.settings && this.settings.visibility) {
      this.selectedVisibility =
          visibilityValueToString(this.settings.visibility);
    } else {
      this.selectedVisibility = null;
    }
  },

  /**
   * @param {string} selectedVisibility
   * @private
   */
  selectedVisibilityChanged_(selectedVisibility) {
    const visibility = visibilityStringToValue(this.selectedVisibility);
    if (visibility) {
      this.set('settings.visibility', visibility);
    }
  },
});
})();
