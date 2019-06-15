// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// TODO(crbug.com/937570): After the RP launches this should be renamed to
// customizationMenu along with the file, and large parts can be
// refactored/removed.
const customize = {};

/**
 * The browser embeddedSearch.newTabPage object.
 * @type {Object}
 */
let ntpApiHandle;

/**
 * The different types of events that are logged from the NTP. This enum is
 * used to transfer information from the NTP JavaScript to the renderer and is
 * not used as a UMA enum histogram's logged value.
 * Note: Keep in sync with common/ntp_logging_events.h
 * @enum {number}
 * @const
 */
customize.BACKGROUND_CUSTOMIZATION_LOG_TYPE = {
  // The 'Chrome backgrounds' menu item was clicked.
  NTP_CUSTOMIZE_CHROME_BACKGROUNDS_CLICKED: 40,
  // The 'Upload an image' menu item was clicked.
  NTP_CUSTOMIZE_LOCAL_IMAGE_CLICKED: 41,
  // The 'Restore default background' menu item was clicked.
  NTP_CUSTOMIZE_RESTORE_BACKGROUND_CLICKED: 42,
  // The attribution link on a customized background image was clicked.
  NTP_CUSTOMIZE_ATTRIBUTION_CLICKED: 43,
  // The 'Restore default shortcuts' menu item was clicked.
  NTP_CUSTOMIZE_RESTORE_SHORTCUTS_CLICKED: 46,
  // A collection was selected in the 'Chrome backgrounds' dialog.
  NTP_CUSTOMIZE_CHROME_BACKGROUND_SELECT_COLLECTION: 47,
  // An image was selected in the 'Chrome backgrounds' dialog.
  NTP_CUSTOMIZE_CHROME_BACKGROUND_SELECT_IMAGE: 48,
  // 'Cancel' was clicked in the 'Chrome backgrounds' dialog.
  NTP_CUSTOMIZE_CHROME_BACKGROUND_CANCEL: 49,
  // NOTE: NTP_CUSTOMIZE_CHROME_BACKGROUND_DONE (50) is logged on the backend
  // when the selected image is saved.
  // 'Cancel' was clicked in the 'Upload an image' dialog.
  NTP_CUSTOMIZE_LOCAL_IMAGE_CANCEL: 51,
  // 'Done' was clicked in the 'Upload an image' dialog.
  NTP_CUSTOMIZE_LOCAL_IMAGE_DONE: 52,
};

/**
 * Enum for key codes.
 * @enum {number}
 * @const
 */
customize.KEYCODES = {
  BACKSPACE: 8,
  DOWN: 40,
  ENTER: 13,
  ESC: 27,
  LEFT: 37,
  RIGHT: 39,
  SPACE: 32,
  TAB: 9,
  UP: 38,
};

/**
 * Array for keycodes corresponding to arrow keys.
 * @type Array
 * @const
 */
customize.arrowKeys = [/*Left*/ 37, /*Up*/ 38, /*Right*/ 39, /*Down*/ 40];

/**
 * Enum for HTML element ids.
 * @enum {string}
 * @const
 */
customize.IDS = {
  ATTR1: 'attr1',
  ATTR2: 'attr2',
  ATTRIBUTIONS: 'custom-bg-attr',
  BACK_CIRCLE: 'bg-sel-back-circle',
  BACKGROUNDS_DEFAULT: 'backgrounds-default',
  BACKGROUNDS_DEFAULT_ICON: 'backgrounds-default-icon',
  BACKGROUNDS_BUTTON: 'backgrounds-button',
  BACKGROUNDS_IMAGE_MENU: 'backgrounds-image-menu',
  BACKGROUNDS_MENU: 'backgrounds-menu',
  BACKGROUNDS_UPLOAD: 'backgrounds-upload',
  BACKGROUNDS_UPLOAD_WRAPPER: 'backgrounds-upload-wrapper',
  CANCEL: 'bg-sel-footer-cancel',
  COLORS_BUTTON: 'colors-button',
  COLORS_DEFAULT: 'colors-default',
  COLORS_MENU: 'colors-menu',
  CUSTOMIZATION_MENU: 'customization-menu',
  CUSTOM_BG: 'custom-bg',
  CUSTOM_LINKS_RESTORE_DEFAULT: 'custom-links-restore-default',
  CUSTOM_LINKS_RESTORE_DEFAULT_TEXT: 'custom-links-restore-default-text',
  DEFAULT_WALLPAPERS: 'edit-bg-default-wallpapers',
  DEFAULT_WALLPAPERS_TEXT: 'edit-bg-default-wallpapers-text',
  DONE: 'bg-sel-footer-done',
  EDIT_BG: 'edit-bg',
  EDIT_BG_DIALOG: 'edit-bg-dialog',
  EDIT_BG_DIVIDER: 'edit-bg-divider',
  EDIT_BG_ICON: 'edit-bg-icon',
  EDIT_BG_MENU: 'edit-bg-menu',
  EDIT_BG_TEXT: 'edit-bg-text',
  MENU_BACK_CIRCLE: 'menu-back-circle',
  MENU_CANCEL: 'menu-cancel',
  MENU_DONE: 'menu-done',
  MENU_TITLE: 'menu-title',
  LINK_ICON: 'link-icon',
  MENU: 'bg-sel-menu',
  OPTIONS_TITLE: 'edit-bg-title',
  RESTORE_DEFAULT: 'edit-bg-restore-default',
  RESTORE_DEFAULT_TEXT: 'edit-bg-restore-default-text',
  SHORTCUTS_BUTTON: 'shortcuts-button',
  SHORTCUTS_MENU: 'shortcuts-menu',
  SHORTCUTS_OPTION_CUSTOM_LINKS: 'sh-option-cl',
  SHORTCUTS_OPTION_MOST_VISITED: 'sh-option-mv',
  UPLOAD_IMAGE: 'edit-bg-upload-image',
  UPLOAD_IMAGE_TEXT: 'edit-bg-upload-image-text',
  TILES: 'bg-sel-tiles',
  TITLE: 'bg-sel-title',
};

/**
 * Enum for classnames.
 * @enum {string}
 * @const
 */
customize.CLASSES = {
  ATTR_SMALL: 'attr-small',
  ATTR_COMMON: 'attr-common',
  ATTR_LINK: 'attr-link',
  COLLECTION_DIALOG: 'is-col-sel',
  COLLECTION_SELECTED: 'bg-selected',  // Highlight selected tile
  COLLECTION_TILE: 'bg-sel-tile',  // Preview tile for background customization
  COLLECTION_TILE_BG: 'bg-sel-tile-bg',
  COLLECTION_TITLE: 'bg-sel-tile-title',  // Title of a background image
  // Extended and elevated style for entry point.
  ENTRY_POINT_ENHANCED: 'ep-enhanced',
  IMAGE_DIALOG: 'is-img-sel',
  ON_IMAGE_MENU: 'on-img-menu',
  OPTION: 'bg-option',
  OPTION_DISABLED: 'bg-option-disabled',  // The menu option is disabled.
  MENU_SHOWN: 'menu-shown',
  MOUSE_NAV: 'using-mouse-nav',
  SELECTED: 'selected',
  SELECTED_BORDER: 'selected-border',
  SELECTED_CHECK: 'selected-check',
  SELECTED_CIRCLE: 'selected-circle',
  SINGLE_ATTR: 'single-attr'
};

/**
 * Enum for background option menu entries, in the order they appear in the UI.
 * @enum {number}
 * @const
 */
customize.MENU_ENTRIES = {
  CHROME_BACKGROUNDS: 0,
  UPLOAD_IMAGE: 1,
  CUSTOM_LINKS_RESTORE_DEFAULT: 2,
  RESTORE_DEFAULT: 3,
};

customize.CUSTOM_BACKGROUND_OVERLAY =
    'linear-gradient(rgba(0, 0, 0, 0), rgba(0, 0, 0, 0.3))';

// These shound match the corresponding values in local_ntp.js, that control the
// mv-notice element.
customize.delayedHideNotification = -1;
customize.NOTIFICATION_TIMEOUT = 10000;

