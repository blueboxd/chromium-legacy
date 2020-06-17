// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'passwords-device-section' represents the page containing
 * the list of passwords which have at least one copy on the user device.
 *
 * This page is *not* displayed on ChromeOS.
 */

import './passwords_list_handler.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import '../settings_shared_css.m.js';
import './passwords_shared_css.js';
import './password_list_item.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {IronA11yKeysBehavior} from 'chrome://resources/polymer/v3_0/iron-a11y-keys-behavior/iron-a11y-keys-behavior.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {GlobalScrollTargetBehavior} from '../global_scroll_target_behavior.m.js';
import {routes} from '../route.js';

import {MergePasswordsStoreCopiesBehavior} from './merge_passwords_store_copies_behavior.js';
import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';

/**
 * Checks if an HTML element is an editable. An editable element is either a
 * text input or a text area.
 * @param {!Element} element
 * @return {boolean}
 */
function isEditable(element) {
  const nodeName = element.nodeName.toLowerCase();
  return element.nodeType === Node.ELEMENT_NODE &&
      (nodeName === 'textarea' ||
       (nodeName === 'input' &&
        /^(?:text|search|email|number|tel|url|password)$/i.test(element.type)));
}

Polymer({
  is: 'passwords-device-section',

  _template: html`{__html_template__}`,

  behaviors: [
    MergePasswordsStoreCopiesBehavior,
    I18nBehavior,
    IronA11yKeysBehavior,
    GlobalScrollTargetBehavior,
  ],

  properties: {
    /** @override */
    subpageRoute: {
      type: Object,
      value: routes.DEVICE_PASSWORDS,
    },

    /** Search filter on the saved passwords. */
    filter: {
      type: String,
      value: '',
    },

    /**
     * The target of the key bindings defined below.
     * @type {EventTarget}
     */
    keyEventTarget: {
      type: Object,
      value: () => document,
    },

    /**
     * Passwords displayed in the device-only subsection.
     * @type {!Array<!MultiStorePasswordUiEntry>}
     * @private
     */
    deviceOnlyPasswords_: {
      type: Array,
      value: () => [],
      computed:
          'computeDeviceOnlyPasswords_(savedPasswords, savedPasswords.splices)',
    },

    /**
     * Passwords displayed in the 'device and account' subsection.
     * @type {!Array<!MultiStorePasswordUiEntry>}
     * @private
     */
    deviceAndAccountPasswords_: {
      type: Array,
      value: () => [],
      computed: 'computeDeviceAndAccountPasswords_(savedPasswords, ' +
          'savedPasswords.splices)',
    },

    /** @private {!MultiStorePasswordUiEntry} */
    lastFocused_: Object,

    /** @private */
    listBlurred_: Boolean,

  },

  /**
   * @param {!Array<!MultiStorePasswordUiEntry>} passwords
   * @return {boolean}
   * @private
   */
  isNonEmpty_(passwords) {
    return passwords.length > 0;
  },

  keyBindings: {
    // <if expr="is_macosx">
    'meta+z': 'onUndoKeyBinding_',
    // </if>
    // <if expr="not is_macosx">
    'ctrl+z': 'onUndoKeyBinding_',
    // </if>
  },

  /**
   * @return {!Array<!MultiStorePasswordUiEntry>}
   * @private
   */
  computeDeviceOnlyPasswords_() {
    return this.savedPasswords.filter(
        p => p.isPresentOnDevice() && !p.isPresentInAccount());
  },

  /**
   * @return {!Array<!MultiStorePasswordUiEntry>}
   * @private
   */
  computeDeviceAndAccountPasswords_() {
    return this.savedPasswords.filter(
        p => p.isPresentOnDevice() && p.isPresentInAccount());
  },

  /**
   * @param {!Array<!MultiStorePasswordUiEntry>} passwords
   * @param {string} filter
   * @return {!Array<!MultiStorePasswordUiEntry>}
   * @private
   */
  getFilteredPasswords_(passwords, filter) {
    if (!filter) {
      return passwords.slice();
    }

    return passwords.filter(
        p => [p.urls.shown, p.username].some(
            term => term.toLowerCase().includes(filter.toLowerCase())));
  },

  /**
   * Handle the undo shortcut.
   * @param {!Event} event
   * @private
   */
  // TODO(crbug.com/1049141): Consider grouping the ctrl-z related code into
  // a dedicated behavior.
  onUndoKeyBinding_(event) {
    const activeElement = getDeepActiveElement();
    if (!activeElement || !isEditable(activeElement)) {
      PasswordManagerImpl.getInstance().undoRemoveSavedPasswordOrException();
      this.$.passwordsListHandler.onSavedPasswordOrExceptionRemoved();
      // Preventing the default is necessary to not conflict with a possible
      // search action.
      event.preventDefault();
    }
  },
});
