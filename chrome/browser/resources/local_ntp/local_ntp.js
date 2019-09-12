// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The local InstantExtended NTP.
 */

// Global local statics (visible for testing).

/**
 * Whether the Most Visited and edit custom link iframes should be created while
 * running tests. Currently the SimpleJavascriptTests are flaky due to some
 * raciness in the creation/destruction of the iframe. crbug.com/786313.
 * @type {boolean}
 */
let iframesAndVoiceSearchDisabledForTesting = false;

/**
 * Whether the most visited tiles have finished loading, i.e. we've received the
 * 'loaded' postMessage from the iframe. Used by tests to detect that loading
 * has completed.
 * @type {boolean}
 */
let tilesAreLoaded = false;

/**
 * Controls rendering the new tab page for InstantExtended.
 * @return {Object} A limited interface for testing the local NTP.
 */
function LocalNTP() {
'use strict';

// Type definitions.

/** @enum {number} */
const ACMatchClassificationStyle = {
  NONE: 0,
  URL: 1 << 0,
  MATCH: 1 << 1,
  DIM: 1 << 2,
};

/** @enum {number} */
const AutocompleteResultStatus = {
  SUCCESS: 0,
  SKIPPED: 1,
};

/** @typedef {{inline: string, text: string}} */
let RealboxOutput;

/**
 * @typedef {{
 *   moveCursorToEnd: (boolean|undefined),
 *   inline: (string|undefined),
 *   text: (string|undefined),
 * }}
 */
let RealboxOutputUpdate;

// Constants.

/**
 * Enum for classnames.
 * @enum {string}
 * @const
 */
const CLASSES = {
  ALTERNATE_LOGO: 'alternate-logo',  // Shows white logo if required by theme
  // Shows a clock next to historical realbox results.
  CLOCK_ICON: 'clock-icon',
  // Applies styles to dialogs used in customization.
  CUSTOMIZE_DIALOG: 'customize-dialog',
  DARK: 'dark',
  DEFAULT_THEME: 'default-theme',
  DELAYED_HIDE_NOTIFICATION: 'mv-notice-delayed-hide',
  // Extended and elevated style for customization entry point.
  ENTRY_POINT_ENHANCED: 'ep-enhanced',
  FAKEBOX_FOCUS: 'fakebox-focused',  // Applies focus styles to the fakebox
  // Applies float animations to the Most Visited notification
  FLOAT_DOWN: 'float-down',
  FLOAT_UP: 'float-up',
  // Applies drag focus style to the fakebox
  FAKEBOX_DRAG_FOCUS: 'fakebox-drag-focused',
  // Applies a different style to the error notification if a link is present.
  HAS_LINK: 'has-link',
  HIDE_FAKEBOX: 'hide-fakebox',
  HIDE_NOTIFICATION: 'notice-hide',
  INITED: 'inited',  // Reveals the <body> once init() is done.
  LEFT_ALIGN_ATTRIBUTION: 'left-align-attribution',
  // Vertically centers the most visited section for a non-Google provided page.
  NON_GOOGLE_PAGE: 'non-google-page',
  NON_WHITE_BG: 'non-white-bg',
  RTL: 'rtl',                  // Right-to-left language text.
  SEARCH_ICON: 'search-icon',  // Magnifying glass/search icon.
  SELECTED: 'selected',  // A selected (via up/down arrow key) realbox match.
  SHOW_ELEMENT: 'show-element',
  // When the realbox has matches to show.
  SHOW_MATCHES: 'show-matches',
  // Applied when the fakebox placeholder text should not be hidden on focus.
  SHOW_PLACEHOLDER: 'show-placeholder',
  URL_ICON: 'url-icon',  // Global/favicon/url icon.
  // Applied when the doodle notifier should be shown instead of the doodle.
  USE_NOTIFIER: 'use-notifier',
};

const SEARCH_HISTORY_MATCH_TYPES = [
  'search-history',
  'search-suggest-personalized',
];

/**
 * Background color for Chrome dark mode. Used to determine if it is possible to
 * display a Google Doodle, or if the notifier should be used instead.
 * @type {string}
 * @const
 */
const DARK_MODE_BACKGROUND_COLOR = 'rgba(50,54,57,1)';

/**
 * The period of time (ms) before transitions can be applied to a toast
 * notification after modifying the "display" property.
 * @type {number}
 */
const DISPLAY_TIMEOUT = 20;

/**
 * Enum for HTML element ids.
 * @enum {string}
 * @const
 */
const IDS = {
  ATTRIBUTION: 'attribution',
  ATTRIBUTION_TEXT: 'attribution-text',
  CUSTOM_BG: 'custom-bg',
  CUSTOM_LINKS_EDIT_IFRAME: 'custom-links-edit',
  CUSTOM_LINKS_EDIT_IFRAME_DIALOG: 'custom-links-edit-dialog',
  ERROR_NOTIFICATION: 'error-notice',
  ERROR_NOTIFICATION_CONTAINER: 'error-notice-container',
  ERROR_NOTIFICATION_LINK: 'error-notice-link',
  ERROR_NOTIFICATION_MSG: 'error-notice-msg',
  FAKEBOX: 'fakebox',
  FAKEBOX_INPUT: 'fakebox-input',
  FAKEBOX_TEXT: 'fakebox-text',
  FAKEBOX_MICROPHONE: 'fakebox-microphone',
  MOST_VISITED: 'most-visited',
  NOTIFICATION: 'mv-notice',
  NOTIFICATION_CONTAINER: 'mv-notice-container',
  NOTIFICATION_MESSAGE: 'mv-msg',
  NTP_CONTENTS: 'ntp-contents',
  OGB: 'one-google',
  PROMO: 'promo',
  REALBOX: 'realbox',
  REALBOX_INPUT_WRAPPER: 'realbox-input-wrapper',
  REALBOX_MATCHES: 'realbox-matches',
  REALBOX_MICROPHONE: 'realbox-microphone',
  RESTORE_ALL_LINK: 'mv-restore',
  SUGGESTIONS: 'suggestions',
  TILES: 'mv-tiles',
  TILES_IFRAME: 'mv-single',
  UNDO_LINK: 'mv-undo',
  USER_CONTENT: 'user-content',
};

/**
 * The different types of events that are logged from the NTP. This enum is
 * used to transfer information from the NTP JavaScript to the renderer and is
 * not used as a UMA enum histogram's logged value.
 * Note: Keep in sync with common/ntp_logging_events.h
 * @enum {number}
 * @const
 */
const LOG_TYPE = {
  // The One Google Bar was shown.
  NTP_ONE_GOOGLE_BAR_SHOWN: 37,

  // 'Cancel' was clicked in the 'Edit shortcut' dialog.
  NTP_CUSTOMIZE_SHORTCUT_CANCEL: 54,
  // 'Done' was clicked in the 'Edit shortcut' dialog.
  NTP_CUSTOMIZE_SHORTCUT_DONE: 55,

  // A middle slot promo was shown.
  NTP_MIDDLE_SLOT_PROMO_SHOWN: 60,
  // A promo link was clicked.
  NTP_MIDDLE_SLOT_PROMO_LINK_CLICKED: 61,
};

/**
 * The maximum number of tiles to show in the Most Visited section if custom
 * links is enabled.
 * @type {number}
 * @const
 */
const MAX_NUM_TILES_CUSTOM_LINKS = 10;

/**
 * The maximum number of tiles to show in the Most Visited section.
 * @type {number}
 * @const
 */
const MAX_NUM_TILES_MOST_VISITED = 8;

/**
 * The period of time (ms) before the Most Visited notification is hidden.
 * @type {number}
 */
const NOTIFICATION_TIMEOUT = 10000;

/**
 * Specifications for an NTP design (not comprehensive).
 *
 * backgroundColor: The 4-component color of default background,
 * darkBackgroundColor: The 4-component color of default dark background,
 * iconBackgroundColor: The 4-component color of default dark icon background,
 * iconDarkBackgroundColor: The 4-component color of default icon background,
 * numTitleLines: Number of lines to display in titles.
 * titleColor: The 4-component color of title text.
 * titleColorAgainstDark: The 4-component color of title text against a dark
 *   theme.
 *
 * @type {{
 *   backgroundColor: !Array<number>,
 *   darkBackgroundColor: !Array<number>,
 *   iconBackgroundColor: !Array<number>,
 *   iconDarkBackgroundColor: !Array<number>,
 *   numTitleLines: number,
 *   titleColor: !Array<number>,
 *   titleColorAgainstDark: !Array<number>,
 * }}
 */
const NTP_DESIGN = {
  backgroundColor: [255, 255, 255, 255],
  darkBackgroundColor: [50, 54, 57, 255],
  iconBackgroundColor: [241, 243, 244, 255],  /** GG100 */
  iconDarkBackgroundColor: [32, 33, 36, 255], /** GG900 */
  numTitleLines: 1,
  titleColor: [60, 64, 67, 255],               /** GG800 */
  titleColorAgainstDark: [248, 249, 250, 255], /** GG050 */
};

const REALBOX_KEYDOWN_HANDLED_KEYS = [
  'ArrowDown',
  'ArrowUp',
  'Enter',
  'Escape',
  'PageDown',
  'PageUp',
];

/**
 * Background colors considered "white". Used to determine if it is possible to
 * display a Google Doodle, or if the notifier should be used instead. Also used
 * to determine if a colored or white logo should be used.
 * @const
 */
const WHITE_BACKGROUND_COLORS = ['rgba(255,255,255,1)', 'rgba(0,0,0,0)'];

// Local statics.

/** @type {!Array<!AutocompleteMatch>} */
let autocompleteMatches = [];

/**
 * The currently visible notification element. Null if no notification is
 * present.
 * @type {?Object}
 */
let currNotification = null;

/**
 * The timeout function for automatically hiding the pop-up notification. Only
 * set if a notification is visible.
 * @type {?Object}
 */
let delayedHideNotification = null;

/**
 * True if dark mode is enabled.
 * @type {boolean}
 */
let isDarkModeEnabled = false;

/** Used to prevent inline autocompleting recently deleted output. */
let isDeleting = false;

/**
 * The last blacklisted tile rid if any, which by definition should not be
 * filler.
 * @type {?number}
 */
let lastBlacklistedTile = null;

/**
 * Last text/inline autocompletion shown in the realbox (either by user input or
 * outputting autocomplete matches).
 * @type {!RealboxOutput}
 */
let lastOutput = {text: '', inline: ''};

/**
 * The browser embeddedSearch.newTabPage object.
 * @type {Object}
 */
let ntpApiHandle;

// Helper methods.

/**
 * Converts an Array of color components into RGBA format "rgba(R,G,B,A)".
 * @param {Array<number>} color Array of rgba color components.
 * @return {string} CSS color in RGBA format.
 */
function convertToRGBAColor(color) {
  return 'rgba(' + color[0] + ',' + color[1] + ',' + color[2] + ',' +
      color[3] / 255 + ')';
}

/**
 * Returns a timeout that can be executed early. Calls back true if this was
 * an early execution, false otherwise.
 * @param {!Function} timeout The timeout function. Requires a boolean param.
 * @param {number} delay The timeout delay.
 * @return {Object}
 */
function createExecutableTimeout(timeout, delay) {
  const timeoutId = window.setTimeout(() => {
    timeout(/*executedEarly=*/ false);
  }, delay);
  return {
    clear: () => {
      window.clearTimeout(timeoutId);
    },
    trigger: () => {
      window.clearTimeout(timeoutId);
      return timeout(/*executedEarly=*/ true);
    }
  };
}

/** Create the Most Visited and edit custom links iframes. */
function createIframes() {
  // Collect arguments for the most visited iframe.
  const args = [];

  const searchboxApiHandle = window.chrome.embeddedSearch.searchBox;

  if (searchboxApiHandle.rtl) {
    args.push('rtl=1');
  }
  if (NTP_DESIGN.numTitleLines > 1) {
    args.push('ntl=' + NTP_DESIGN.numTitleLines);
  }

  args.push(
      'title=' +
      encodeURIComponent(configData.translatedStrings.mostVisitedTitle));
  args.push(
      'removeTooltip=' +
      encodeURIComponent(configData.translatedStrings.removeThumbnailTooltip));

  if (configData.isGooglePage) {
    args.push('enableCustomLinks=1');
    if (configData.enableShortcutsGrid) {
      args.push('enableGrid=1');
    }
    args.push(
        'addLink=' +
        encodeURIComponent(configData.translatedStrings.addLinkTitle));
    args.push(
        'addLinkTooltip=' +
        encodeURIComponent(configData.translatedStrings.addLinkTooltip));
    args.push(
        'editLinkTooltip=' +
        encodeURIComponent(configData.translatedStrings.editLinkTooltip));
  }

  // Create the most visited iframe.
  const iframe = document.createElement('iframe');
  iframe.id = IDS.TILES_IFRAME;
  iframe.name = IDS.TILES_IFRAME;
  iframe.title = configData.translatedStrings.mostVisitedTitle;
  iframe.src = 'chrome-search://most-visited/single.html?' + args.join('&');
  $(IDS.TILES).appendChild(iframe);

  iframe.onload = function() {
    sendThemeInfoToMostVisitedIframe();
    reloadTiles();
  };

  if (configData.isGooglePage) {
    // Collect arguments for the edit custom link iframe.
    const clArgs = [];

    if (searchboxApiHandle.rtl) {
      clArgs.push('rtl=1');
    }

    clArgs.push(
        'addTitle=' +
        encodeURIComponent(configData.translatedStrings.addLinkTitle));
    clArgs.push(
        'editTitle=' +
        encodeURIComponent(configData.translatedStrings.editLinkTitle));
    clArgs.push(
        'nameField=' +
        encodeURIComponent(configData.translatedStrings.nameField));
    clArgs.push(
        'urlField=' +
        encodeURIComponent(configData.translatedStrings.urlField));
    clArgs.push(
        'linkRemove=' +
        encodeURIComponent(configData.translatedStrings.linkRemove));
    clArgs.push(
        'linkCancel=' +
        encodeURIComponent(configData.translatedStrings.linkCancel));
    clArgs.push(
        'linkDone=' +
        encodeURIComponent(configData.translatedStrings.linkDone));
    clArgs.push(
        'invalidUrl=' +
        encodeURIComponent(configData.translatedStrings.invalidUrl));

    // Create the edit custom link iframe.
    const clIframe = document.createElement('iframe');
    clIframe.id = IDS.CUSTOM_LINKS_EDIT_IFRAME;
    clIframe.name = IDS.CUSTOM_LINKS_EDIT_IFRAME;
    clIframe.title = configData.translatedStrings.editLinkTitle;
    clIframe.src = 'chrome-search://most-visited/edit.html?' + clArgs.join('&');
    const clIframeDialog = document.createElement('dialog');
    clIframeDialog.id = IDS.CUSTOM_LINKS_EDIT_IFRAME_DIALOG;
    clIframeDialog.classList.add(CLASSES.CUSTOMIZE_DIALOG);
    clIframeDialog.appendChild(clIframe);
    document.body.appendChild(clIframeDialog);
  }

  window.addEventListener('message', handlePostMessage);
}

/**
 * Return true if custom links are enabled.
 * @return {boolean}
 */
function customLinksEnabled() {
  return configData.isGooglePage &&
      !chrome.embeddedSearch.newTabPage.isUsingMostVisited;
}

/**
 * Called by tests to disable the creation of Most Visited and edit custom link
 * iframes.
 */
function disableIframesAndVoiceSearchForTesting() {
  iframesAndVoiceSearchDisabledForTesting = true;
}

/**
 * Animates the pop-up notification to float down, and clears the timeout to
 * hide the notification.
 * @param {?Element} notification The notification element.
 * @param {?Element} notificationContainer The notification container element.
 * @param {boolean} showPromo Do show the promo if present.
 */
function floatDownNotification(notification, notificationContainer, showPromo) {
  if (!notification || !notificationContainer) {
    return;
  }

  if (!notificationContainer.classList.contains(CLASSES.FLOAT_UP)) {
    return;
  }

  // Clear the timeout to hide the notification.
  if (delayedHideNotification) {
    delayedHideNotification.clear();
    delayedHideNotification = null;
    currNotification = null;
  }

  if (showPromo) {
    // Show middle-slot promo if one is present.
    const promo = $(IDS.PROMO);
    if (promo) {
      promo.classList.remove(CLASSES.HIDE_NOTIFICATION);
      // Timeout is required for the "float" transition to work. Modifying the
      // "display" property prevents transitions from activating for a brief
      // period of time.
      window.setTimeout(() => {
        promo.classList.remove(CLASSES.FLOAT_DOWN);
      }, DISPLAY_TIMEOUT);
    }
  }

  // Reset notification visibility once the animation is complete.
  notificationContainer.addEventListener('transitionend', (event) => {
    // Blur the hidden items.
    $(IDS.UNDO_LINK).blur();
    $(IDS.RESTORE_ALL_LINK).blur();
    if (notification.classList.contains(CLASSES.HAS_LINK)) {
      notification.classList.remove(CLASSES.HAS_LINK);
      $(IDS.ERROR_NOTIFICATION_LINK).blur();
    }
    // Hide the notification
    if (!notification.classList.contains(CLASSES.FLOAT_UP)) {
      notification.classList.add(CLASSES.HIDE_NOTIFICATION);
    }
  }, {once: true});
  notificationContainer.classList.remove(CLASSES.FLOAT_UP);
}

/**
 * Animates the specified notification to float up. Automatically hides any
 * pre-existing notification and sets a delayed timer to hide the new
 * notification.
 * @param {?Element} notification The notification element.
 * @param {?Element} notificationContainer The notification container element.
 */
function floatUpNotification(notification, notificationContainer) {
  if (!notification || !notificationContainer) {
    return;
  }

  // Hide any pre-existing notification.
  if (delayedHideNotification) {
    // Hide the current notification if it's a different type (i.e. error vs
    // success). Otherwise, simply clear the notification timeout and reset it
    // later.
    if (currNotification === notificationContainer) {
      delayedHideNotification.clear();
    } else {
      delayedHideNotification.trigger();
    }
    delayedHideNotification = null;
  }

  // Hide middle-slot promo if one is present.
  const promo = $(IDS.PROMO);
  if (promo) {
    promo.classList.add(CLASSES.FLOAT_DOWN);
    // Prevent keyboard focus once the promo is hidden.
    promo.addEventListener('transitionend', (event) => {
      if (event.propertyName === 'bottom' &&
          promo.classList.contains(CLASSES.FLOAT_DOWN)) {
        promo.classList.add(CLASSES.HIDE_NOTIFICATION);
      }
    }, {once: true});
  }

  notification.classList.remove(CLASSES.HIDE_NOTIFICATION);
  // Timeout is required for the "float" transition to work. Modifying the
  // "display" property prevents transitions from activating for a brief period
  // of time.
  window.setTimeout(() => {
    notificationContainer.classList.add(CLASSES.FLOAT_UP);
  }, DISPLAY_TIMEOUT);

  // Automatically hide the notification after a period of time.
  delayedHideNotification = createExecutableTimeout((executedEarly) => {
    // Early execution occurs if another notification should be shown. In this
    // case, we do not want to re-show the promo yet.
    floatDownNotification(notification, notificationContainer, !executedEarly);
  }, NOTIFICATION_TIMEOUT);
  currNotification = notificationContainer;
}

/**
 * Returns theme background info, first checking for history.state.notheme. If
 * the page has notheme set, returns a fallback light-colored theme (or dark-
 * colored theme if dark mode is enabled). This is used when the doodle is
 * displayed after clicking the notifier.
 * @return {?ThemeBackgroundInfo}
 */
function getThemeBackgroundInfo() {
  if (history.state && history.state.notheme) {
    return {
      alternateLogo: false,
      backgroundColorRgba:
          (isDarkModeEnabled ? NTP_DESIGN.darkBackgroundColor :
                               NTP_DESIGN.backgroundColor),
      customBackgroundConfigured: false,
      iconBackgroundColor:
          (isDarkModeEnabled ? NTP_DESIGN.iconDarkBackgroundColor :
                               NTP_DESIGN.iconBackgroundColor),
      isNtpBackgroundDark: isDarkModeEnabled,
      textColorLightRgba: [102, 102, 102, 255],
      textColorRgba:
          (isDarkModeEnabled ? NTP_DESIGN.titleColorAgainstDark :
                               NTP_DESIGN.titleColor),
      useTitleContainer: false,
      useWhiteAddIcon: isDarkModeEnabled,
      usingDefaultTheme: true,
    };
  }

  const info = window.chrome.embeddedSearch.newTabPage.themeBackgroundInfo;
  const preview = $(customize.IDS.CUSTOM_BG_PREVIEW);
  if (preview.dataset.hasPreview === 'true') {
    info.isNtpBackgroundDark = preview.dataset.hasImage === 'true';
    info.customBackgroundConfigured = preview.dataset.hasImage === 'true';
    info.alternateLogo = preview.dataset.hasImage === 'true';
    // backgroundImage is in the form: url("actual url"). Remove everything
    // except the actual url.
    info.imageUrl = preview.style.backgroundImage.slice(5, -2);
  }
  return info;
}

/**
 * Determine whether dark chips should be used if dark mode is enabled. This is
 * is the case when dark mode is enabled and a background image (from a custom
 * background or user theme) is not set.
 *
 * @param {!Object} info Theme background information.
 * @return {boolean} Whether the chips should be dark.
 */
function getUseDarkChips(info) {
  return window.matchMedia('(prefers-color-scheme: dark)').matches &&
      !info.imageUrl;
}

/**
 * Event handler for messages from the most visited and edit custom link iframe.
 * @param {Event} event Event received.
 */
function handlePostMessage(event) {
  const cmd = event.data.cmd;
  const args = event.data;
  if (cmd === 'loaded') {
    tilesAreLoaded = true;
    if (configData.isGooglePage) {
      // Show search suggestions, promo, and the OGB if they were previously
      // hidden.
      if ($(IDS.SUGGESTIONS)) {
        $(IDS.SUGGESTIONS).style.visibility = 'visible';
      }
      if ($(IDS.PROMO)) {
        $(IDS.PROMO).classList.add(CLASSES.SHOW_ELEMENT);
      }
      if (customLinksEnabled()) {
        $(customize.IDS.CUSTOM_LINKS_RESTORE_DEFAULT)
            .classList.toggle(
                customize.CLASSES.OPTION_DISABLED, !args.showRestoreDefault);
        $(customize.IDS.CUSTOM_LINKS_RESTORE_DEFAULT).tabIndex =
            (args.showRestoreDefault ? 0 : -1);
      }
      $(IDS.OGB).classList.add(CLASSES.SHOW_ELEMENT);
    }
  } else if (cmd === 'tileBlacklisted') {
    if (customLinksEnabled()) {
      showNotification(configData.translatedStrings.linkRemovedMsg);
    } else {
      showNotification(
          configData.translatedStrings.thumbnailRemovedNotification);
    }
    lastBlacklistedTile = args.rid;

    ntpApiHandle.deleteMostVisitedItem(args.rid);
  } else if (cmd === 'resizeDoodle') {
    doodles.resizeDoodleHandler(args);
  } else if (cmd === 'startEditLink') {
    $(IDS.CUSTOM_LINKS_EDIT_IFRAME)
        .contentWindow.postMessage({cmd: 'linkData', rid: args.rid}, '*');
    // Small delay to allow the dialog to finish setting up before displaying.
    window.setTimeout(function() {
      $(IDS.CUSTOM_LINKS_EDIT_IFRAME_DIALOG).showModal();
    }, 10);
  } else if (cmd === 'closeDialog') {
    $(IDS.CUSTOM_LINKS_EDIT_IFRAME_DIALOG).close();
  } else if (cmd === 'focusMenu') {
    // Focus the edited tile's menu or the add shortcut tile after closing the
    // custom link edit dialog without saving.
    const iframe = $(IDS.TILES_IFRAME);
    if (iframe) {
      iframe.contentWindow.postMessage({cmd: 'focusMenu', rid: args.rid}, '*');
    }
  }
}

/** Hides the Most Visited pop-up notification. */
function hideNotification() {
  floatDownNotification(
      $(IDS.NOTIFICATION), $(IDS.NOTIFICATION_CONTAINER), /*showPromo=*/ true);
}

function hideRealboxMatches() {
  const realboxWrapper = $(IDS.REALBOX_INPUT_WRAPPER);
  realboxWrapper.classList.remove(CLASSES.SHOW_MATCHES);
  realboxWrapper.removeEventListener('keydown', onRealboxKeyDown);
  autocompleteMatches = [];
}

/**
 * Prepares the New Tab Page by adding listeners, the most visited pages
 * section, and Google-specific elements for a Google-provided page.
 */
function init() {
  // If an accessibility tool is in use, increase the time for which the
  // "tile was blacklisted" notification is shown.
  if (configData.isAccessibleBrowser) {
    document.body.style.setProperty('--mv-notice-time', '30s');
  }

  // Hide notifications after fade out, so we can't focus on links via keyboard.
  $(IDS.NOTIFICATION).addEventListener('transitionend', (event) => {
    if (event.propertyName === 'opacity') {
      hideNotification();
    }
  });

  $(IDS.NOTIFICATION_MESSAGE).textContent =
      configData.translatedStrings.thumbnailRemovedNotification;

  const undoLink = $(IDS.UNDO_LINK);
  undoLink.addEventListener('click', onUndo);
  registerKeyHandler(undoLink, ['Enter', ' '], onUndo);
  undoLink.textContent = configData.translatedStrings.undoThumbnailRemove;

  const restoreAllLink = $(IDS.RESTORE_ALL_LINK);
  restoreAllLink.addEventListener('click', onRestoreAll);
  registerKeyHandler(restoreAllLink, ['Enter', ' '], onRestoreAll);

  $(IDS.ATTRIBUTION_TEXT).textContent =
      configData.translatedStrings.attributionIntro;

  const embeddedSearchApiHandle = window.chrome.embeddedSearch;

  ntpApiHandle = embeddedSearchApiHandle.newTabPage;
  ntpApiHandle.onthemechange = onThemeChange;
  ntpApiHandle.onmostvisitedchange = onMostVisitedChange;

  renderTheme();

  window.matchMedia('(prefers-color-scheme: dark)').onchange = onThemeChange;

  const searchboxApiHandle = embeddedSearchApiHandle.searchBox;

  if (configData.isGooglePage) {
    requestAndInsertGoogleResources();
    animations.addRippleAnimations();

    ntpApiHandle.onaddcustomlinkdone = onAddCustomLinkDone;
    ntpApiHandle.onupdatecustomlinkdone = onUpdateCustomLinkDone;
    ntpApiHandle.ondeletecustomlinkdone = onDeleteCustomLinkDone;

    customize.init(showErrorNotification, hideNotification);

    if (configData.realboxEnabled) {
      const realboxEl = $(IDS.REALBOX);
      realboxEl.placeholder = configData.translatedStrings.searchboxPlaceholder;
      realboxEl.classList.toggle(
          CLASSES.SHOW_PLACEHOLDER, configData.showPlaceholderOnFocus);
      realboxEl.addEventListener('copy', onRealboxCopy);
      realboxEl.addEventListener('input', onRealboxInput);

      const realboxWrapper = $(IDS.REALBOX_INPUT_WRAPPER);
      realboxWrapper.addEventListener('focusin', onRealboxWrapperFocusIn);
      realboxWrapper.addEventListener('focusout', onRealboxWrapperFocusOut);

      searchboxApiHandle.onqueryautocompletedone = onQueryAutocompleteDone;

      if (!iframesAndVoiceSearchDisabledForTesting) {
        speech.init(
            configData.googleBaseUrl, configData.translatedStrings,
            $(IDS.REALBOX_MICROPHONE), searchboxApiHandle);
      }

      utils.disableOutlineOnMouseClick($(IDS.REALBOX_MICROPHONE));
    } else {
      if (configData.showPlaceholderOnFocus) {
        $(IDS.FAKEBOX_TEXT).classList.add(CLASSES.SHOW_PLACEHOLDER);
      }

      // Set up the fakebox (which only exists on the Google NTP).
      ntpApiHandle.oninputstart = onInputStart;
      ntpApiHandle.oninputcancel = onInputCancel;

      if (ntpApiHandle.isInputInProgress) {
        onInputStart();
      }

      $(IDS.FAKEBOX_TEXT).textContent =
          configData.translatedStrings.searchboxPlaceholder;

      if (!iframesAndVoiceSearchDisabledForTesting) {
        speech.init(
            configData.googleBaseUrl, configData.translatedStrings,
            $(IDS.FAKEBOX_MICROPHONE), searchboxApiHandle);
      }

      // Listener for updating the key capture state.
      document.body.onmousedown = function(event) {
        if (isFakeboxClick(event)) {
          searchboxApiHandle.startCapturingKeyStrokes();
        } else if (isFakeboxFocused()) {
          searchboxApiHandle.stopCapturingKeyStrokes();
        }
      };
      searchboxApiHandle.onkeycapturechange = function() {
        setFakeboxFocus(searchboxApiHandle.isKeyCaptureEnabled);
      };
      const inputbox = $(IDS.FAKEBOX_INPUT);
      inputbox.onpaste = function(event) {
        event.preventDefault();
        // Send pasted text to Omnibox.
        const text = event.clipboardData.getData('text/plain');
        if (text) {
          searchboxApiHandle.paste(text);
        }
      };
      inputbox.ondrop = function(event) {
        event.preventDefault();
        const text = event.dataTransfer.getData('text/plain');
        if (text) {
          searchboxApiHandle.paste(text);
        }
        setFakeboxDragFocus(false);
      };
      inputbox.ondragenter = function() {
        setFakeboxDragFocus(true);
      };
      inputbox.ondragleave = function() {
        setFakeboxDragFocus(false);
      };
      utils.disableOutlineOnMouseClick($(IDS.FAKEBOX_MICROPHONE));

      // Update the fakebox style to match the current key capturing state.
      setFakeboxFocus(searchboxApiHandle.isKeyCaptureEnabled);
      // Also tell the browser that we're capturing, otherwise it's possible
      // that both fakebox and Omnibox have visible focus at the same time, see
      // crbug.com/792850.
      if (searchboxApiHandle.isKeyCaptureEnabled) {
        searchboxApiHandle.startCapturingKeyStrokes();
      }
    }

    doodles.init();
  } else {
    document.body.classList.add(CLASSES.NON_GOOGLE_PAGE);
  }

  if (searchboxApiHandle.rtl) {
    $(IDS.NOTIFICATION).dir = 'rtl';
    // Grabbing the root HTML element. TODO(dbeam): could this just be <html ...
    // dir="$i18n{textdirection}"> in the .html file instead? It could result in
    // less flicker for RTL users (as HTML/CSS can render before JavaScript has
    // the chance to run).
    document.documentElement.setAttribute('dir', 'rtl');
    // Add class for setting alignments based on language directionality.
    document.documentElement.classList.add(CLASSES.RTL);
  }

  if (!iframesAndVoiceSearchDisabledForTesting) {
    createIframes();
  }

  utils.setPlatformClass(document.body);
  utils.disableOutlineOnMouseClick($(customize.IDS.EDIT_BG));
  document.documentElement.classList.add(CLASSES.INITED);
}

/**
 * Injects the One Google Bar into the page. Called asynchronously, so that it
 * doesn't block the main page load.
 */
function injectOneGoogleBar(ogb) {
  if (ogb.barHtml === '') {
    return;
  }

  const inHeadStyle = document.createElement('style');
  inHeadStyle.type = 'text/css';
  inHeadStyle.appendChild(document.createTextNode(ogb.inHeadStyle));
  document.head.appendChild(inHeadStyle);

  const inHeadScript = document.createElement('script');
  inHeadScript.type = 'text/javascript';
  inHeadScript.appendChild(document.createTextNode(ogb.inHeadScript));
  document.head.appendChild(inHeadScript);

  renderOneGoogleBarTheme();

  const ogElem = $('one-google');
  ogElem.innerHTML = ogb.barHtml;

  const afterBarScript = document.createElement('script');
  afterBarScript.type = 'text/javascript';
  afterBarScript.appendChild(document.createTextNode(ogb.afterBarScript));
  ogElem.parentNode.insertBefore(afterBarScript, ogElem.nextSibling);

  $('one-google-end-of-body').innerHTML = ogb.endOfBodyHtml;

  const endOfBodyScript = document.createElement('script');
  endOfBodyScript.type = 'text/javascript';
  endOfBodyScript.appendChild(document.createTextNode(ogb.endOfBodyScript));
  document.body.appendChild(endOfBodyScript);

  ntpApiHandle.logEvent(LOG_TYPE.NTP_ONE_GOOGLE_BAR_SHOWN);
}

/**
 * Injects a middle-slot promo into the page. Called asynchronously, so that it
 * doesn't block the main page load.
 */
function injectPromo(promo) {
  if (promo.promoHtml == '') {
    return;
  }

  const promoContainer = document.createElement('div');
  promoContainer.id = IDS.PROMO;
  promoContainer.innerHTML += promo.promoHtml;
  $(IDS.NTP_CONTENTS).appendChild(promoContainer);

  if (promo.promoLogUrl) {
    navigator.sendBeacon(promo.promoLogUrl);
  }

  ntpApiHandle.logEvent(LOG_TYPE.NTP_MIDDLE_SLOT_PROMO_SHOWN);

  const links = promoContainer.getElementsByTagName('a');
  if (links[0]) {
    links[0].onclick = function() {
      ntpApiHandle.logEvent(LOG_TYPE.NTP_MIDDLE_SLOT_PROMO_LINK_CLICKED);
    };
  }

  // The the MV tiles are already loaded show the promo immediately.
  if (tilesAreLoaded) {
    promoContainer.classList.add(CLASSES.SHOW_ELEMENT);
  }
}

/**
 * Injects search suggestions into the page. Called *synchronously* with cached
 * data as not to cause shifting of the most visited tiles.
 */
function injectSearchSuggestions(suggestions) {
  if (suggestions.suggestionsHtml === '') {
    return;
  }

  const suggestionsContainer = document.createElement('div');
  suggestionsContainer.id = IDS.SUGGESTIONS;
  suggestionsContainer.style.visibility = 'hidden';
  suggestionsContainer.innerHTML += suggestions.suggestionsHtml;
  $(IDS.USER_CONTENT).insertAdjacentElement('afterbegin', suggestionsContainer);

  const endOfBodyScript = document.createElement('script');
  endOfBodyScript.type = 'text/javascript';
  endOfBodyScript.appendChild(
      document.createTextNode(suggestions.suggestionsEndOfBodyScript));
  document.body.appendChild(endOfBodyScript);
}

/**
 * @param {!Event} event The click event.
 * @return {boolean} True if the click occurred in an enabled fakebox.
 */
function isFakeboxClick(event) {
  return $(IDS.FAKEBOX).contains(/** @type HTMLElement */ (event.target)) &&
      !$(IDS.FAKEBOX_MICROPHONE)
           .contains(/** @type HTMLElement */ (event.target));
}

/** @return {boolean} True if the fakebox has focus. */
function isFakeboxFocused() {
  return document.body.classList.contains(CLASSES.FAKEBOX_FOCUS) ||
      document.body.classList.contains(CLASSES.FAKEBOX_DRAG_FOCUS);
}

/** Binds event listeners. */
function listen() {
  document.addEventListener('DOMContentLoaded', init);
}

/**
 * Callback for embeddedSearch.newTabPage.onaddcustomlinkdone. Called when the
 * custom link was successfully added. Shows the "Shortcut added" notification.
 * @param {boolean} success True if the link was successfully added.
 */
function onAddCustomLinkDone(success) {
  if (success) {
    showNotification(configData.translatedStrings.linkAddedMsg);
  } else {
    showErrorNotification(
        configData.translatedStrings.linkCantCreate, null, null);
  }
  ntpApiHandle.logEvent(LOG_TYPE.NTP_CUSTOMIZE_SHORTCUT_DONE);
}

/**
 * Callback for embeddedSearch.newTabPage.ondeletecustomlinkdone. Called when
 * the custom link was successfully deleted. Shows the "Shortcut deleted"
 * notification.
 * @param {boolean} success True if the link was successfully deleted.
 */
function onDeleteCustomLinkDone(success) {
  if (success) {
    showNotification(configData.translatedStrings.linkRemovedMsg);
  } else {
    showErrorNotification(
        configData.translatedStrings.linkCantRemove, null, null);
  }
}

/**
 * Callback for embeddedSearch.newTabPage.oninputcancel. Restores the NTP
 * (re-enables the fakebox and unhides the logo.)
 */
function onInputCancel() {
  setFakeboxVisibility(true);
}

/**
 * Callback for embeddedSearch.newTabPage.oninputstart. Handles new input by
 * disposing the NTP, according to where the input was entered.
 */
function onInputStart() {
  if (isFakeboxFocused()) {
    setFakeboxFocus(false);
    setFakeboxDragFocus(false);
    setFakeboxVisibility(false);
  }
}

/**
 * Callback for embeddedSearch.newTabPage.onmostvisitedchange. Called when the
 * NTP tiles are updated.
 */
function onMostVisitedChange() {
  reloadTiles();
}

/** @param {!AutocompleteResult} result */
function onQueryAutocompleteDone(result) {
  if (result.status === AutocompleteResultStatus.SKIPPED ||
      result.input !== lastOutput.text) {
    return;  // Stale or skipped result; ignore.
  }

  const realboxMatchesEl = document.createElement('div');

  for (const [i, match] of result.matches.entries()) {
    const matchEl = document.createElement('a');
    matchEl.href = match.destinationUrl;

    let iconClass;
    if (match.isSearchType) {
      const isSearchHistory = SEARCH_HISTORY_MATCH_TYPES.includes(match.type);
      const useClock = isSearchHistory && configData.realboxUseClockIcon;
      iconClass = useClock ? CLASSES.CLOCK_ICON : CLASSES.SEARCH_ICON;
    } else {
      // TODO(crbug.com/997229): use chrome://favicon/<url> when perms allow.
      iconClass = CLASSES.URL_ICON;
    }
    const icon = document.createElement('div');
    icon.classList.add(assert(iconClass));
    matchEl.appendChild(icon);

    const contentsEls =
        renderMatchClassifications(match.contents, match.contentsClass);
    const descriptionEls = [];
    const separatorEls = [];

    if (match.description) {
      descriptionEls.push(...renderMatchClassifications(
          match.description, match.descriptionClass));
      separatorEls.push(document.createTextNode(
          configData.translatedStrings.realboxSeparator));
    }

    const layout = match.swapContentsAndDescription ?
        [descriptionEls, separatorEls, contentsEls] :
        [contentsEls, separatorEls, descriptionEls];

    for (const col of layout) {
      col.forEach(colEl => matchEl.appendChild(colEl));
    }

    realboxMatchesEl.append(matchEl);
  }

  const hasMatches = result.matches.length > 0;
  if (hasMatches) {
    realboxMatchesEl.firstElementChild.classList.add(CLASSES.SELECTED);
  }

  $(IDS.REALBOX_MATCHES).remove();
  realboxMatchesEl.id = IDS.REALBOX_MATCHES;

  const realboxWrapper = $(IDS.REALBOX_INPUT_WRAPPER);
  realboxWrapper.appendChild(realboxMatchesEl);

  realboxWrapper.classList.toggle(CLASSES.SHOW_MATCHES, hasMatches);

  if (hasMatches) {
    realboxWrapper.addEventListener('keydown', onRealboxKeyDown);

    // If the user is deleting content, don't quickly re-suggest the same
    // output.
    if (!isDeleting) {
      const first = result.matches[0];
      if (first.allowedToBeDefaultMatch && first.inlineAutocompletion) {
        updateRealboxOutput({inline: first.inlineAutocompletion});
      }
    }
  }

  autocompleteMatches = result.matches;
}

/** @param {!Event} e */
function onRealboxCopy(e) {
  const realboxEl = $(IDS.REALBOX);
  if (!realboxEl.value || realboxEl.selectionStart !== 0 ||
      realboxEl.selectionEnd !== realboxEl.value.length ||
      autocompleteMatches.length === 0) {
    // Only handle copy events when realbox has content and it's all selected.
    return;
  }

  const matchEls = Array.from($(IDS.REALBOX_MATCHES).children);
  const selected = matchEls.findIndex(match => {
    return match.classList.contains(CLASSES.SELECTED);
  });

  const selectedMatch = autocompleteMatches[selected];
  if (!selectedMatch.isSearchType) {
    e.clipboardData.setData('text/plain', selectedMatch.destinationUrl);
    e.preventDefault();
  }
}

/** @param {Event} e */
function onRealboxKeyDown(e) {
  const key = e.key;

  const realboxEl = $(IDS.REALBOX);
  if (e.target === realboxEl && lastOutput.inline) {
    const realboxValue = realboxEl.value;
    const realboxSelected = realboxValue.substring(
        realboxEl.selectionStart, realboxEl.selectionEnd);
    // If the current state matches the default text + inline autocompletion
    // and the user types the next key in the inline autocompletion, just move
    // the selection and requery autocomplete. This is required to avoid flicker
    // while setting .value and .selection{Start,End} to keep typing smooth.
    if (realboxSelected === lastOutput.inline &&
        realboxValue === lastOutput.text + lastOutput.inline &&
        lastOutput.inline[0].toLocaleLowerCase() === key.toLocaleLowerCase()) {
      updateRealboxOutput({
        inline: lastOutput.inline.substr(1),
        text: lastOutput.text + key,
      });
      window.chrome.embeddedSearch.searchBox.queryAutocomplete(lastOutput.text);
      e.preventDefault();
      return;
    }
  }

  if (!REALBOX_KEYDOWN_HANDLED_KEYS.includes(key)) {
    return;
  }

  const hasMods = e.ctrlKey || e.metaKey || e.shiftKey;
  if (hasMods && key !== 'Enter') {
    return;
  }

  const matchEls = Array.from($(IDS.REALBOX_MATCHES).children);
  const selected = matchEls.findIndex(match => {
    return match.classList.contains(CLASSES.SELECTED);
  });

  if (key === 'Enter') {
    if (matchEls[selected]) {
      // Note: dispatching a MouseEvent here instead of using e.g. .click() as
      // this forwards key modifiers. This enables Shift+Enter to open a match
      // in a new window, for example.
      matchEls[selected].dispatchEvent(new MouseEvent('click', e));
    }
    return;
  }

  e.preventDefault();

  if (key === 'Escape' && selected === 0) {
    updateRealboxOutput({inline: '', text: ''});
    return;
  }

  /** @type {number} */ let newSelected;
  if (key === 'ArrowDown') {
    newSelected = selected + 1 < matchEls.length ? selected + 1 : 0;
  } else if (key === 'ArrowUp') {
    newSelected = selected - 1 >= 0 ? selected - 1 : matchEls.length - 1;
  } else if (key === 'Escape' || key === 'PageUp') {
    newSelected = 0;
  } else if (key === 'PageDown') {
    newSelected = matchEls.length - 1;
  }
  selectMatchEl(matchEls[newSelected]);

  const newMatch = autocompleteMatches[newSelected];
  const newFill = newMatch.fillIntoEdit;
  let newInline = '';
  if (newMatch.allowedToBeDefaultMatch) {
    newInline = newMatch.inlineAutocompletion;
  }
  const newFillEnd = newFill.length - newInline.length;
  updateRealboxOutput({
    moveCursorToEnd: true,
    inline: newInline,
    text: newFill.substr(0, newFillEnd),
  });
}

function onRealboxInput() {
  updateRealboxOutput({inline: '', text: $(IDS.REALBOX).value});
  if (lastOutput.text.trim()) {
    window.chrome.embeddedSearch.searchBox.queryAutocomplete(lastOutput.text);
  }
}

/** @param {Event} e */
function onRealboxWrapperFocusIn(e) {
  if (e.target.matches(`#${IDS.REALBOX}`) && !$(IDS.REALBOX).value) {
    window.chrome.embeddedSearch.searchBox.queryAutocomplete('');
  } else if (e.target.matches(`#${IDS.REALBOX_MATCHES} a`)) {
    const selectedIndex = selectMatchEl(e.target);
    // It doesn't really make sense to use fillFromMatch() here as the focus
    // change drops the selection (and is probably just noisy to
    // screenreaders).
    const newFill = autocompleteMatches[selectedIndex].fillIntoEdit;
    updateRealboxOutput({moveCursorToEnd: true, inline: '', text: newFill});
  }
}

/** @param {Event} e */
function onRealboxWrapperFocusOut(e) {
  const relatedTarget = /** @type {Element} */ (e.relatedTarget);
  if (!$(IDS.REALBOX_INPUT_WRAPPER).contains(relatedTarget)) {
    hideRealboxMatches();  // Hide but don't clear input.
    window.chrome.embeddedSearch.searchBox.stopAutocomplete(
        /*clearResult=*/ true);
  }
}

/**
 * Handles a click on the restore all notification link by hiding the
 * notification and informing Chrome.
 */
function onRestoreAll() {
  hideNotification();
  // Focus on the omnibox after the notification is hidden.
  window.chrome.embeddedSearch.searchBox.startCapturingKeyStrokes();
  if (customLinksEnabled()) {
    ntpApiHandle.resetCustomLinks();
  } else {
    ntpApiHandle.undoAllMostVisitedDeletions();
  }
}

/**
 * Callback for embeddedSearch.newTabPage.onthemechange.
 */
function onThemeChange() {
  renderTheme();
  renderOneGoogleBarTheme();
  sendThemeInfoToMostVisitedIframe();
}

/**
 * Handles a click on the notification undo link by hiding the notification and
 * informing Chrome.
 */
function onUndo() {
  hideNotification();
  // Focus on the omnibox after the notification is hidden.
  window.chrome.embeddedSearch.searchBox.startCapturingKeyStrokes();
  if (customLinksEnabled()) {
    ntpApiHandle.undoCustomLinkAction();
  } else if (lastBlacklistedTile != null) {
    ntpApiHandle.undoMostVisitedDeletion(lastBlacklistedTile);
  }
}

/**
 * Callback for embeddedSearch.newTabPage.onupdatecustomlinkdone. Called when
 * the custom link was successfully updated. Shows the "Shortcut edited"
 * notification.
 * @param {boolean} success True if the link was successfully updated.
 */
function onUpdateCustomLinkDone(success) {
  if (success) {
    showNotification(configData.translatedStrings.linkEditedMsg);
  } else {
    showErrorNotification(
        configData.translatedStrings.linkCantEdit, null, null);
  }
}

/**
 * Called by tests to override the executable timeout with a test timeout.
 * @param {!Function} timeout The timeout function. Requires a boolean param.
 */
function overrideExecutableTimeoutForTesting(timeout) {
  createExecutableTimeout = timeout;
}

/**
 * @param {!Element} element
 * @param {!Array<string>} keys
 * @param {!function(Event)} handler
 */
function registerKeyHandler(element, keys, handler) {
  element.addEventListener('keydown', e => {
    if (keys.includes(e.key)) {
      handler(e);
    }
  });
}

/**
 * Fetches new data (RIDs) from the embeddedSearch.newTabPage API and passes
 * them to the iframe.
 */
function reloadTiles() {
  // Don't attempt to load tiles if the MV data isn't available yet - this can
  // happen occasionally, see https://crbug.com/794942. In that case, we should
  // get an onMostVisitedChange call once they are available.
  // Note that MV data being available is different from having > 0 tiles. There
  // can legitimately be 0 tiles, e.g. if the user blacklisted them all.
  if (!ntpApiHandle.mostVisitedAvailable) {
    return;
  }

  const pages = ntpApiHandle.mostVisited;
  const cmds = [];
  const maxNumTiles = customLinksEnabled() ? MAX_NUM_TILES_CUSTOM_LINKS :
                                             MAX_NUM_TILES_MOST_VISITED;
  for (let i = 0; i < Math.min(maxNumTiles, pages.length); ++i) {
    cmds.push({cmd: 'tile', rid: pages[i].rid});
  }
  cmds.push({cmd: 'show'});

  $(IDS.MOST_VISITED).hidden =
      !chrome.embeddedSearch.newTabPage.areShortcutsVisible;

  const iframe = $(IDS.TILES_IFRAME);
  if (iframe) {
    iframe.contentWindow.postMessage(cmds, '*');
  }
}

/**
 * @param {string} text
 * @param {!Array<!ACMatchClassification>} classifications
 * @return {!Array<!Element>}
 */
function renderMatchClassifications(text, classifications) {
  return classifications.map((classification, i) => {
    const classes = classificationStyleToClasses(classification.style);
    const next = classifications[i + 1] || {offset: text.length};
    const classifiedText = text.substring(classification.offset, next.offset);
    return classes.length ? spanWithClasses(classifiedText, classes) :
                            document.createTextNode(classifiedText);
  });
}

/**
 * Updates the OneGoogleBar (if it is loaded) based on the current theme.
 * TODO(crbug.com/918582): Add support for OGB dark mode.
 */
function renderOneGoogleBarTheme() {
  if (!window.gbar) {
    return;
  }
  try {
    const oneGoogleBarApi = window.gbar.a;
    const oneGoogleBarPromise = oneGoogleBarApi.bf();
    oneGoogleBarPromise.then(function(oneGoogleBar) {
      const setForegroundStyle = oneGoogleBar.pc.bind(oneGoogleBar);
      const themeInfo = getThemeBackgroundInfo();
      setForegroundStyle(themeInfo && themeInfo.isNtpBackgroundDark ? 1 : 0);
    });
  } catch (err) {
    console.log('Failed setting OneGoogleBar theme:\n' + err);
  }
}

/** Updates the NTP based on the current theme. */
function renderTheme() {
  const info = getThemeBackgroundInfo();
  if (!info) {
    return;
  }

  $(IDS.NTP_CONTENTS).classList.toggle(CLASSES.DARK, info.isNtpBackgroundDark);

  // Update dark mode styling.
  isDarkModeEnabled = window.matchMedia('(prefers-color-scheme: dark)').matches;
  document.body.classList.toggle('light-chip', !getUseDarkChips(info));

  const background = [
    convertToRGBAColor(info.backgroundColorRgba), info.imageUrl,
    info.imageTiling, info.imageHorizontalAlignment, info.imageVerticalAlignment
  ].join(' ').trim();

  // If a custom background has been selected the image will be applied to the
  // custom-background element instead of the body.
  if (!info.customBackgroundConfigured) {
    document.body.style.background = background;
  }

  // Dark mode uses a white Google logo.
  const useWhiteLogo =
      info.alternateLogo || (info.usingDefaultTheme && isDarkModeEnabled);
  document.body.classList.toggle(CLASSES.ALTERNATE_LOGO, useWhiteLogo);
  const isNonWhiteBackground = !WHITE_BACKGROUND_COLORS.includes(background);
  document.body.classList.toggle(CLASSES.NON_WHITE_BG, isNonWhiteBackground);

  if (info.logoColor) {
    document.body.style.setProperty(
        '--logo-color', convertToRGBAColor(info.logoColor));
  }

  // The doodle notifier should be shown for non-default backgrounds. This
  // includes non-white backgrounds, excluding dark mode gray if dark mode is
  // enabled.
  const isDefaultBackground = WHITE_BACKGROUND_COLORS.includes(background) ||
      (isDarkModeEnabled && background === DARK_MODE_BACKGROUND_COLOR);
  document.body.classList.toggle(CLASSES.USE_NOTIFIER, !isDefaultBackground);

  updateThemeAttribution(info.attributionUrl, info.imageHorizontalAlignment);
  setCustomThemeStyle(info);

  if (info.customBackgroundConfigured) {
    // Do anything only if the custom background changed.
    const imageUrl = assert(info.imageUrl);
    if (!$(IDS.CUSTOM_BG).style.backgroundImage.includes(imageUrl)) {
      const imageWithOverlay = [
        customize.CUSTOM_BACKGROUND_OVERLAY, 'url(' + imageUrl + ')'
      ].join(',').trim();
      // If the theme update is because of uploading a local image then we
      // should close the customization menu. Closing the menu before the image
      // is selected doesn't look good.
      const localImageFileName = 'background.jpg';
      if (!configData.richerPicker &&
          imageWithOverlay.includes(localImageFileName)) {
        customize.closeCustomizationDialog();
      }
      // |image| and |imageWithOverlay| use the same url as their source.
      // Waiting to display the custom background until |image| is fully
      // loaded ensures that |imageWithOverlay| is also loaded.
      $(IDS.CUSTOM_BG).style.backgroundImage = imageWithOverlay;
      const image = new Image();
      image.onload = function() {
        $(IDS.CUSTOM_BG).style.opacity = '1';
      };
      image.src = imageUrl;

      customize.clearAttribution();
      customize.setAttribution(
          '' + info.attribution1, '' + info.attribution2,
          '' + info.attributionActionUrl);
    }
  } else {
    $(IDS.CUSTOM_BG).style.opacity = '0';
    $(IDS.CUSTOM_BG).style.backgroundImage = '';
    customize.clearAttribution();
  }

  $(customize.IDS.RESTORE_DEFAULT)
      .classList.toggle(
          customize.CLASSES.OPTION_DISABLED, !info.customBackgroundConfigured);
  $(customize.IDS.RESTORE_DEFAULT).tabIndex =
      (info.customBackgroundConfigured ? 0 : -1);

  $(customize.IDS.EDIT_BG)
      .classList.toggle(
          CLASSES.ENTRY_POINT_ENHANCED, !info.customBackgroundConfigured);

  if (configData.isGooglePage) {
    customize.onThemeChange();
  }
}

/**
 * Request data for search suggestions, promo, and the OGB. Insert it into
 * the page once it's available. For search suggestions this should be almost
 * immediately as cached data is always used. Promos and the OGB may need
 * to wait for the asynchronous network request to complete.
 */
function requestAndInsertGoogleResources() {
  if (!$('search-suggestions-loader')) {
    const ssScript = document.createElement('script');
    ssScript.id = 'search-suggestions-loader';
    ssScript.src = 'chrome-search://local-ntp/search-suggestions.js';
    ssScript.async = false;
    document.body.appendChild(ssScript);
    ssScript.onload = function() {
      injectSearchSuggestions(searchSuggestions);
    };
  }
  if (!$('one-google-loader')) {
    // Load the OneGoogleBar script. It'll create a global variable |og| which
    // is a JSON object corresponding to the native OneGoogleBarData type.
    const ogScript = document.createElement('script');
    ogScript.id = 'one-google-loader';
    ogScript.src = 'chrome-search://local-ntp/one-google.js';
    document.body.appendChild(ogScript);
    ogScript.onload = function() {
      injectOneGoogleBar(og);
    };
  }
  if (!$('promo-loader')) {
    const promoScript = document.createElement('script');
    promoScript.id = 'promo-loader';
    promoScript.src = 'chrome-search://local-ntp/promo.js';
    document.body.appendChild(promoScript);
    promoScript.onload = function() {
      injectPromo(promo);
    };
  }
}

/**
 * @param {!EventTarget} elToSelect
 * @return {number} The selected index (if found); else -1.
 */
function selectMatchEl(elToSelect) {
  let selectedIndex = -1;
  Array.from($(IDS.REALBOX_MATCHES).children).forEach((matchEl, i) => {
    const found = matchEl === elToSelect;
    matchEl.classList.toggle(CLASSES.SELECTED, found);
    if (found) {
      selectedIndex = i;
    }
  });
  return selectedIndex;
}

/** Sends the current theme info to the most visited iframe. */
function sendThemeInfoToMostVisitedIframe() {
  const info = getThemeBackgroundInfo();
  if (!info) {
    return;
  }

  const message = {cmd: 'updateTheme'};
  message.isThemeDark = info.isNtpBackgroundDark;
  message.customBackground = info.customBackgroundConfigured;
  message.useTitleContainer = info.useTitleContainer;
  message.iconBackgroundColor = convertToRGBAColor(info.iconBackgroundColor);
  message.useWhiteAddIcon = info.useWhiteAddIcon;

  let titleColor = NTP_DESIGN.titleColor;
  if (!info.usingDefaultTheme && info.textColorRgba) {
    titleColor = info.textColorRgba;
  } else if (info.isNtpBackgroundDark) {
    titleColor = NTP_DESIGN.titleColorAgainstDark;
  }
  message.tileTitleColor = convertToRGBAColor(titleColor);

  const iframe = $(IDS.TILES_IFRAME);
  if (iframe) {
    iframe.contentWindow.postMessage(message, '*');
  }
}

/**
 * Sets the visibility of the theme attribution.
 * @param {boolean} show True to show the attribution.
 */
function setAttributionVisibility(show) {
  $(IDS.ATTRIBUTION).style.display = show ? '' : 'none';
}

/**
 * Updates the NTP style according to theme.
 * @param {Object} themeInfo The information about the theme.
 */
function setCustomThemeStyle(themeInfo) {
  let textColor = '';
  let textColorLight = '';
  let mvxFilter = '';
  if (!themeInfo.usingDefaultTheme) {
    textColor = convertToRGBAColor(themeInfo.textColorRgba);
    textColorLight = convertToRGBAColor(themeInfo.textColorLightRgba);
    mvxFilter = 'drop-shadow(0 0 0 ' + textColor + ')';
  }

  $(IDS.NTP_CONTENTS)
      .classList.toggle(CLASSES.DEFAULT_THEME, themeInfo.usingDefaultTheme);

  document.body.style.setProperty('--text-color', textColor);
  document.body.style.setProperty('--text-color-light', textColorLight);
}

/**
 * @param {boolean} focus True to show a dragging focus on the fakebox.
 */
function setFakeboxDragFocus(focus) {
  document.body.classList.toggle(CLASSES.FAKEBOX_DRAG_FOCUS, focus);
}

/**
 * @param {boolean} focus True to focus the fakebox.
 */
function setFakeboxFocus(focus) {
  document.body.classList.toggle(CLASSES.FAKEBOX_FOCUS, focus);
}

/**
 * @param {boolean} show True to show the fakebox and logo.
 */
function setFakeboxVisibility(show) {
  document.body.classList.toggle(CLASSES.HIDE_FAKEBOX, !show);
}

/**
 * Shows the error pop-up notification and triggers a delay to hide it. The
 * message will be set to |msg|. If |linkName| and |linkOnClick| are present,
 * shows an error link with text set to |linkName| and onclick handled by
 * |linkOnClick|.
 * @param {string} msg The notification message.
 * @param {?string} linkName The error link text.
 * @param {?Function} linkOnClick The error link onclick handler.
 */
function showErrorNotification(msg, linkName, linkOnClick) {
  const notification = $(IDS.ERROR_NOTIFICATION);
  $(IDS.ERROR_NOTIFICATION_MSG).textContent = msg;
  if (linkName && linkOnClick) {
    const notificationLink = $(IDS.ERROR_NOTIFICATION_LINK);
    notificationLink.textContent = linkName;
    notificationLink.onclick = linkOnClick;
    notification.classList.add(CLASSES.HAS_LINK);
  } else {
    notification.classList.remove(CLASSES.HAS_LINK);
  }
  floatUpNotification(notification, $(IDS.ERROR_NOTIFICATION_CONTAINER));
}

/**
 * Shows the Most Visited pop-up notification and triggers a delay to hide it.
 * The message will be set to |msg|.
 * @param {string} msg The notification message.
 */
function showNotification(msg) {
  $(IDS.NOTIFICATION_MESSAGE).textContent = msg;
  $(IDS.RESTORE_ALL_LINK).textContent = customLinksEnabled() ?
      configData.translatedStrings.restoreDefaultLinks :
      configData.translatedStrings.restoreThumbnailsShort;
  floatUpNotification($(IDS.NOTIFICATION), $(IDS.NOTIFICATION_CONTAINER));
  $(IDS.UNDO_LINK).focus();
}

/**
 * @param {string} text
 * @param {!Array<string>} classes
 * @return {!Element}
 */
function spanWithClasses(text, classes) {
  const span = document.createElement('span');
  span.classList.add(...classes);
  span.textContent = text;
  return span;
}

/**
 * @param {number} style
 * @return {!Array<string>}
 */
function classificationStyleToClasses(style) {
  const classes = [];
  if (style & ACMatchClassificationStyle.DIM) {
    classes.push('dim');
  }
  if (style & ACMatchClassificationStyle.MATCH) {
    classes.push('match');
  }
  if (style & ACMatchClassificationStyle.URL) {
    classes.push('url');
  }
  return classes;
}

/** @param {!RealboxOutputUpdate} update */
function updateRealboxOutput(update) {
  assert(Object.keys(update).length > 0);

  const realboxEl = $(IDS.REALBOX);
  const newOutput =
      /** @type {!RealboxOutput} */ (Object.assign({}, lastOutput, update));
  const newAll = newOutput.text + newOutput.inline;

  const inlineDiffers = newOutput.inline !== lastOutput.inline;
  const preserveSelection = !inlineDiffers && !update.moveCursorToEnd;
  let needsSelectionUpdate = !preserveSelection;

  const oldSelectionStart = realboxEl.selectionStart;

  if (newAll !== realboxEl.value) {
    realboxEl.value = newAll;
    needsSelectionUpdate = true;  // Setting .value blows away selection.
  }

  if (!newAll.trim()) {
    hideRealboxMatches();
  } else if (needsSelectionUpdate) {
    realboxEl.selectionStart =
        preserveSelection ? oldSelectionStart : newOutput.text.length;
    // If the selection shouldn't be preserved, set the selection end to the
    // same as the selection start (i.e. drop selection but move cursor).
    realboxEl.selectionEnd =
        preserveSelection ? oldSelectionStart : newAll.length;
  }

  isDeleting = userDeletedOutput(lastOutput, newOutput);
  lastOutput = newOutput;
}

/**
 * Renders the attribution if the URL is present, otherwise hides it.
 * @param {string|undefined} url The URL of the attribution image, if any.
 * @param {string|undefined} themeBackgroundAlignment The alignment of the theme
 *     background image. This is used to compute the attribution's alignment.
 */
function updateThemeAttribution(url, themeBackgroundAlignment) {
  if (!url) {
    setAttributionVisibility(false);
    return;
  }

  const attribution = $(IDS.ATTRIBUTION);
  let attributionImage = attribution.querySelector('img');
  if (!attributionImage) {
    attributionImage = new Image();
    attribution.appendChild(attributionImage);
  }
  attributionImage.style.content = url;

  // To avoid conflicts, place the attribution on the left for themes that
  // right align their background images.
  attribution.classList.toggle(
      CLASSES.LEFT_ALIGN_ATTRIBUTION, themeBackgroundAlignment == 'right');
  setAttributionVisibility(true);
}

/**
 * @param {!RealboxOutput} before
 * @param {!RealboxOutput} after
 * @return {boolean}
 */
function userDeletedOutput(before, after) {
  const beforeAll = before.text + before.inline;
  const afterAll = after.text + after.inline;
  return beforeAll.length > afterAll.length && beforeAll.startsWith(afterAll);
}

return {
  init: init,  // Exposed for testing.
  listen: listen,
  disableIframesAndVoiceSearchForTesting:
      disableIframesAndVoiceSearchForTesting,
  overrideExecutableTimeoutForTesting: overrideExecutableTimeoutForTesting
};
}

if (!window.localNTPUnitTest) {
  LocalNTP().listen();
}