/**
 * Were the background tiles already created.
 * @type {boolean}
 */
customize.builtTiles = false;

/**
 * Tile that was selected by the user.
 * @type {?Element}
 */
customize.selectedTile = null;

/**
 * Number of rows in the custom background dialog to preload.
 * @type {number}
 * @const
 */
customize.ROWS_TO_PRELOAD = 3;

/**
 * Called when the error notification should be shown.
 * @type {?Function}
 * @private
 */
customize.showErrorNotification = null;

/**
 * Called when the custom link notification should be hidden.
 * @type {?Function}
 * @private
 */
customize.hideCustomLinkNotification = null;

/**
 * The currently selected option in the richer picker.
 * @type {?Element}
 * @private
 */
customize.richerPicker_selectedOption = null;

/**
 * The currently selected option in the Colors menu.
 * @type {?Element}
 */
customize.selectedColorTile = null;

/**
 * Whether tiles for Colors menu already loaded.
 * @type {boolean}
 */
customize.colorMenuLoaded = false;

/**
 * The original NTP background. Used to restore from image previews.
 * @type {string}
 */
customize.originalBackground = '';

/**
 * Sets the visibility of the settings menu and individual options depending on
 * their respective features.
 */
customize.setMenuVisibility = function() {
  // Reset all hidden values.
  $(customize.IDS.EDIT_BG).hidden = false;
  $(customize.IDS.DEFAULT_WALLPAPERS).hidden = false;
  $(customize.IDS.UPLOAD_IMAGE).hidden = false;
  $(customize.IDS.RESTORE_DEFAULT).hidden = false;
  $(customize.IDS.EDIT_BG_DIVIDER).hidden = false;
  $(customize.IDS.CUSTOM_LINKS_RESTORE_DEFAULT).hidden =
      configData.hideShortcuts;
  $(customize.IDS.COLORS_BUTTON).hidden = !configData.chromeColors;
};

/**
 * Display custom background image attributions on the page.
 * @param {string} attributionLine1 First line of attribution.
 * @param {string} attributionLine2 Second line of attribution.
 * @param {string} attributionActionUrl Url to learn more about the image.
 */
customize.setAttribution = function(
    attributionLine1, attributionLine2, attributionActionUrl) {
  const attributionBox = $(customize.IDS.ATTRIBUTIONS);
  const attr1 = document.createElement('span');
  attr1.id = customize.IDS.ATTR1;
  const attr2 = document.createElement('span');
  attr2.id = customize.IDS.ATTR2;

  if (attributionLine1 !== '') {
    // Shouldn't be changed from textContent for security assurances.
    attr1.textContent = attributionLine1;
    attr1.classList.add(customize.CLASSES.ATTR_COMMON);
    $(customize.IDS.ATTRIBUTIONS).appendChild(attr1);
  }
  if (attributionLine2 !== '') {
    // Shouldn't be changed from textContent for security assurances.
    attr2.textContent = attributionLine2;
    attr2.classList.add(customize.CLASSES.ATTR_SMALL);
    attr2.classList.add(customize.CLASSES.ATTR_COMMON);
    attributionBox.appendChild(attr2);
  }
  if (attributionActionUrl !== '') {
    const attr = (attributionLine2 !== '' ? attr2 : attr1);
    attr.classList.add(customize.CLASSES.ATTR_LINK);

    const linkIcon = document.createElement('div');
    linkIcon.id = customize.IDS.LINK_ICON;
    // Enlarge link-icon when there is only one line of attribution
    if (attributionLine2 === '') {
      linkIcon.classList.add(customize.CLASSES.SINGLE_ATTR);
    }
    attr.insertBefore(linkIcon, attr.firstChild);

    attributionBox.classList.add(customize.CLASSES.ATTR_LINK);
    attributionBox.href = attributionActionUrl;
    attributionBox.onclick = function() {
      ntpApiHandle.logEvent(customize.BACKGROUND_CUSTOMIZATION_LOG_TYPE
                                .NTP_CUSTOMIZE_ATTRIBUTION_CLICKED);
    };
    attributionBox.style.cursor = 'pointer';
  }
};

customize.clearAttribution = function() {
  const attributions = $(customize.IDS.ATTRIBUTIONS);
  attributions.removeAttribute('href');
  attributions.className = '';
  attributions.style.cursor = 'none';
  while (attributions.firstChild) {
    attributions.removeChild(attributions.firstChild);
  }
};

customize.unselectTile = function() {
  $(customize.IDS.DONE).disabled = true;
  customize.selectedTile = null;
  $(customize.IDS.DONE).tabIndex = -1;
};

/**
 * Remove all collection tiles from the container when the dialog
 * is closed.
 */
customize.resetSelectionDialog = function() {
  $(customize.IDS.TILES).scrollTop = 0;
  const tileContainer = $(customize.IDS.TILES);
  while (tileContainer.firstChild) {
    tileContainer.removeChild(tileContainer.firstChild);
  }
  customize.unselectTile();
};

/**
 * Apply selected styling to |button| and make corresponding |menu| visible.
 * @param {?Element} button The button element to apply styling to.
 * @param {?Element} menu The menu element to apply styling to.
 */
customize.richerPicker_selectMenuOption = function(button, menu) {
  if (!button || !menu) {
    return;
  }
  button.classList.toggle(customize.CLASSES.SELECTED, true);
  customize.richerPicker_selectedOption = button;
  menu.classList.toggle(customize.CLASSES.MENU_SHOWN, true);
};

/**
 * Remove image tiles and maybe swap back to main background menu.
 * @param {boolean} showMenu Whether the main background menu should be shown.
 */
customize.richerPicker_resetImageMenu = function(showMenu) {
  const backgroundMenu = $(customize.IDS.BACKGROUNDS_MENU);
  const imageMenu = $(customize.IDS.BACKGROUNDS_IMAGE_MENU);
  const menu = $(customize.IDS.CUSTOMIZATION_MENU);
  const menuTitle = $(customize.IDS.MENU_TITLE);

  imageMenu.innerHTML = '';
  imageMenu.classList.toggle(customize.CLASSES.MENU_SHOWN, false);
  menuTitle.textContent = menuTitle.dataset.mainTitle;
  menu.classList.toggle(customize.CLASSES.ON_IMAGE_MENU, false);
  backgroundMenu.classList.toggle(customize.CLASSES.MENU_SHOWN, showMenu);
  backgroundMenu.scrollTop = 0;

  // Reset done button state.
  $(customize.IDS.MENU_DONE).disabled = true;
  customize.richerPicker_deselectTile(customize.selectedTile);
  customize.selectedTile = null;
  $(customize.IDS.MENU_DONE).tabIndex = -1;
};

/**
 * Close the collection selection dialog and cleanup the state
 * @param {?Element} menu The dialog to be closed
 */
customize.closeCollectionDialog = function(menu) {
  if (!menu) {
    return;
  }
  menu.close();
  customize.resetSelectionDialog();
};

/**
 * Close and reset the dialog, and set the background.
 * @param {string} url The url of the selected background.
 */
customize.setBackground = function(
    url, attributionLine1, attributionLine2, attributionActionUrl) {
  if (configData.richerPicker) {
    customize.richerPicker_closeCustomizationMenu();
  } else {
    customize.closeCollectionDialog($(customize.IDS.MENU));
  }
  window.chrome.embeddedSearch.newTabPage.setBackgroundURLWithAttributions(
      url, attributionLine1, attributionLine2, attributionActionUrl);
};

/**
 * Creates a tile for the customization menu with a title.
 * @param {string} id The id for the new element.
 * @param {string} imageUrl The background image url for the new element.
 * @param {string} name The name for the title of the new element.
 * @param {Object} dataset The dataset for the new element.
 * @param {?Function} onClickInteraction Function for onclick interaction.
 * @param {?Function} onKeyInteraction Function for onkeydown interaction.
 */
