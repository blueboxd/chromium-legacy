// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'passwords-section' is the collapsible section containing
 * the list of saved passwords as well as the list of sites that will never
 * save any passwords.
 */

/** @typedef {!{model: !{item: !PasswordManagerProxy.UiEntryWithPassword}}} */
let PasswordUiEntryEvent;

/** @typedef {!{model: !{item: !chrome.passwordsPrivate.ExceptionEntry}}} */
let ExceptionEntryEntryEvent;

import {afterNextRender, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.m.js';
import {getToastManager} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {ListPropertyUpdateBehavior} from 'chrome://resources/js/list_property_update_behavior.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {IronA11yKeysBehavior} from 'chrome://resources/polymer/v3_0/iron-a11y-keys-behavior/iron-a11y-keys-behavior.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '../controls/extension_controlled_indicator.m.js';
import '../controls/settings_toggle_button.m.js';
import {GlobalScrollTargetBehavior} from '../global_scroll_target_behavior.m.js';
import {loadTimeData} from '../i18n_setup.js';
import {SyncBrowserProxyImpl, SyncPrefs, SyncStatus} from '../people_page/sync_browser_proxy.m.js';
import {PluralStringProxyImpl} from '../plural_string_proxy.js';
import '../prefs/prefs.m.js';
import {PrefsBehavior} from '../prefs/prefs_behavior.m.js';
import {routes} from '../route.js';
import {Router} from '../router.m.js';
import '../settings_shared_css.m.js';
import '../site_favicon.js';
import {PasswordCheckBehavior} from './password_check_behavior.js';
import './password_edit_dialog.js';
import './password_list_item.js';
import {PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';
import './passwords_export_dialog.js';
import './passwords_shared_css.js';
// <if expr="chromeos">
import '../controls/password_prompt_dialog.m.js';
import {BlockingRequestManager} from './blocking_request_manager.js';
// </if>


/**
 * Checks if an HTML element is an editable. An editable is either a text
 * input or a text area.
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
  is: 'passwords-section',

  _template: html`{__html_template__}`,

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
    ListPropertyUpdateBehavior,
    PasswordCheckBehavior,
    IronA11yKeysBehavior,
    GlobalScrollTargetBehavior,
    PrefsBehavior,
  ],

  properties: {
    // <if expr="not chromeos">
    /** @private */
    storedAccounts_: Array,
    // </if>

    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * An array of passwords to display.
     * @type {!Array<!PasswordManagerProxy.UiEntryWithPassword>}
     */
    savedPasswords: {
      type: Array,
      value: () => [],
    },

    /**
     * An array of sites to display.
     * @type {!Array<!PasswordManagerProxy.ExceptionEntry>}
     */
    passwordExceptions: {
      type: Array,
      value: () => [],
    },

    /** @override */
    subpageRoute: {
      type: Object,
      value: routes.PASSWORDS,
    },

    /**
     * The model for any password related action menus or dialogs.
     * @private {?PasswordListItemElement}
     */
    activePassword: Object,

    /** The target of the key bindings defined below. */
    keyEventTarget: {
      type: Object,
      value: () => document,
    },

    /** @private */
    enablePasswordCheck_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('enablePasswordCheck');
      }
    },

    /** @private */
    signedIn_: {
      type: Boolean,
      value: true,
      computed: 'computeSignedIn_(syncStatus_, storedAccounts_)',
    },

    /** @private */
    eligibleForAccountStorage_: {
      type: Boolean,
      computed: 'computeEligibleForAccountStorage_(syncStatus_, signedIn_)',
    },

    /** @private */
    hasNeverCheckedPasswords_: {
      type: Boolean,
      computed: 'computeHasNeverCheckedPasswords_(status)',
    },

    /** @private */
    hasStoredPasswords_: {
      type: Boolean,
      value: false,
    },

    shouldShowBanner_: {
      type: Boolean,
      value: true,
      computed: 'computeShouldShowBanner_(hasLeakedCredentials_,' +
          'signedIn_, hasNeverCheckedPasswords_, hasStoredPasswords_)',
    },

    /** @private */
    hasLeakedCredentials_: {
      type: Boolean,
      computed: 'computeHasLeakedCredentials_(leakedPasswords)',
    },

    /** @private */
    hidePasswordsLink_: {
      type: Boolean,
      computed: 'computeHidePasswordsLink_(syncPrefs_, syncStatus_)',
    },

    /** @private */
    showExportPasswords_: {
      type: Boolean,
      computed: 'hasPasswords_(savedPasswords.splices)',
    },

    /** @private */
    showImportPasswords_: {
      type: Boolean,
      value() {
        return loadTimeData.valueExists('showImportPasswords') &&
            loadTimeData.getBoolean('showImportPasswords');
      }
    },

    /** @private */
    accountStorageFeatureEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('enableAccountStorage');
      }
    },

    /** @private */
    showPasswordEditDialog_: Boolean,

    /** @private */
    isOptedInForAccountStorage_: Boolean,

    /** @private {SyncPrefs} */
    syncPrefs_: Object,

    /** @private {SyncStatus} */
    syncStatus_: Object,

    /** Filter on the saved passwords and exceptions. */
    filter: {
      type: String,
      value: '',
    },

    /** @private {!PasswordManagerProxy.UiEntryWithPassword} */
    lastFocused_: Object,

    /** @private */
    listBlurred_: Boolean,

    // <if expr="chromeos">
    /** @private */
    showPasswordPromptDialog_: Boolean,

    /** @private {BlockingRequestManager} */
    tokenRequestManager_: Object
    // </if>
  },

  listeners: {
    'password-menu-tap': 'onPasswordMenuTap_',
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
   * A stack of the elements that triggered dialog to open and should therefore
   * receive focus when that dialog is closed. The bottom of the stack is the
   * element that triggered the earliest open dialog and top of the stack is the
   * element that triggered the most recent (i.e. active) dialog. If no dialog
   * is open, the stack is empty.
   * @private {!Array<Element>}
   */
  activeDialogAnchorStack_: [],

  /** @private {?PasswordManagerProxy} */
  passwordManager_: null,

  /**
   * @type {?function(boolean):void}
   * @private
   */
  setIsOptedInForAccountStorageListener_: null,

  /**
   * @type {?function(!Array<PasswordManagerProxy.PasswordUiEntry>):void}
   * @private
   */
  setSavedPasswordsListener_: null,

  /**
   * @type {?function(!Array<PasswordManagerProxy.ExceptionEntry>):void}
   * @private
   */
  setPasswordExceptionsListener_: null,

  /** @override */
  attached() {
    // Create listener functions.
    const setIsOptedInForAccountStorageListener = optedIn => {
      this.isOptedInForAccountStorage_ = optedIn;
    };

    const setSavedPasswordsListener = list => {
      const newList = list.map(entry => ({entry: entry, password: ''}));
      // Because the backend guarantees that item.entry.id uniquely identifies a
      // given entry and is stable with regard to mutations to the list, it is
      // sufficient to just use this id to create a item uid.
      this.updateList('savedPasswords', item => item.entry.id, newList);
      this.hasStoredPasswords_ = list.length > 0;
    };

    const setPasswordExceptionsListener = list => {
      this.passwordExceptions = list;
    };

    this.setIsOptedInForAccountStorageListener_ =
        setIsOptedInForAccountStorageListener;
    this.setSavedPasswordsListener_ = setSavedPasswordsListener;
    this.setPasswordExceptionsListener_ = setPasswordExceptionsListener;

    // Set the manager. These can be overridden by tests.
    this.passwordManager_ = PasswordManagerImpl.getInstance();

    // <if expr="chromeos">
    // If the user's account supports the password check, an auth token will be
    // required in order for them to view or export passwords. Otherwise there
    // is no additional security so |tokenRequestManager_| will immediately
    // resolve requests.
    if (loadTimeData.getBoolean('userCannotManuallyEnterPassword')) {
      this.tokenRequestManager_ = new BlockingRequestManager();
    } else {
      this.tokenRequestManager_ =
          new BlockingRequestManager(this.openPasswordPromptDialog_.bind(this));
    }
    // </if>

    // Request initial data.
    this.passwordManager_.isOptedInForAccountStorage().then(
        setIsOptedInForAccountStorageListener);
    this.passwordManager_.getSavedPasswordList(setSavedPasswordsListener);
    this.passwordManager_.getExceptionList(setPasswordExceptionsListener);

    // Listen for changes.
    this.passwordManager_.addAccountStorageOptInStateListener(
        setIsOptedInForAccountStorageListener);
    this.passwordManager_.addSavedPasswordListChangedListener(
        setSavedPasswordsListener);
    this.passwordManager_.addExceptionListChangedListener(
        setPasswordExceptionsListener);

    this.notifySplices('savedPasswords', []);

    const syncBrowserProxy = SyncBrowserProxyImpl.getInstance();

    const syncStatusChanged = syncStatus => this.syncStatus_ = syncStatus;
    syncBrowserProxy.getSyncStatus().then(syncStatusChanged);
    this.addWebUIListener('sync-status-changed', syncStatusChanged);

    const syncPrefsChanged = syncPrefs => this.syncPrefs_ = syncPrefs;
    syncBrowserProxy.sendSyncPrefsChanged();
    this.addWebUIListener('sync-prefs-changed', syncPrefsChanged);

    // For non-ChromeOS, also check whether accounts are available.
    // <if expr="not chromeos">
    const storedAccountsChanged = accounts => this.storedAccounts_ = accounts;
    syncBrowserProxy.getStoredAccounts().then(storedAccountsChanged);
    this.addWebUIListener('stored-accounts-updated', storedAccountsChanged);
    // </if>

    afterNextRender(this, function() {
      IronA11yAnnouncer.requestAvailability();
    });
  },

  /** @override */
  detached() {
    this.passwordManager_.removeSavedPasswordListChangedListener(
        assert(this.setSavedPasswordsListener_));
    this.passwordManager_.removeExceptionListChangedListener(
        assert(this.setPasswordExceptionsListener_));
    this.passwordManager_.removeAccountStorageOptInStateListener(
        assert(this.setIsOptedInForAccountStorageListener_));

    if (getToastManager().isToastOpen) {
      getToastManager().hide();
    }
  },

  /**
   * Shows the check passwords sub page.
   * @private
   */
  onCheckPasswordsClick_() {
    Router.getInstance().navigateTo(
        routes.CHECK_PASSWORDS, new URLSearchParams('start=true'));
    this.passwordManager_.recordPasswordCheckReferrer(
        PasswordManagerProxy.PasswordCheckReferrer.PASSWORD_SETTINGS);
  },

  // <if expr="chromeos">
  /**
   * When this event fired, it means that the password-prompt-dialog succeeded
   * in creating a fresh token in the quickUnlockPrivate API. Because new tokens
   * can only ever be created immediately following a GAIA password check, the
   * passwordsPrivate API can now safely grant requests for secure data (i.e.
   * saved passwords) for a limited time. This observer resolves the request,
   * triggering a callback that requires a fresh auth token to succeed and that
   * was provided to the BlockingRequestManager by another DOM element seeking
   * secure data.
   *
   * @param {!CustomEvent<!chrome.quickUnlockPrivate.TokenInfo>} e - Contains
   *     newly created auth token. Note that its precise value is not relevant
   *     here, only the facts that it's created.
   * @private
   */
  onTokenObtained_(e) {
    assert(e.detail);
    this.tokenRequestManager_.resolve();
  },

  onPasswordPromptClosed_() {
    this.showPasswordPromptDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchorStack_.pop()));
  },

  openPasswordPromptDialog_() {
    this.activeDialogAnchorStack_.push(getDeepActiveElement());
    this.showPasswordPromptDialog_ = true;
  },
  // </if>

  /**
   * Shows the edit password dialog.
   * @param {!Event} e
   * @private
   */
  onMenuEditPasswordTap_(e) {
    e.preventDefault();
    /** @type {CrActionMenuElement} */ (this.$.menu).close();
    this.showPasswordEditDialog_ = true;
  },

  /** @private */
  onPasswordEditDialogClosed_() {
    this.showPasswordEditDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchorStack_.pop()));

    // Trigger a re-evaluation of the activePassword as the visibility state of
    // the password might have changed.
    this.activePassword.notifyPath('item.password');
  },

  /**
   * @return {boolean}
   * @private
   */
  computeSignedIn_() {
    return !!this.syncStatus_ && !!this.syncStatus_.signedIn ?
        !this.syncStatus_.hasError :
        (!!this.storedAccounts_ && this.storedAccounts_.length > 0);
  },

  /**
   * @return {boolean}
   * @private
   */
  computeEligibleForAccountStorage_() {
    // |this.syncStatus_.signedIn| means the user has sync enabled, while
    // |this.signedIn_| means they have signed in, in the content area.
    return this.accountStorageFeatureEnabled_ &&
        (!!this.syncStatus_ && !this.syncStatus_.signedIn) && this.signedIn_;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShouldShowBanner_() {
    return this.signedIn_ && this.hasStoredPasswords_ &&
        this.hasNeverCheckedPasswords_ && !this.hasLeakedCredentials_;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeHidePasswordsLink_() {
    return !!this.syncStatus_ && !!this.syncStatus_.signedIn &&
        !!this.syncPrefs_ && !!this.syncPrefs_.encryptAllData;
  },

  /**
   * @param {string} filter
   * @return {!Array<!PasswordManagerProxy.UiEntryWithPassword>}
   * @private
   */
  getFilteredPasswords_(filter) {
    if (!filter) {
      return this.savedPasswords.slice();
    }

    return this.savedPasswords.filter(
        p => [p.entry.urls.shown, p.entry.username].some(
            term => term.toLowerCase().includes(filter.toLowerCase())));
  },

  /**
   * @param {string} filter
   * @return {function(!chrome.passwordsPrivate.ExceptionEntry): boolean}
   * @private
   */
  passwordExceptionFilter_(filter) {
    return exception => exception.urls.shown.toLowerCase().includes(
               filter.toLowerCase());
  },

  /**
   * Fires an event that should delete the saved password.
   * @private
   */
  onMenuRemovePasswordTap_() {
    this.passwordManager_.removeSavedPassword(
        this.activePassword.item.entry.id);
    getToastManager().show(this.i18n('passwordDeleted'));
    this.fire('iron-announce', {
      text: this.i18n('undoDescription'),
    });
    /** @type {CrActionMenuElement} */ (this.$.menu).close();
  },

  /**
   * Copy selected password to clipboard.
   * @private
   */
  onMenuCopyPasswordButtonTap_() {
    // Copy to clipboard occurs inside C++ and we don't expect getting
    // result back to javascript.
    this.passwordManager_
        .requestPlaintextPassword(
            this.activePassword.item.entry.id,
            chrome.passwordsPrivate.PlaintextReason.COPY)
        .catch(error => {
          // <if expr="chromeos">
          // If no password was found, refresh auth token and retry.
          this.tokenRequestManager_.request(
              this.onMenuCopyPasswordButtonTap_.bind(this));
          // </if>});
        });
    (this.$.menu).close();
  },

  /**
   * Handle the undo shortcut.
   * @param {!Event} event
   * @private
   */
  onUndoKeyBinding_(event) {
    const activeElement = getDeepActiveElement();
    if (!activeElement || !isEditable(activeElement)) {
      this.passwordManager_.undoRemoveSavedPasswordOrException();
      getToastManager().hide();
      // Preventing the default is necessary to not conflict with a possible
      // search action.
      event.preventDefault();
    }
  },

  /** @private */
  onUndoButtonClick_() {
    this.passwordManager_.undoRemoveSavedPasswordOrException();
    getToastManager().hide();
  },

  /**
   * Fires an event that should delete the password exception.
   * @param {!ExceptionEntryEntryEvent} e The polymer event.
   * @private
   */
  onRemoveExceptionButtonTap_(e) {
    this.passwordManager_.removeException(e.model.item.id);
  },

  /**
   * Opens the password action menu.
   * @param {!Event} event
   * @private
   */
  onPasswordMenuTap_(event) {
    const menu = /** @type {!CrActionMenuElement} */ (this.$.menu);
    const target = /** @type {!HTMLElement} */ (event.detail.target);

    this.activePassword =
        /** @type {!PasswordListItemElement} */ (event.detail.listItem);
    menu.showAt(target);
    this.activeDialogAnchorStack_.push(target);
  },

  /**
   * Opens the export/import action menu.
   * @private
   */
  onImportExportMenuTap_() {
    const menu = /** @type {!CrActionMenuElement} */ (this.$.exportImportMenu);
    const target =
        /** @type {!HTMLElement} */ (this.$$('#exportImportMenuButton'));

    menu.showAt(target);
    this.activeDialogAnchorStack_.push(target);
  },

  /**
   * Fires an event that should trigger the password import process.
   * @private
   */
  onImportTap_() {
    this.passwordManager_.importPasswords();
    this.$.exportImportMenu.close();
  },

  /**
   * Opens the export passwords dialog.
   * @private
   */
  onExportTap_() {
    this.showPasswordsExportDialog_ = true;
    this.$.exportImportMenu.close();
  },

  /** @private */
  onPasswordsExportDialogClosed_() {
    this.showPasswordsExportDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchorStack_.pop()));
  },

  /** @private */
  onOptIn_: function() {
    this.passwordManager_.optInForAccountStorage(true);
  },

  /** @private */
  onOptOut_: function() {
    this.passwordManager_.optInForAccountStorage(false);
  },

  /**
   * Returns true if the list exists and has items.
   * @param {Array<Object>} list
   * @return {boolean}
   * @private
   */
  hasSome_(list) {
    return !!(list && list.length);
  },

  /** @private */
  hasPasswords_() {
    return this.savedPasswords.length > 0;
  },

  /**
   * @private
   * @param {boolean} showExportPasswords
   * @param {boolean} showImportPasswords
   * @return {boolean}
   */
  showImportOrExportPasswords_(showExportPasswords, showImportPasswords) {
    return showExportPasswords || showImportPasswords;
  },

  /**
   * @private
   * @param {!PasswordManagerProxy.ExceptionEntry} item This row's item.
   * @return {string}
   */
  getStorageText_(item) {
    // TODO(crbug.com/1049141): Add proper translated strings once we have them.
    return item.fromAccountStore ? 'Account' : 'Local';
  },

  /**
   * @private
   * @param {!PasswordManagerProxy.ExceptionEntry} item This row's item.
   * @return {string}
   */
  getStorageIcon_(item) {
    // TODO(crbug.com/1049141): Add the proper icons once we know them.
    return item.fromAccountStore ? 'cr:sync' : 'cr:computer';
  },

  /**
   * @private
   * @return {boolean}
   */
  computeHasLeakedCredentials_() {
    return this.leakedPasswords.length > 0;
  },

  /**
   * @private
   * @return {boolean}
   */
  computeHasNeverCheckedPasswords_() {
    return !this.status.elapsedTimeSinceLastCheck;
  },
});
