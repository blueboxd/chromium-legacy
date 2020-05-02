// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-import-data-dialog' is a component for importing
 * bookmarks and other data from other sources.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../controls/settings_checkbox.js';
import '../controls/settings_toggle_button.m.js';
import '../icons.m.js';
import '../settings_vars_css.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {PrefsBehavior} from '../prefs/prefs_behavior.m.js';

import {BrowserProfile, ImportDataBrowserProxy, ImportDataBrowserProxyImpl, ImportDataStatus} from './import_data_browser_proxy.js';

Polymer({
  is: 'settings-import-data-dialog',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior, WebUIListenerBehavior, PrefsBehavior],

  properties: {
    /** @private {!Array<!BrowserProfile>} */
    browserProfiles_: Array,

    /** @private {!BrowserProfile} */
    selected_: Object,

    /**
     * Whether none of the import data categories is selected.
     * @private
     */
    noImportDataTypeSelected_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    importStatus_: {
      type: String,
      value: ImportDataStatus.INITIAL,
    },

    /**
     * Mirroring the enum so that it can be used from HTML bindings.
     * @private
     */
    importStatusEnum_: {
      type: Object,
      value: ImportDataStatus,
    },
  },

  observers: [
    'prefsChanged_(selected_, prefs.*)',
  ],

  /** @private {?ImportDataBrowserProxy} */
  browserProxy_: null,

  /** @override */
  attached() {
    this.browserProxy_ = ImportDataBrowserProxyImpl.getInstance();
    this.browserProxy_.initializeImportDialog().then(data => {
      this.browserProfiles_ = data;
      this.selected_ = this.browserProfiles_[0];

      // Show the dialog only after the browser profiles data is populated
      // to avoid UI flicker.
      this.$.dialog.showModal();
    });

    this.addWebUIListener('import-data-status-changed', importStatus => {
      this.importStatus_ = importStatus;
      if (this.hasImportStatus_(ImportDataStatus.FAILED)) {
        this.closeDialog_();
      }
    });
  },

  /**
   * @param {string} name
   * @param {string} profileName
   * @return {string}
   * @private
   */
  getProfileDisplayName_(name, profileName) {
    return profileName ? `${name} - ${profileName}` : name;
  },

  /** @private */
  prefsChanged_() {
    if (this.selected_ == undefined || this.prefs == undefined) {
      return;
    }

    this.noImportDataTypeSelected_ =
        !(this.getPref('import_dialog_history').value &&
          this.selected_.history) &&
        !(this.getPref('import_dialog_bookmarks').value &&
          this.selected_.favorites) &&
        !(this.getPref('import_dialog_saved_passwords').value &&
          this.selected_.passwords) &&
        !(this.getPref('import_dialog_search_engine').value &&
          this.selected_.search) &&
        !(this.getPref('import_dialog_autofill_form_data').value &&
          this.selected_.autofillFormData);
  },

  /**
   * @param {!ImportDataStatus} status
   * @return {boolean} Whether |status| is the current status.
   * @private
   */
  hasImportStatus_(status) {
    return this.importStatus_ == status;
  },

  /** @private */
  isImportFromFileSelected_() {
    // The last entry in |browserProfiles_| always refers to dummy profile for
    // importing from a bookmarks file.
    return this.selected_.index == this.browserProfiles_.length - 1;
  },

  /**
   * @return {string}
   * @private
   */
  getActionButtonText_() {
    return this.i18n(
        this.isImportFromFileSelected_() ? 'importChooseFile' : 'importCommit');
  },

  /** @private */
  onBrowserProfileSelectionChange_() {
    this.selected_ = this.browserProfiles_[this.$.browserSelect.selectedIndex];
  },

  /** @private */
  onActionButtonTap_() {
    const checkboxes = /** @type {!NodeList<!SettingsCheckboxElement>} */ (
        this.shadowRoot.querySelectorAll('settings-checkbox'));
    if (this.isImportFromFileSelected_()) {
      this.browserProxy_.importFromBookmarksFile();
    } else {
      const types = {};
      checkboxes.forEach(checkbox => {
        types[checkbox.pref.key] = checkbox.checked;
      });
      this.browserProxy_.importData(this.$.browserSelect.selectedIndex, types);
    }
    checkboxes.forEach(checkbox => checkbox.sendPrefChange());
  },

  /** @private */
  closeDialog_() {
    this.$.dialog.close();
  },

  /**
   * @return {boolean} Whether the import button should be disabled.
   * @private
   */
  shouldDisableImport_() {
    return this.hasImportStatus_(ImportDataStatus.IN_PROGRESS) ||
        this.noImportDataTypeSelected_;
  },
});