customize.createTileWithTitle = function(
    id, imageUrl, name, dataset, onClickInteraction, onKeyInteraction) {
  const tile = customize.createTile(
      id, imageUrl, dataset, onClickInteraction, onKeyInteraction);
  customize.fadeInImageTile(tile, imageUrl, null);

  const title = document.createElement('div');
  title.classList.add(customize.CLASSES.COLLECTION_TITLE);
  title.textContent = name;
  tile.appendChild(title);

  const tileBackground = document.createElement('div');
  tileBackground.classList.add(customize.CLASSES.COLLECTION_TILE_BG);
  tileBackground.appendChild(tile);
  return tileBackground;
};

/**
 * Create a tile for customization menu.
 * @param {string} id The id for the new element.
 * @param {string} imageUrl The background image url for the new element.
 * @param {Object} dataset The dataset for the new element.
 * @param {?Function} onClickInteraction Function for onclick interaction.
 * @param {?Function} onKeyInteraction Function for onkeydown interaction.
 */
customize.createTile = function(
    id, imageUrl, dataset, onClickInteraction, onKeyInteraction) {
  const tile = document.createElement('div');
  tile.id = id;
  tile.classList.add(customize.CLASSES.COLLECTION_TILE);
  tile.style.backgroundImage = 'url(' + imageUrl + ')';
  for (const key in dataset) {
    tile.dataset[key] = dataset[key];
  }
  tile.tabIndex = -1;

  // Accessibility support for screen readers.
  tile.setAttribute('role', 'button');

  tile.onclick = onClickInteraction;
  tile.onkeydown = onKeyInteraction;
  return tile;
};

/**
 * Get the number of tiles in a row according to current window width.
 * @return {number} the number of tiles per row
 */
customize.getTilesWide = function() {
  // Browser window can only fit two columns. Should match "#bg-sel-menu" width.
  if ($(customize.IDS.MENU).offsetWidth < 517) {
    return 2;
  } else if ($(customize.IDS.MENU).offsetWidth < 356) {
    // Browser window can only fit one column. Should match @media (max-width:
    // 356) "#bg-sel-menu" width.
    return 1;
  }

  return 3;
};

/**
 * @param {number} deltaX Change in the x direction.
 * @param {number} deltaY Change in the y direction.
 * @param {Element} current The current tile.
 */
customize.richerPicker_getNextTile = function(deltaX, deltaY, current) {
  const menu = $(customize.IDS.CUSTOMIZATION_MENU);
  const container = menu.classList.contains(customize.CLASSES.ON_IMAGE_MENU) ?
      $(customize.IDS.BACKGROUNDS_IMAGE_MENU) :
      $(customize.IDS.BACKGROUNDS_MENU);
  const tiles = Array.from(
      container.getElementsByClassName(customize.CLASSES.COLLECTION_TILE));
  const curIndex = tiles.indexOf(current);
  if (deltaX != 0) {
    return tiles[curIndex + deltaX];
  } else if (deltaY != 0) {
    let nextIndex = tiles.indexOf(current);
    const startingTop = current.getBoundingClientRect().top;
    const startingLeft = current.getBoundingClientRect().left;

    // Search until a tile in a different row and the same column is found.
    while (tiles[nextIndex] &&
           (tiles[nextIndex].getBoundingClientRect().top == startingTop ||
            tiles[nextIndex].getBoundingClientRect().left != startingLeft)) {
      nextIndex += deltaY;
    }
    return tiles[nextIndex];
  }
  return null;
};

/**
 * Get the next tile when the arrow keys are used to navigate the grid.
 * Returns null if the tile doesn't exist.
 * @param {number} deltaX Change in the x direction.
 * @param {number} deltaY Change in the y direction.
 * @param {Element} currentElem The current tile.
 */
customize.getNextTile = function(deltaX, deltaY, currentElem) {
  if (configData.richerPicker) {
    return customize.richerPicker_getNextTile(deltaX, deltaY, currentElem);
  }
  const current = currentElem.dataset.tileNum;
  let idPrefix = 'coll_tile_';
  if ($(customize.IDS.MENU)
          .classList.contains(customize.CLASSES.IMAGE_DIALOG)) {
    idPrefix = 'img_tile_';
  }

  if (deltaX != 0) {
    const target = parseInt(current, /*radix=*/ 10) + deltaX;
    return $(idPrefix + target);
  } else if (deltaY != 0) {
    let target = parseInt(current, /*radix=*/ 10);
    let nextTile = $(idPrefix + target);
    const startingTop = nextTile.getBoundingClientRect().top;
    const startingLeft = nextTile.getBoundingClientRect().left;

    // Search until a tile in a different row and the same column is found.
    while (nextTile &&
           (nextTile.getBoundingClientRect().top == startingTop ||
            nextTile.getBoundingClientRect().left != startingLeft)) {
      target += deltaY;
      nextTile = $(idPrefix + target);
    }
    return nextTile;
  }
};

/**
 * Show dialog for selecting a Chrome background.
 */
customize.showCollectionSelectionDialog = function() {
  const tileContainer = configData.richerPicker ?
      $(customize.IDS.BACKGROUNDS_MENU) :
      $(customize.IDS.TILES);
  if (configData.richerPicker && customize.builtTiles) {
    return;
  }
  customize.builtTiles = true;
  const menu = configData.richerPicker ? $(customize.IDS.CUSTOMIZATION_MENU) :
                                         $(customize.IDS.MENU);
  if (!menu.open) {
    menu.showModal();
  }

  // Create dialog header.
  $(customize.IDS.TITLE).textContent =
      configData.translatedStrings.selectChromeWallpaper;
  if (!configData.richerPicker) {
    menu.classList.toggle(customize.CLASSES.COLLECTION_DIALOG);
    menu.classList.remove(customize.CLASSES.IMAGE_DIALOG);
  }

  const tileOnClickInteraction = function(event) {
    let tile = event.target;
    if (tile.classList.contains(customize.CLASSES.COLLECTION_TITLE)) {
      tile = tile.parentNode;
    }

    // Load images for selected collection.
    const imgElement = $('ntp-images-loader');
    if (imgElement) {
      imgElement.parentNode.removeChild(imgElement);
    }
    const imgScript = document.createElement('script');
    imgScript.id = 'ntp-images-loader';
    imgScript.src = 'chrome-search://local-ntp/ntp-background-images.js?' +
        'collection_id=' + tile.dataset.id;
    ntpApiHandle.logEvent(
        customize.BACKGROUND_CUSTOMIZATION_LOG_TYPE
            .NTP_CUSTOMIZE_CHROME_BACKGROUND_SELECT_COLLECTION);

    document.body.appendChild(imgScript);

    imgScript.onload = function() {
      // Verify that the individual image data was successfully loaded.
      const imageDataLoaded =
          (collImg.length > 0 && collImg[0].collectionId == tile.dataset.id);

      // Dependent upon the success of the load, populate the image selection
      // dialog or close the current dialog.
      if (imageDataLoaded) {
        $(customize.IDS.BACKGROUNDS_MENU)
            .classList.toggle(customize.CLASSES.MENU_SHOWN, false);
        $(customize.IDS.BACKGROUNDS_IMAGE_MENU)
            .classList.toggle(customize.CLASSES.MENU_SHOWN, true);

        // In the RP the upload or default tile may be selected.
        if (configData.richerPicker) {
          customize.richerPicker_deselectTile(customize.selectedTile);
        } else {
          customize.resetSelectionDialog();
        }
        customize.showImageSelectionDialog(tile.dataset.name);
      } else {
        customize.handleError(collImgErrors);
      }
    };
  };

  const tileOnKeyDownInteraction = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER) {
      event.preventDefault();
      event.stopPropagation();
      if (event.currentTarget.onClickOverride) {
        event.currentTarget.onClickOverride(event);
        return;
      }
      tileOnClickInteraction(event);
    } else if (customize.arrowKeys.includes(event.keyCode)) {
      // Handle arrow key navigation.
      event.preventDefault();
      event.stopPropagation();

      let target = null;
      if (event.keyCode === customize.KEYCODES.LEFT) {
        target = customize.getNextTile(
            document.documentElement.classList.contains('rtl') ? 1 : -1, 0,
            event.currentTarget);
      } else if (event.keyCode === customize.KEYCODES.UP) {
        target = customize.getNextTile(0, -1, event.currentTarget);
      } else if (event.keyCode === customize.KEYCODES.RIGHT) {
        target = customize.getNextTile(
            document.documentElement.classList.contains('rtl') ? -1 : 1, 0,
            event.currentTarget);
      } else if (event.keyCode === customize.KEYCODES.DOWN) {
        target = customize.getNextTile(0, 1, event.currentTarget);
      }
      if (target) {
        target.focus();
      } else {
        event.currentTarget.focus();
      }
    }
  };

  // Create dialog tiles.
  for (let i = 0; i < coll.length; ++i) {
    const id = coll[i].collectionId;
    const name = coll[i].collectionName;
    const imageUrl = coll[i].previewImageUrl;
    const dataset = {'id': id, 'name': name, 'tileNum': i};

    const tile = customize.createTileWithTitle(
        'coll_tile_' + i, imageUrl, name, dataset, tileOnClickInteraction,
        tileOnKeyDownInteraction);
    tileContainer.appendChild(tile);
  }

  // Attach event listeners for upload and default tiles
  $(customize.IDS.BACKGROUNDS_UPLOAD_WRAPPER).onkeydown =
      tileOnKeyDownInteraction;
  $(customize.IDS.BACKGROUNDS_DEFAULT_ICON).onkeydown =
      tileOnKeyDownInteraction;
  $(customize.IDS.BACKGROUNDS_UPLOAD_WRAPPER).onClickOverride =
      $(customize.IDS.BACKGROUNDS_UPLOAD).onkeydown;
  $(customize.IDS.BACKGROUNDS_DEFAULT_ICON).onClickOverride =
      $(customize.IDS.BACKGROUNDS_DEFAULT).onkeydown;

  $(customize.IDS.TILES).focus();
};

/**
 * Preview an image as a custom backgrounds.
 * @param {!Element} tile The tile that was selected.
 */
customize.richerPicker_previewImage = function(tile) {
  customize.originalBackground =
      $(customize.IDS.CUSTOM_BG).style.backgroundImage;

  // TODO(crbug/971853): add browertests for previews.
  // Set preview images at 720p by replacing the params in the url.
  const re = /w\d+\-h\d+/;
  $(customize.IDS.CUSTOM_BG).style.backgroundImage =
      tile.style.backgroundImage.replace(re, 'w1280-h720');
};

/**
 * Remove a preview image of a custom backgrounds.
 * @param {!Element} tile The tile that was deselected.
 */
customize.richerPicker_unpreviewImage = function(tile) {
  $(customize.IDS.CUSTOM_BG).style.backgroundImage =
      customize.originalBackground;
};

/**
 * Apply styling to a selected tile in the richer picker and enable the
 * done button.
 * @param {?Element} tile The tile to apply styling to.
 */
customize.richerPicker_selectTile = function(tile) {
  if (!tile) {
    return;
  }
  tile.parentElement.classList.toggle(customize.CLASSES.SELECTED, true);
  $(customize.IDS.MENU_DONE).disabled = false;
  customize.selectedTile = tile;
  $(customize.IDS.MENU_DONE).tabIndex = 0;

  // Create and append selected check.
  const selectedCircle = document.createElement('div');
  const selectedCheck = document.createElement('div');
  selectedCircle.classList.add(customize.CLASSES.SELECTED_CIRCLE);
  selectedCheck.classList.add(customize.CLASSES.SELECTED_CHECK);
  tile.appendChild(selectedCircle);
  tile.appendChild(selectedCheck);

  customize.richerPicker_previewImage(tile);
};

/**
 * Remove styling from a selected tile in the richer picker and disable the
 * done button.
 * @param {?Element} tile The tile to remove styling from.
 */
customize.richerPicker_deselectTile = function(tile) {
  if (!tile) {
    return;
  }
  tile.parentElement.classList.toggle(customize.CLASSES.SELECTED, false);
  $(customize.IDS.MENU_DONE).disabled = true;
  customize.selectedTile = null;
  $(customize.IDS.MENU_DONE).tabIndex = -1;

  // Remove selected check and circle.
  for (let i = 0; i < tile.children.length; ++i) {
    if (tile.children[i].classList.contains(customize.CLASSES.SELECTED_CHECK) ||
        tile.children[i].classList.contains(
            customize.CLASSES.SELECTED_CIRCLE)) {
      tile.removeChild(tile.children[i]);
      --i;
    }
  }

  customize.richerPicker_unpreviewImage(tile);
};

/**
 * Apply styling to a selected shortcut option in the richer picker and enable
 * the done button.
 * @param {?Element} option The option to apply styling to.
 */
customize.richerPicker_selectShortcutOption = function(option) {
  if (!option || customize.selectedTile === option) {
    return;  // The option has already been selected.
  }
  // Clear the previous selection, if any.
  if (customize.selectedTile) {
    customize.richerPicker_deselectTile(customize.selectedTile);
  }
  customize.richerPicker_selectTile(option);
};

/**
 * Apply border and checkmark when a tile is selected
 * @param {!Element} tile The tile to apply styling to.
 */
customize.applySelectedState = function(tile) {
  tile.classList.add(customize.CLASSES.COLLECTION_SELECTED);
  const selectedBorder = document.createElement('div');
  const selectedCircle = document.createElement('div');
  const selectedCheck = document.createElement('div');
  selectedBorder.classList.add(customize.CLASSES.SELECTED_BORDER);
  selectedCircle.classList.add(customize.CLASSES.SELECTED_CIRCLE);
  selectedCheck.classList.add(customize.CLASSES.SELECTED_CHECK);
  selectedBorder.appendChild(selectedCircle);
  selectedBorder.appendChild(selectedCheck);
  tile.appendChild(selectedBorder);
  tile.dataset.oldLabel = tile.getAttribute('aria-label');
  tile.setAttribute(
      'aria-label',
      tile.dataset.oldLabel + ' ' + configData.translatedStrings.selectedLabel);
};

/**
 * Remove border and checkmark when a tile is un-selected
 * @param {!Element} tile The tile to remove styling from.
 */
customize.removeSelectedState = function(tile) {
  tile.classList.remove(customize.CLASSES.COLLECTION_SELECTED);
  tile.removeChild(tile.firstChild);
  tile.setAttribute('aria-label', tile.dataset.oldLabel);
};

/**
 * Show dialog for selecting an image. Image data should previous have been
 * loaded into collImg via
 * chrome-search://local-ntp/ntp-background-images.js?collection_id=<collection_id>
 * @param {string} dialogTitle The title to be displayed at the top of the
 *     dialog.
 */
customize.showImageSelectionDialog = function(dialogTitle) {
  const firstNTile = customize.ROWS_TO_PRELOAD * customize.getTilesWide();
  const tileContainer = configData.richerPicker ?
      $(customize.IDS.BACKGROUNDS_IMAGE_MENU) :
      $(customize.IDS.TILES);
  const menu = configData.richerPicker ? $(customize.IDS.CUSTOMIZATION_MENU) :
                                         $(customize.IDS.MENU);

  if (configData.richerPicker) {
    $(customize.IDS.MENU_TITLE).textContent = dialogTitle;
    menu.classList.toggle(customize.CLASSES.ON_IMAGE_MENU, true);
  } else {
    $(customize.IDS.TITLE).textContent = dialogTitle;
    menu.classList.remove(customize.CLASSES.COLLECTION_DIALOG);
    menu.classList.add(customize.CLASSES.IMAGE_DIALOG);
  }

  const tileInteraction = function(tile) {
    if (customize.selectedTile) {
      if (configData.richerPicker) {
        const id = customize.selectedTile.id;
        customize.richerPicker_deselectTile(customize.selectedTile);
        if (id === tile.id) {
          return;
        }
      } else {
        customize.removeSelectedState(customize.selectedTile);
        if (customize.selectedTile.id === tile.id) {
          customize.unselectTile();
          return;
        }
      }
    }

    if (configData.richerPicker) {
      customize.richerPicker_selectTile(tile);
    } else {
      customize.applySelectedState(tile);
      customize.selectedTile = tile;
    }

    $(customize.IDS.DONE).tabIndex = 0;

    // Turn toggle off when an image is selected.
    $(customize.IDS.DONE).disabled = false;
    ntpApiHandle.logEvent(customize.BACKGROUND_CUSTOMIZATION_LOG_TYPE
                              .NTP_CUSTOMIZE_CHROME_BACKGROUND_SELECT_IMAGE);
  };

  const tileOnClickInteraction = function(event) {
    const clickCount = event.detail;
    // Control + option + space will fire the onclick event with 0 clickCount.
    if (clickCount <= 1) {
      tileInteraction(event.currentTarget);
    } else if (
        clickCount === 2 && customize.selectedTile === event.currentTarget) {
      customize.setBackground(
          event.currentTarget.dataset.url,
          event.currentTarget.dataset.attributionLine1,
          event.currentTarget.dataset.attributionLine2,
          event.currentTarget.dataset.attributionActionUrl);
    }
  };

  const tileOnKeyDownInteraction = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER) {
      event.preventDefault();
      event.stopPropagation();
      tileInteraction(event.currentTarget);
    } else if (customize.arrowKeys.includes(event.keyCode)) {
      // Handle arrow key navigation.
      event.preventDefault();
      event.stopPropagation();

      let target = null;
      if (event.keyCode == customize.KEYCODES.LEFT) {
        target = customize.getNextTile(
            document.documentElement.classList.contains('rtl') ? 1 : -1, 0,
            event.currentTarget);
      } else if (event.keyCode == customize.KEYCODES.UP) {
        target = customize.getNextTile(0, -1, event.currentTarget);
      } else if (event.keyCode == customize.KEYCODES.RIGHT) {
        target = customize.getNextTile(
            document.documentElement.classList.contains('rtl') ? -1 : 1, 0,
            event.currentTarget);
      } else if (event.keyCode == customize.KEYCODES.DOWN) {
        target = customize.getNextTile(0, 1, event.currentTarget);
      }
      if (target) {
        target.focus();
      } else {
        event.currentTarget.focus();
      }
    }
  };

  const preLoadTiles = [];
  const postLoadTiles = [];

  for (let i = 0; i < collImg.length; ++i) {
    const dataset = {};

    // TODO(crbug.com/854028): Remove this hardcoded check when wallpaper
    // previews are supported.
    if (collImg[i].collectionId === 'solidcolors') {
      dataset.attributionLine1 = '';
      dataset.attributionLine2 = '';
      dataset.attributionActionUrl = '';
    } else {
      dataset.attributionLine1 =
          (collImg[i].attributions[0] !== undefined ?
               collImg[i].attributions[0] :
               '');
      dataset.attributionLine2 =
          (collImg[i].attributions[1] !== undefined ?
               collImg[i].attributions[1] :
               '');
      dataset.attributionActionUrl = collImg[i].attributionActionUrl;
    }
    dataset.url = collImg[i].imageUrl;
    dataset.tileNum = i;

    const tile = customize.createTile(
        'img_tile_' + i, collImg[i].imageUrl, dataset, tileOnClickInteraction,
        tileOnKeyDownInteraction);

    tile.setAttribute('aria-label', collImg[i].attributions[0]);

    // Load the first |ROWS_TO_PRELOAD| rows of tiles.
    if (i < firstNTile) {
      preLoadTiles.push(tile);
    } else {
      postLoadTiles.push(tile);
    }

    const tileBackground = document.createElement('div');
    tileBackground.classList.add(customize.CLASSES.COLLECTION_TILE_BG);
    tileBackground.appendChild(tile);
    tileContainer.appendChild(tileBackground);
  }
  let tileGetsLoaded = 0;
  for (const tile of preLoadTiles) {
    customize.loadTile(tile, collImg, () => {
      // After the preloaded tiles finish loading, the rest of the tiles start
      // loading.
      if (++tileGetsLoaded === preLoadTiles.length) {
        postLoadTiles.forEach(
            (tile) => customize.loadTile(tile, collImg, null));
      }
    });
  }

  $(customize.IDS.TILES).focus();
};

/**
 * Add background image src to the tile and add animation for the tile once it
 * successfully loaded.
 * @param {!Object} tile the tile that needs to be loaded.
 * @param {!Object} imageData the source imageData.
 * @param {?Function} countLoad If not null, called after the tile finishes
 *     loading.
 */
customize.loadTile = function(tile, imageData, countLoad) {
  if (imageData[tile.dataset.tileNum].collectionId === 'solidcolors') {
    tile.style.backgroundImage = [
      customize.CUSTOM_BACKGROUND_OVERLAY,
      'url(' + imageData[tile.dataset.tileNum].thumbnailImageUrl + ')'
    ].join(',').trim();
  } else {
    tile.style.backgroundImage =
        'url(' + imageData[tile.dataset.tileNum].thumbnailImageUrl + ')';
  }
  customize.fadeInImageTile(
      tile, imageData[tile.dataset.tileNum].thumbnailImageUrl, countLoad);
};

/**
 * Fade in effect for both collection and image tile. Once the image
 * successfully loads, we can assume the background image with the same source
 * has also loaded. Then, we set opacity for the tile to start the animation.
 * @param {!Object} tile The tile to add the fade in animation to.
 * @param {string} imageUrl the image url for the tile
 * @param {?Function} countLoad If not null, called after the tile finishes
 *     loading.
 */
customize.fadeInImageTile = function(tile, imageUrl, countLoad) {
  const image = new Image();
  image.onload = () => {
    tile.style.opacity = '1';
    if (countLoad) {
      countLoad();
    }
  };
  image.src = imageUrl;
};

/**
 * Load the NTPBackgroundCollections script. It'll create a global
 * variable name "coll" which is a dict of background collections data.
 * @private
 */
customize.loadChromeBackgrounds = function() {
  const collElement = $('ntp-collection-loader');
  if (collElement) {
    collElement.parentNode.removeChild(collElement);
  }
  const collScript = document.createElement('script');
  collScript.id = 'ntp-collection-loader';
  collScript.src = 'chrome-search://local-ntp/ntp-background-collections.js?' +
      'collection_type=background';
  collScript.onload = function() {
    if (configData.richerPicker) {
      customize.showCollectionSelectionDialog();
    }
  };
  document.body.appendChild(collScript);
};

/**
 * Close dialog when an image is selected via the file picker.
 */
customize.closeCustomizationDialog = function() {
  if (configData.richerPicker) {
    $(customize.IDS.CUSTOMIZATION_MENU).close();
  } else {
    $(customize.IDS.EDIT_BG_DIALOG).close();
  }
};

/**
 * Get the next visible option. There are times when various combinations of
 * options are hidden.
 * @param {number} current_index Index of the option the key press occurred on.
 * @param {number} deltaY Direction to search in, -1 for up, 1 for down.
 */
customize.getNextOption = function(current_index, deltaY) {
  // Create array corresponding to the menu. Important that this is in the same
  // order as the MENU_ENTRIES enum, so we can index into it.
  const entries = [];
  entries.push($(customize.IDS.DEFAULT_WALLPAPERS));
  entries.push($(customize.IDS.UPLOAD_IMAGE));
  entries.push($(customize.IDS.CUSTOM_LINKS_RESTORE_DEFAULT));
  entries.push($(customize.IDS.RESTORE_DEFAULT));

  let idx = current_index;
  do {
    idx = idx + deltaY;
    if (idx === -1) {
      idx = 3;
    }
    if (idx === 4) {
      idx = 0;
    }
  } while (
      idx !== current_index &&
      (entries[idx].hidden ||
       entries[idx].classList.contains(customize.CLASSES.OPTION_DISABLED)));
  return entries[idx];
};

/**
 * Hide custom background options based on the network state
 * @param {boolean} online The current state of the network
 */
customize.networkStateChanged = function(online) {
  $(customize.IDS.DEFAULT_WALLPAPERS).hidden = !online;
};

/**
 * Set customization menu to default options (custom backgrounds).
 */
customize.richerPicker_setCustomizationMenuToDefaultState = function() {
  customize.richerPicker_resetCustomizationMenu();
  $(customize.IDS.BACKGROUNDS_MENU)
      .classList.toggle(customize.CLASSES.MENU_SHOWN, true);
  customize.richerPicker_selectedOption = $(customize.IDS.BACKGROUNDS_BUTTON);
};

/**
 * Resets customization menu options.
 */
customize.richerPicker_resetCustomizationMenu = function() {
  customize.richerPicker_resetImageMenu(false);
  $(customize.IDS.BACKGROUNDS_MENU)
      .classList.toggle(customize.CLASSES.MENU_SHOWN, false);
  $(customize.IDS.SHORTCUTS_MENU)
      .classList.toggle(customize.CLASSES.MENU_SHOWN, false);
  $(customize.IDS.COLORS_MENU)
      .classList.toggle(customize.CLASSES.MENU_SHOWN, false);
  if (customize.richerPicker_selectedOption) {
    customize.richerPicker_selectedOption.classList.toggle(
        customize.CLASSES.SELECTED, false);
    customize.richerPicker_selectedOption = null;
  }
};

/**
 * Close customization menu.
 */
customize.richerPicker_closeCustomizationMenu = function() {
  $(customize.IDS.CUSTOMIZATION_MENU).close();
  customize.richerPicker_resetCustomizationMenu();
};

/**
 * Initialize the settings menu, custom backgrounds dialogs, and custom
 * links menu items. Set the text and event handlers for the various
 * elements.
 * @param {!Function} showErrorNotification Called when the error notification
 *     should be displayed.
 * @param {!Function} hideCustomLinkNotification Called when the custom link
 *     notification should be hidden.
 */
customize.init = function(showErrorNotification, hideCustomLinkNotification) {
  ntpApiHandle = window.chrome.embeddedSearch.newTabPage;
  const editDialog = $(customize.IDS.EDIT_BG_DIALOG);
  const menu = $(customize.IDS.MENU);

  $(customize.IDS.OPTIONS_TITLE).textContent =
      configData.translatedStrings.customizeBackground;

  // Store the main menu title so it can be restored if needed.
  $(customize.IDS.MENU_TITLE).dataset.mainTitle =
      $(customize.IDS.MENU_TITLE).textContent;

  $(customize.IDS.EDIT_BG_ICON)
      .setAttribute(
          'aria-label', configData.translatedStrings.customizeThisPage);

  $(customize.IDS.EDIT_BG_ICON)
      .setAttribute('title', configData.translatedStrings.customizeBackground);

  // Edit gear icon interaction events.
  const editBackgroundInteraction = function() {
    if (configData.richerPicker) {
      customize.richerPicker_setCustomizationMenuToDefaultState();
      customize.loadChromeBackgrounds();
      customize.loadColorTiles();
      $(customize.IDS.CUSTOMIZATION_MENU).showModal();
    } else {
      editDialog.showModal();
    }
  };
  $(customize.IDS.EDIT_BG).onclick = function(event) {
    editDialog.classList.add(customize.CLASSES.MOUSE_NAV);
    editBackgroundInteraction();
  };

  $(customize.IDS.MENU_CANCEL).onclick = function(event) {
    if (customize.richerPicker_selectedOption ==
        $(customize.IDS.COLORS_BUTTON)) {
      customize.colorsCancel();
    }
    customize.richerPicker_closeCustomizationMenu();
  };


  // Find the first menu option that is not hidden or disabled.
  const findFirstMenuOption = () => {
    const editMenu = $(customize.IDS.EDIT_BG_MENU);
    for (let i = 1; i < editMenu.children.length; i++) {
      const option = editMenu.children[i];
      if (option.classList.contains(customize.CLASSES.OPTION) &&
          !option.hidden &&
          !option.classList.contains(customize.CLASSES.OPTION_DISABLED)) {
        option.focus();
        return;
      }
    }
  };

  $(customize.IDS.EDIT_BG).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      // no default behavior for ENTER
      event.preventDefault();
      editDialog.classList.remove(customize.CLASSES.MOUSE_NAV);
      editBackgroundInteraction();
      findFirstMenuOption();
    }
  };

  // Interactions to close the customization option dialog.
  const editDialogInteraction = function() {
    editDialog.close();
  };
  editDialog.onclick = function(event) {
    editDialog.classList.add(customize.CLASSES.MOUSE_NAV);
    if (event.target === editDialog) {
      editDialogInteraction();
    }
  };
  editDialog.onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ESC) {
      editDialogInteraction();
    } else if (
        editDialog.classList.contains(customize.CLASSES.MOUSE_NAV) &&
        (event.keyCode === customize.KEYCODES.TAB ||
         event.keyCode === customize.KEYCODES.UP ||
         event.keyCode === customize.KEYCODES.DOWN)) {
      // When using tab in mouse navigation mode, select the first option
      // available.
      event.preventDefault();
      findFirstMenuOption();
      editDialog.classList.remove(customize.CLASSES.MOUSE_NAV);
    } else if (event.keyCode === customize.KEYCODES.TAB) {
      // If keyboard navigation is attempted, remove mouse-only mode.
      editDialog.classList.remove(customize.CLASSES.MOUSE_NAV);
    } else if (customize.arrowKeys.includes(event.keyCode)) {
      event.preventDefault();
      editDialog.classList.remove(customize.CLASSES.MOUSE_NAV);
    }
  };

  customize.initCustomLinksItems(hideCustomLinkNotification);
  customize.initCustomBackgrounds(showErrorNotification);
};

/**
 * Initialize custom link items in the settings menu dialog. Set the text
 * and event handlers for the various elements.
 * @param {!Function} hideCustomLinkNotification Called when the custom link
 *     notification should be hidden.
 */
customize.initCustomLinksItems = function(hideCustomLinkNotification) {
  customize.hideCustomLinkNotification = hideCustomLinkNotification;

  const editDialog = $(customize.IDS.EDIT_BG_DIALOG);
  const menu = $(customize.IDS.MENU);

  $(customize.IDS.CUSTOM_LINKS_RESTORE_DEFAULT_TEXT).textContent =
      configData.translatedStrings.restoreDefaultLinks;

  // Interactions with the "Restore default shortcuts" option.
  const customLinksRestoreDefaultInteraction = function() {
    editDialog.close();
    customize.hideCustomLinkNotification();
    window.chrome.embeddedSearch.newTabPage.resetCustomLinks();
    ntpApiHandle.logEvent(customize.BACKGROUND_CUSTOMIZATION_LOG_TYPE
                              .NTP_CUSTOMIZE_RESTORE_SHORTCUTS_CLICKED);
  };
  $(customize.IDS.CUSTOM_LINKS_RESTORE_DEFAULT).onclick = () => {
    if (!$(customize.IDS.CUSTOM_LINKS_RESTORE_DEFAULT)
             .classList.contains(customize.CLASSES.OPTION_DISABLED)) {
      customLinksRestoreDefaultInteraction();
    }
  };
  $(customize.IDS.CUSTOM_LINKS_RESTORE_DEFAULT).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER) {
      customLinksRestoreDefaultInteraction();
    } else if (event.keyCode === customize.KEYCODES.UP) {
      // Handle arrow key navigation.
      event.preventDefault();
      customize
          .getNextOption(
              customize.MENU_ENTRIES.CUSTOM_LINKS_RESTORE_DEFAULT, -1)
          .focus();
    } else if (event.keyCode === customize.KEYCODES.DOWN) {
      event.preventDefault();
      customize
          .getNextOption(customize.MENU_ENTRIES.CUSTOM_LINKS_RESTORE_DEFAULT, 1)
          .focus();
    }
  };
};

/**
 * Initialize the settings menu and custom backgrounds dialogs. Set the
 * text and event handlers for the various elements.
 * @param {!Function} showErrorNotification Called when the error notification
 *     should be displayed.
 */
customize.initCustomBackgrounds = function(showErrorNotification) {
  customize.showErrorNotification = showErrorNotification;

  const editDialog = $(customize.IDS.EDIT_BG_DIALOG);
  const menu = $(customize.IDS.MENU);

  $(customize.IDS.DEFAULT_WALLPAPERS_TEXT).textContent =
      configData.translatedStrings.defaultWallpapers;
  $(customize.IDS.UPLOAD_IMAGE_TEXT).textContent =
      configData.translatedStrings.uploadImage;
  $(customize.IDS.RESTORE_DEFAULT_TEXT).textContent =
      configData.translatedStrings.restoreDefaultBackground;
  $(customize.IDS.DONE).textContent =
      configData.translatedStrings.selectionDone;
  $(customize.IDS.CANCEL).textContent =
      configData.translatedStrings.selectionCancel;

  window.addEventListener('online', function(event) {
    customize.networkStateChanged(true);
  });

  window.addEventListener('offline', function(event) {
    customize.networkStateChanged(false);
  });

  if (!window.navigator.onLine) {
    customize.networkStateChanged(false);
  }

  $(customize.IDS.BACK_CIRCLE)
      .setAttribute('aria-label', configData.translatedStrings.backLabel);
  $(customize.IDS.CANCEL)
      .setAttribute('aria-label', configData.translatedStrings.selectionCancel);
  $(customize.IDS.DONE)
      .setAttribute('aria-label', configData.translatedStrings.selectionDone);

  $(customize.IDS.DONE).disabled = true;

  // Interactions with the "Upload an image" option.
  const uploadImageInteraction = function() {
    window.chrome.embeddedSearch.newTabPage.selectLocalBackgroundImage();
    ntpApiHandle.logEvent(customize.BACKGROUND_CUSTOMIZATION_LOG_TYPE
                              .NTP_CUSTOMIZE_LOCAL_IMAGE_CLICKED);
  };

  $(customize.IDS.UPLOAD_IMAGE).onclick = (event) => {
    if (!$(customize.IDS.UPLOAD_IMAGE)
             .classList.contains(customize.CLASSES.OPTION_DISABLED)) {
      uploadImageInteraction();
    }
  };
  $(customize.IDS.UPLOAD_IMAGE).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER) {
      uploadImageInteraction();
    }

    // Handle arrow key navigation.
    if (event.keyCode === customize.KEYCODES.UP) {
      event.preventDefault();
      customize.getNextOption(customize.MENU_ENTRIES.UPLOAD_IMAGE, -1).focus();
    }
    if (event.keyCode === customize.KEYCODES.DOWN) {
      event.preventDefault();
      customize.getNextOption(customize.MENU_ENTRIES.UPLOAD_IMAGE, 1).focus();
    }
  };

  // Interactions with the "Restore default background" option.
  const restoreDefaultInteraction = function() {
    editDialog.close();
    customize.clearAttribution();
    window.chrome.embeddedSearch.newTabPage.setBackgroundURL('');
    ntpApiHandle.logEvent(customize.BACKGROUND_CUSTOMIZATION_LOG_TYPE
                              .NTP_CUSTOMIZE_RESTORE_BACKGROUND_CLICKED);
  };
  $(customize.IDS.RESTORE_DEFAULT).onclick = (event) => {
    if (!$(customize.IDS.RESTORE_DEFAULT)
             .classList.contains(customize.CLASSES.OPTION_DISABLED)) {
      restoreDefaultInteraction();
    }
  };
  $(customize.IDS.RESTORE_DEFAULT).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER) {
      restoreDefaultInteraction();
    }

    // Handle arrow key navigation.
    if (event.keyCode === customize.KEYCODES.UP) {
      event.preventDefault();
      customize.getNextOption(customize.MENU_ENTRIES.RESTORE_DEFAULT, -1)
          .focus();
    }
    if (event.keyCode === customize.KEYCODES.DOWN) {
      event.preventDefault();
      customize.getNextOption(customize.MENU_ENTRIES.RESTORE_DEFAULT, 1)
          .focus();
    }
  };

  // Interactions with the "Chrome backgrounds" option.
  const defaultWallpapersInteraction = function(event) {
    customize.loadChromeBackgrounds();
    $('ntp-collection-loader').onload = function() {
      editDialog.close();
      if (typeof coll != 'undefined' && coll.length > 0) {
        customize.showCollectionSelectionDialog();
      } else {
        customize.handleError(collErrors);
      }
    };
    ntpApiHandle.logEvent(customize.BACKGROUND_CUSTOMIZATION_LOG_TYPE
                              .NTP_CUSTOMIZE_CHROME_BACKGROUNDS_CLICKED);
  };
  $(customize.IDS.DEFAULT_WALLPAPERS).onclick = function(event) {
    $(customize.IDS.MENU).classList.add(customize.CLASSES.MOUSE_NAV);
    defaultWallpapersInteraction(event);
  };
  $(customize.IDS.DEFAULT_WALLPAPERS).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER) {
      $(customize.IDS.MENU).classList.remove(customize.CLASSES.MOUSE_NAV);
      defaultWallpapersInteraction(event);
    }

    // Handle arrow key navigation.
    if (event.keyCode === customize.KEYCODES.UP) {
      event.preventDefault();
      customize.getNextOption(customize.MENU_ENTRIES.CHROME_BACKGROUNDS, -1)
          .focus();
    }
    if (event.keyCode === customize.KEYCODES.DOWN) {
      event.preventDefault();
      customize.getNextOption(customize.MENU_ENTRIES.CHROME_BACKGROUNDS, 1)
          .focus();
    }
  };

  // Escape and Backspace handling for the background picker dialog.
  menu.onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.SPACE) {
      $(customize.IDS.TILES).scrollTop += $(customize.IDS.TILES).offsetHeight;
      event.stopPropagation();
      event.preventDefault();
    }
    if (event.keyCode === customize.KEYCODES.ESC ||
        event.keyCode === customize.KEYCODES.BACKSPACE) {
      event.preventDefault();
      event.stopPropagation();
      if (menu.classList.contains(customize.CLASSES.COLLECTION_DIALOG)) {
        menu.close();
        customize.resetSelectionDialog();
      } else {
        customize.resetSelectionDialog();
        customize.showCollectionSelectionDialog();
      }
    }

    // If keyboard navigation is attempted, remove mouse-only mode.
    if (event.keyCode === customize.KEYCODES.TAB ||
        event.keyCode === customize.KEYCODES.LEFT ||
        event.keyCode === customize.KEYCODES.UP ||
        event.keyCode === customize.KEYCODES.RIGHT ||
        event.keyCode === customize.KEYCODES.DOWN) {
      menu.classList.remove(customize.CLASSES.MOUSE_NAV);
    }
  };

  // Interactions with the back arrow on the image selection dialog.
  const backInteraction = function(event) {
    if (configData.richerPicker) {
      customize.richerPicker_resetImageMenu(true);
    }
    customize.resetSelectionDialog();
    customize.showCollectionSelectionDialog();
  };
  $(customize.IDS.BACK_CIRCLE).onclick = backInteraction;
  $(customize.IDS.MENU_BACK_CIRCLE).onclick = backInteraction;
  $(customize.IDS.BACK_CIRCLE).onkeyup = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      backInteraction(event);
    }
  };
  $(customize.IDS.MENU_BACK_CIRCLE).onkeyup = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      backInteraction(event);
    }
  };
  // Pressing Spacebar on the back arrow shouldn't scroll the dialog.
  $(customize.IDS.BACK_CIRCLE).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.SPACE) {
      event.stopPropagation();
    }
  };

  // Interactions with the cancel button on the background picker dialog.
  $(customize.IDS.CANCEL).onclick = function(event) {
    customize.closeCollectionDialog(menu);
    ntpApiHandle.logEvent(customize.BACKGROUND_CUSTOMIZATION_LOG_TYPE
                              .NTP_CUSTOMIZE_CHROME_BACKGROUND_CANCEL);
  };
  $(customize.IDS.CANCEL).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      customize.closeCollectionDialog(menu);
      ntpApiHandle.logEvent(customize.BACKGROUND_CUSTOMIZATION_LOG_TYPE
                                .NTP_CUSTOMIZE_CHROME_BACKGROUND_CANCEL);
    }
  };

  // Interactions with the done button on the background picker dialog.
  const doneInteraction = function(event) {
    const done = configData.richerPicker ? $(customize.IDS.MENU_DONE) :
                                           $(customize.IDS.DONE);
    if (done.disabled) {
      return;
    }

    if (customize.richerPicker_selectedOption ==
        $(customize.IDS.COLORS_BUTTON)) {
      customize.colorsDone();
      customize.richerPicker_closeCustomizationMenu();
    } else {
      customize.setBackground(
          customize.selectedTile.dataset.url,
          customize.selectedTile.dataset.attributionLine1,
          customize.selectedTile.dataset.attributionLine2,
          customize.selectedTile.dataset.attributionActionUrl);
    }
  };
  $(customize.IDS.DONE).onclick = doneInteraction;
  $(customize.IDS.MENU_DONE).onclick = doneInteraction;
  $(customize.IDS.DONE).onkeyup = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER) {
      doneInteraction(event);
    }
  };

  // On any arrow key event in the tiles area, focus the first tile.
  $(customize.IDS.TILES).onkeydown = function(event) {
    if (customize.arrowKeys.includes(event.keyCode)) {
      event.preventDefault();
      if ($(customize.IDS.MENU)
              .classList.contains(customize.CLASSES.COLLECTION_DIALOG)) {
        $('coll_tile_0').focus();
      } else {
        $('img_tile_0').focus();
      }
    }
  };

  $(customize.IDS.BACKGROUNDS_MENU).onkeydown = function(event) {
    if (customize.arrowKeys.includes(event.keyCode)) {
      $(customize.IDS.BACKGROUNDS_UPLOAD_WRAPPER).focus();
    }
  };

  $(customize.IDS.BACKGROUNDS_IMAGE_MENU).onkeydown = function(event) {
    if (customize.arrowKeys.includes(event.keyCode)) {
      $('img_tile_0').focus();
    }
  };

  $(customize.IDS.BACKGROUNDS_UPLOAD).onclick = uploadImageInteraction;
  $(customize.IDS.BACKGROUNDS_UPLOAD).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      uploadImageInteraction();
    }
  };

  $(customize.IDS.BACKGROUNDS_DEFAULT).onclick = function(event) {
    const tile = $(customize.IDS.BACKGROUNDS_DEFAULT_ICON);
    tile.dataset.url = '';
    tile.dataset.attributionLine1 = '';
    tile.dataset.attributionLine2 = '';
    tile.dataset.attributionActionUrl = '';
    customize.richerPicker_selectTile(tile);
  };
  $(customize.IDS.BACKGROUNDS_DEFAULT).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      $(customize.IDS.BACKGROUNDS_DEFAULT).onclick(event);
    }
  };

  const richerPickerOpenBackgrounds = function() {
    customize.richerPicker_resetCustomizationMenu();
    customize.richerPicker_selectMenuOption(
        $(customize.IDS.BACKGROUNDS_BUTTON), $(customize.IDS.BACKGROUNDS_MENU));
  };

  $(customize.IDS.BACKGROUNDS_BUTTON).onclick = richerPickerOpenBackgrounds;
  $(customize.IDS.BACKGROUNDS_BUTTON).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      richerPickerOpenBackgrounds();
    }
  };

  const clOption = $(customize.IDS.SHORTCUTS_OPTION_CUSTOM_LINKS);
  clOption.onclick = function() {
    customize.richerPicker_selectShortcutOption(clOption);
  };
  clOption.onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      customize.richerPicker_selectShortcutOption(clOption);
    }
  };

  const mvOption = $(customize.IDS.SHORTCUTS_OPTION_MOST_VISITED);
  mvOption.onclick = function() {
    customize.richerPicker_selectShortcutOption(mvOption);
  };
  mvOption.onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      customize.richerPicker_selectShortcutOption(mvOption);
    }
  };

  const richerPickerOpenShortcuts = function() {
    customize.richerPicker_resetCustomizationMenu();
    customize.richerPicker_selectMenuOption(
        $(customize.IDS.SHORTCUTS_BUTTON), $(customize.IDS.SHORTCUTS_MENU));
  };

  $(customize.IDS.SHORTCUTS_BUTTON).onclick = richerPickerOpenShortcuts;
  $(customize.IDS.SHORTCUTS_BUTTON).onkeydown = function(event) {
    if (event.keyCode === customize.KEYCODES.ENTER ||
        event.keyCode === customize.KEYCODES.SPACE) {
      richerPickerOpenShortcuts();
    }
  };

  $(customize.IDS.COLORS_BUTTON).onclick = function() {
    customize.richerPicker_resetCustomizationMenu();
    customize.richerPicker_selectMenuOption(
        $(customize.IDS.COLORS_BUTTON), $(customize.IDS.COLORS_MENU));
    ntpApiHandle.getColorsInfo();
  };
};

customize.handleError = function(errors) {
  const unavailableString = configData.translatedStrings.backgroundsUnavailable;

  if (errors != 'undefined') {
    // Network errors.
    if (errors.net_error) {
      if (errors.net_error_no != 0) {
        const onClick = () => {
          window.open(
              'https://chrome://network-error/' + errors.net_error_no,
              '_blank');
        };
        customize.showErrorNotification(
            configData.translatedStrings.connectionError,
            configData.translatedStrings.moreInfo, onClick);
      } else {
        customize.showErrorNotification(
            configData.translatedStrings.connectionErrorNoPeriod);
      }
    } else if (errors.service_error) {  // Service errors.
      customize.showErrorNotification(unavailableString);
    }
    return;
  }

  // Generic error when we can't tell what went wrong.
  customize.showErrorNotification(unavailableString);
};

/**
 * Updates what is the selected tile of the Color menu and does necessary
 * changes for displaying the selection.
 * @param {Object} event The event attributes for the interaction.
 */
customize.updateColorMenuTileSelection = function(event) {
  if (customize.selectedColorTile) {
    customize.richerPicker_deselectTile(customize.selectedColorTile);
  }

  customize.richerPicker_selectTile(event.target);
  customize.selectedColorTile = event.target;
};

/**
 * Handles color tile selection.
 * @param {Object} event The event attributes for the interaction.
 */
customize.colorTileInteraction = function(event) {
  customize.updateColorMenuTileSelection(event);
  ntpApiHandle.applyAutogeneratedTheme(event.target.dataset.color.split(','));
};

/**
 * Handles default theme tile selection.
 * @param {Object} event The event attributes for the interaction.
 */
customize.defaultTileInteraction = function(event) {
  customize.updateColorMenuTileSelection(event);
  ntpApiHandle.applyDefaultTheme();
};

/**
 * Loads tiles for colors menu.
 */
customize.loadColorTiles = function() {
  if (customize.colorMenuLoaded) {
    return;
  }

  const colorsColl = ntpApiHandle.getColorsInfo();
  for (let i = 0; i < colorsColl.length; ++i) {
    const id = 'color_' + i;
    const imageUrl = colorsColl[i].icon;
    const name = colorsColl[i].label;
    const dataset = {'color': colorsColl[i].color};

    const tile = customize.createTileWithTitle(
        id, imageUrl, name, dataset, customize.colorTileInteraction,
        customize.colorTileInteraction);
    $(customize.IDS.COLORS_MENU).appendChild(tile);
  }

  // Configure the default tile.
  $(customize.IDS.COLORS_DEFAULT).dataset.color = null;
  $(customize.IDS.COLORS_DEFAULT).onclick = customize.defaultTileInteraction;

  customize.colorMenuLoaded = true;
};

/**
 * Handles 'Done' button interaction when Colors is the current option in the
 * customization menu.
 */
customize.colorsDone = function() {
  ntpApiHandle.confirmThemeChanges();
};

/**
 * Handles 'Cancel' button interaction when Colors is the current option in the
 * customization menu.
 */
customize.colorsCancel = function() {
  ntpApiHandle.revertThemeChanges();
};
