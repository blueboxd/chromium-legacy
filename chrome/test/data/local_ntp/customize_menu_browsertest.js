// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests local NTP richer picker.
 */

/**
 * Local NTP's object for test and setup functions.
 */
test.customizeMenu = {};

/**
 * Enum for ids.
 * @enum {string}
 * @const
 */
test.customizeMenu.IDS = {
  BACKGROUNDS_MENU: 'backgrounds-menu',
  BACKGROUNDS_IMAGE_MENU: 'backgrounds-image-menu',
  COLORS_BUTTON: 'colors-button',
  COLORS_DEFAULT: 'colors-default',
  COLORS_MENU: 'colors-menu',
  CUSTOMIZATION_MENU: 'customization-menu',
  DONE: 'menu-done',
  EDIT_BG: 'edit-bg',
  MENU_BACK: 'menu-back',
  MENU_CANCEL: 'menu-cancel',
  MENU_DONE: 'menu-done',
  SHORTCUTS_BUTTON: 'shortcuts-button',
  SHORTCUTS_HIDE_TOGGLE: 'sh-hide-toggle',
  SHORTCUTS_MENU: 'shortcuts-menu',
  SHORTCUTS_OPTION_CUSTOM_LINKS: 'sh-option-cl',
  SHORTCUTS_OPTION_MOST_VISITED: 'sh-option-mv',
};

/**
 * Enum for classnames.
 * @enum {string}
 * @const
 */
test.customizeMenu.CLASSES = {
  ENTRY_POINT_ENHANCED: 'ep-enhanced',
  MENU_SHOWN: 'menu-shown',
  SELECTED: 'selected',
};

/**
 * Utility to mock out parts of the DOM.
 * @type {Replacer}
 */
test.customizeMenu.stubs = new Replacer();

/**
 * The test value of chrome.embeddedSearch.newTabPage.isUsingMostVisited.
 * @type {boolean}
 */
test.customizeMenu.isUsingMostVisited = false;

/**
 * The test value of chrome.embeddedSearch.newTabPage.areShortcutsVisible.
 * @type {boolean}
 */
test.customizeMenu.areShortcutsVisible = true;

/**
 * The number of times
 * chrome.embeddedSearch.newTabPage.toggleMostVisitedOrCustomLinks is called.
 * @type {number}
 */
test.customizeMenu.toggleMostVisitedOrCustomLinksCount = 0;

/**
 * The number of times
 * chrome.embeddedSearch.newTabPage.toggleShortcutsVisibility is called.
 * @type {number}
 */
test.customizeMenu.toggleShortcutsVisibilityCount = 0;

/**
 * The number of times
 * chrome.embeddedSearch.newTabPage.SetCustomBackground* is called.
 * @type {number}
 */
test.customizeMenu.timesCustomBackgroundWasSet = 0;

/**
 * Sets up the page for each individual test.
 */
test.customizeMenu.setUp = function() {
  setUpPage('local-ntp-template');

  // Required to enable the richer picker. |configData| does not correctly
  // populate using base::test::ScopedFeatureList.
  configData.richerPicker = true;
  configData.chromeColors = true;
  customize.colorMenuLoaded = false;
  customize.builtTiles = false;

  // Reset variable values.
  test.customizeMenu.stubs.reset();
  test.customizeMenu.isUsingMostVisited = false;
  test.customizeMenu.areShortcutsVisible = true;
  test.customizeMenu.toggleMostVisitedOrCustomLinksCount = 0;
  test.customizeMenu.toggleShortcutsVisibilityCount = 0;
  test.customizeMenu.timesCustomBackgroundWasSet = 0;
};

// ******************************* SIMPLE TESTS *******************************
// These are run by runSimpleTests above.
// Functions from test_utils.js are automatically imported.

/**
 * Tests that the richer picker opens to the default submenu (Background).
 */
test.customizeMenu.testOpenCustomizeMenu = function() {
  init();

  const menuButton = $(test.customizeMenu.IDS.EDIT_BG);
  assertTrue(elementIsVisible(menuButton));
  assertTrue(menuButton.classList.contains(
      test.customizeMenu.CLASSES.ENTRY_POINT_ENHANCED));

  menuButton.click();  // Open the richer picker.

  const menuDialog = $(test.customizeMenu.IDS.CUSTOMIZATION_MENU);
  assertTrue(menuDialog.open);
  assertTrue(elementIsVisible(menuDialog));

  // The default submenu (Background) should be shown.
  const backgroundSubmenu = $(test.customizeMenu.IDS.BACKGROUNDS_MENU);
  assertTrue(backgroundSubmenu.classList.contains(
      test.customizeMenu.CLASSES.MENU_SHOWN));
};

/**
 * Tests that the Shortcuts submenu can be opened.
 */
test.customizeMenu.testShortcuts_OpenSubmenu = function() {
  init();

  // Open the Shortcuts submenu.
  $(test.customizeMenu.IDS.EDIT_BG).click();
  $(test.customizeMenu.IDS.SHORTCUTS_BUTTON).click();

  assertTrue($(test.customizeMenu.IDS.SHORTCUTS_MENU)
                 .classList.contains(test.customizeMenu.CLASSES.MENU_SHOWN));
};

/**
 * Tests that the custom link option will be preselected.
 */
test.customizeMenu.testShortcuts_CustomLinksPreselected = function() {
  init();  // Custom links should be enabled and the shortcuts not hidden.

  // Open the Shortcuts submenu.
  $(test.customizeMenu.IDS.EDIT_BG).click();
  $(test.customizeMenu.IDS.SHORTCUTS_BUTTON).click();

  // Check that only the custom links option is preselected.
  assertShortcutOptionsSelected(
      /*clSelected=*/ true, /*mvSelected=*/ false, /*isHidden=*/ false);
};

/**
 * Tests that the most visited option will be preselected.
 */
test.customizeMenu.testShortcuts_MostVisitedPreselected = function() {
  test.customizeMenu.isUsingMostVisited = true;
  init();  // Most visited should be enabled and the shortcuts not hidden.

  // Open the Shortcuts submenu.
  $(test.customizeMenu.IDS.EDIT_BG).click();
  $(test.customizeMenu.IDS.SHORTCUTS_BUTTON).click();

  // Check that only the most visited option is preselected.
  assertShortcutOptionsSelected(
      /*clSelected=*/ false, /*mvSelected=*/ true, /*isHidden=*/ false);
};

/**
 * Tests that the "hide shortcuts" option will be preselected.
 */
test.customizeMenu.testShortcuts_IsHiddenPreselected = function() {
  test.customizeMenu.areShortcutsVisible = false;
  init();  // Custom links should be enabled and the shortcuts hidden.

  // Open the Shortcuts submenu.
  $(test.customizeMenu.IDS.EDIT_BG).click();
  $(test.customizeMenu.IDS.SHORTCUTS_BUTTON).click();

  // Check that the custom links and "hide shortcuts" options are preselected.
  assertShortcutOptionsSelected(
      /*clSelected=*/ true, /*mvSelected=*/ false, /*isHidden=*/ true);
};

/**
 * Tests that the shortcut options can be selected.
 */
test.customizeMenu.testShortcuts_CanSelectOptions = function() {
  init();  // Custom links should be enabled and the shortcuts not hidden.

  // Open the Shortcuts submenu.
  $(test.customizeMenu.IDS.EDIT_BG).click();
  $(test.customizeMenu.IDS.SHORTCUTS_BUTTON).click();

  assertShortcutOptionsSelected(
      /*clSelected=*/ true, /*mvSelected=*/ false, /*isHidden=*/ false);

  // Select the custom links option. The option should remain selected.
  $(test.customizeMenu.IDS.SHORTCUTS_OPTION_CUSTOM_LINKS).click();
  assertShortcutOptionsSelected(true, false, false);

  // Select the most visited option. The custom links option should be
  // deselected.
  $(test.customizeMenu.IDS.SHORTCUTS_OPTION_MOST_VISITED).click();
  assertShortcutOptionsSelected(false, true, false);

  // Toggle the hide shortcuts option. Toggle should be enabled.
  const hiddenToggle = $(test.customizeMenu.IDS.SHORTCUTS_HIDE_TOGGLE);
  hiddenToggle.click();
  assertShortcutOptionsSelected(false, true, true);

  // Toggle the hide shortcuts option again. Toggle should be disabled.
  hiddenToggle.click();
  assertShortcutOptionsSelected(false, true, false);
};

/**
 * Tests that the shortcut options will not be applied when the user cancels
 * customization.
 */
test.customizeMenu.testShortcuts_Cancel = function() {
  init();  // Custom links should be enabled and the shortcuts not hidden.

  // Open the Shortcuts submenu.
  $(test.customizeMenu.IDS.EDIT_BG).click();
  $(test.customizeMenu.IDS.SHORTCUTS_BUTTON).click();

  // Select the custom links option and hide the shortcuts.
  $(test.customizeMenu.IDS.SHORTCUTS_OPTION_MOST_VISITED).click();
  $(test.customizeMenu.IDS.SHORTCUTS_HIDE_TOGGLE).click();
  assertShortcutOptionsSelected(
      /*clSelected=*/ false, /*mvSelected=*/ true, /*isHidden=*/ true);

  $(test.customizeMenu.IDS.MENU_CANCEL).click();

  // Check that the EmbeddedSearchAPI was not called.
  assertEquals(0, test.customizeMenu.toggleMostVisitedOrCustomLinksCount);
  assertEquals(0, test.customizeMenu.toggleShortcutsVisibilityCount);
};

/**
 * Tests that the shortcut type will be changed on "done".
 */
test.customizeMenu.testShortcuts_ChangeShortcutType = function() {
  init();  // Custom links should be enabled and the shortcuts not hidden.

  // Open the Shortcuts submenu.
  $(test.customizeMenu.IDS.EDIT_BG).click();
  $(test.customizeMenu.IDS.SHORTCUTS_BUTTON).click();

  // Select the most visited option.
  $(test.customizeMenu.IDS.SHORTCUTS_OPTION_MOST_VISITED).click();
  assertShortcutOptionsSelected(
      /*clSelected=*/ false, /*mvSelected=*/ true, /*isHidden=*/ false);

  $(test.customizeMenu.IDS.MENU_DONE).click();

  // Check that the EmbeddedSearchAPI was correctly called.
  assertEquals(1, test.customizeMenu.toggleMostVisitedOrCustomLinksCount);
  assertEquals(0, test.customizeMenu.toggleShortcutsVisibilityCount);
};

/**
 * Tests that the shortcut visibility will be changed on "done".
 */
test.customizeMenu.testShortcuts_ChangeVisibility = function() {
  init();  // Custom links should be enabled and the shortcuts not hidden.

  // Open the Shortcuts submenu.
  $(test.customizeMenu.IDS.EDIT_BG).click();
  $(test.customizeMenu.IDS.SHORTCUTS_BUTTON).click();

  // Hide the shortcuts.
  $(test.customizeMenu.IDS.SHORTCUTS_HIDE_TOGGLE).click();
  assertShortcutOptionsSelected(
      /*clSelected=*/ true, /*mvSelected=*/ false, /*isHidden=*/ true);

  $(test.customizeMenu.IDS.MENU_DONE).click();

  // Check that the EmbeddedSearchAPI was correctly called.
  assertEquals(0, test.customizeMenu.toggleMostVisitedOrCustomLinksCount);
  assertEquals(1, test.customizeMenu.toggleShortcutsVisibilityCount);
};

/**
 * Tests that the shortcut type and visibility will be changed on "done".
 */
test.customizeMenu.testShortcuts_ChangeShortcutTypeAndVisibility = function() {
  test.customizeMenu.isUsingMostVisited = true;
  init();  // Most visited should be enabled and the shortcuts not hidden.

  // Open the Shortcuts submenu.
  $(test.customizeMenu.IDS.EDIT_BG).click();
  $(test.customizeMenu.IDS.SHORTCUTS_BUTTON).click();

  // Select the custom links option and hide the shortcuts.
  $(test.customizeMenu.IDS.SHORTCUTS_OPTION_CUSTOM_LINKS).click();
  $(test.customizeMenu.IDS.SHORTCUTS_HIDE_TOGGLE).click();
  assertShortcutOptionsSelected(
      /*clSelected=*/ true, /*mvSelected=*/ false, /*isHidden=*/ true);

  $(test.customizeMenu.IDS.MENU_DONE).click();

  // Check that the EmbeddedSearchAPI was correctly called.
  assertEquals(1, test.customizeMenu.toggleMostVisitedOrCustomLinksCount);
  assertEquals(1, test.customizeMenu.toggleShortcutsVisibilityCount);
};

/**
 * Test that Customization dialog doesn't have Colors option when Colors are
 * disabled.
 */
test.customizeMenu.testColors_Disabled = function() {
  configData.chromeColors = false;
  init();

  $(test.customizeMenu.IDS.EDIT_BG).click();
  assertFalse(elementIsVisible($(test.customizeMenu.IDS.COLORS_BUTTON)));
};

/**
 * Test that Customization dialog has Colors option when Colors are enabled.
 */
test.customizeMenu.testColors_Enabled = function() {
  init();

  $(test.customizeMenu.IDS.EDIT_BG).click();
  assertTrue(elementIsVisible($(test.customizeMenu.IDS.COLORS_BUTTON)));
};

/**
 * Test that Colors menu is visible after Colors button is clicked.
 */
test.customizeMenu.testColors_MenuVisibility = function() {
  init();

  $(test.customizeMenu.IDS.EDIT_BG).click();
  assertTrue(elementIsVisible($(test.customizeMenu.IDS.COLORS_BUTTON)));
  assertFalse(elementIsVisible($(test.customizeMenu.IDS.COLORS_MENU)));

  $(test.customizeMenu.IDS.COLORS_BUTTON).click();
  assertTrue(elementIsVisible($(test.customizeMenu.IDS.COLORS_MENU)));

  $(test.customizeMenu.IDS.SHORTCUTS_BUTTON).click();
  assertFalse(elementIsVisible($(test.customizeMenu.IDS.COLORS_MENU)));
};


/**
 * Test that default tile is visible.
 */
test.customizeMenu.testColors_DefaultTile = function() {
  init();

  $(test.customizeMenu.IDS.EDIT_BG).click();
  $(test.customizeMenu.IDS.COLORS_BUTTON).click();
  assertTrue(elementIsVisible($(test.customizeMenu.IDS.COLORS_DEFAULT)));
};

/**
 * Test that at least one color tile is loaded.
 */
test.customizeMenu.testColors_ColorTilesLoaded = function() {
  init();

  $(test.customizeMenu.IDS.EDIT_BG).click();
  $(test.customizeMenu.IDS.COLORS_BUTTON).click();
  assertTrue($(test.customizeMenu.IDS.COLORS_MENU).children.length > 1);
  assertTrue(!!$('color_0').dataset.color);
  assertTrue(!!$('color_0').style.backgroundImage);
};

/*
 * Tests that a custom background can be set through the menu.
 */
test.customizeMenu.testBackgrounds_SetCustomBackground = function() {
  init();

  setupFakeAsyncCollectionLoad();

  // Open the Shortcuts submenu.
  $(test.customizeMenu.IDS.EDIT_BG).click();

  const backgroundSubmenu = $(test.customizeMenu.IDS.BACKGROUNDS_MENU);
  const backgroundImageSubmenu =
      $(test.customizeMenu.IDS.BACKGROUNDS_IMAGE_MENU);
  assertTrue(backgroundSubmenu.classList.contains(
      test.customizeMenu.CLASSES.MENU_SHOWN));

  // 5 total tiles: upload, default, and the 3 collection tiles.
  assertTrue(
      $(test.customizeMenu.IDS.BACKGROUNDS_MENU)
          .getElementsByClassName('bg-sel-tile')
          .length === 5);

  setupFakeAsyncImageLoad('coll_tile_0');
  $('coll_tile_0').click();

  // The open menu is now the images menu with 4 tiles.
  assertFalse(backgroundSubmenu.classList.contains(
      test.customizeMenu.CLASSES.MENU_SHOWN));
  assertTrue(backgroundImageSubmenu.classList.contains(
      test.customizeMenu.CLASSES.MENU_SHOWN));
  assertTrue(
      $(test.customizeMenu.IDS.BACKGROUNDS_IMAGE_MENU)
          .getElementsByClassName('bg-sel-tile')
          .length === 4);

  $('img_tile_0').click();
  $(test.customizeMenu.IDS.MENU_DONE).click();

  // No menu should be open, and setCustomBackground should have been called.
  assertFalse(backgroundSubmenu.classList.contains(
      test.customizeMenu.CLASSES.MENU_SHOWN));
  assertFalse(backgroundImageSubmenu.classList.contains(
      test.customizeMenu.CLASSES.MENU_SHOWN));
  assertEquals(1, test.customizeMenu.timesCustomBackgroundWasSet);
};

/**
 * Tests that canceling after selecting an image doesn't set
 * a custom background.
 */
test.customizeMenu.testBackgrounds_CancelCustomBackground = function() {
  init();

  assertEquals(0, test.customizeMenu.timesCustomBackgroundWasSet);
  setupFakeAsyncCollectionLoad();

  // Open the Shortcuts submenu.
  $(test.customizeMenu.IDS.EDIT_BG).click();

  const backgroundSubmenu = $(test.customizeMenu.IDS.BACKGROUNDS_MENU);
  const backgroundImageSubmenu =
      $(test.customizeMenu.IDS.BACKGROUNDS_IMAGE_MENU);
  assertTrue(backgroundSubmenu.classList.contains(
      test.customizeMenu.CLASSES.MENU_SHOWN));

  // 5 total tiles: upload, default, and the 3 collection tiles.
  assertTrue(
      $(test.customizeMenu.IDS.BACKGROUNDS_MENU)
          .getElementsByClassName('bg-sel-tile')
          .length === 5);

  setupFakeAsyncImageLoad('coll_tile_0');
  $('coll_tile_0').click();

  // The open menu is now the images menu with 4 tiles.
  assertFalse(backgroundSubmenu.classList.contains(
      test.customizeMenu.CLASSES.MENU_SHOWN));
  assertTrue(backgroundImageSubmenu.classList.contains(
      test.customizeMenu.CLASSES.MENU_SHOWN));
  assertTrue(
      $(test.customizeMenu.IDS.BACKGROUNDS_IMAGE_MENU)
          .getElementsByClassName('bg-sel-tile')
          .length === 4);

  $('img_tile_0').click();
  $(test.customizeMenu.IDS.MENU_CANCEL).click();

  // No menu should be open, and setCustomBackground should NOT have been
  // called.
  assertFalse(backgroundSubmenu.classList.contains(
      test.customizeMenu.CLASSES.MENU_SHOWN));
  assertFalse(backgroundImageSubmenu.classList.contains(
      test.customizeMenu.CLASSES.MENU_SHOWN));
  assertEquals(0, test.customizeMenu.timesCustomBackgroundWasSet);
};

/**
 * Tests the back arrow for custom backgrounds.
 */
test.customizeMenu.testBackgrounds_BackArrowCustomBackground = function() {
  init();

  setupFakeAsyncCollectionLoad();

  // Open the Shortcuts submenu.
  $(test.customizeMenu.IDS.EDIT_BG).click();

  const backgroundSubmenu = $(test.customizeMenu.IDS.BACKGROUNDS_MENU);
  const backgroundImageSubmenu =
      $(test.customizeMenu.IDS.BACKGROUNDS_IMAGE_MENU);
  assertTrue(backgroundSubmenu.classList.contains(
      test.customizeMenu.CLASSES.MENU_SHOWN));

  // 5 total tiles: upload, default, and the 3 collection tiles.
  assertTrue(
      $(test.customizeMenu.IDS.BACKGROUNDS_MENU)
          .getElementsByClassName('bg-sel-tile')
          .length === 5);

  setupFakeAsyncImageLoad('coll_tile_0');
  $('coll_tile_0').click();

  // The open menu is now the images menu with 4 tiles.
  assertFalse(backgroundSubmenu.classList.contains(
      test.customizeMenu.CLASSES.MENU_SHOWN));
  assertTrue(backgroundImageSubmenu.classList.contains(
      test.customizeMenu.CLASSES.MENU_SHOWN));
  assertTrue(
      $(test.customizeMenu.IDS.BACKGROUNDS_IMAGE_MENU)
          .getElementsByClassName('bg-sel-tile')
          .length === 4);

  $('img_tile_0').click();
  $(test.customizeMenu.IDS.MENU_BACK).click();

  // The main backgrounds menu should be open, and no custom background set.
  assertTrue(backgroundSubmenu.classList.contains(
      test.customizeMenu.CLASSES.MENU_SHOWN));
  assertFalse(backgroundImageSubmenu.classList.contains(
      test.customizeMenu.CLASSES.MENU_SHOWN));
  assertEquals(0, test.customizeMenu.timesCustomBackgroundWasSet);

  // Reopen the images menu, the selection should be cleared.
  $('coll_tile_0').click();

  assertFalse(backgroundSubmenu.classList.contains(
      test.customizeMenu.CLASSES.MENU_SHOWN));
  assertTrue(backgroundImageSubmenu.classList.contains(
      test.customizeMenu.CLASSES.MENU_SHOWN));
  assertTrue(
      $(test.customizeMenu.IDS.BACKGROUNDS_IMAGE_MENU)
          .getElementsByClassName('bg-sel-tile')
          .length === 4);
  assertTrue(
      $(test.customizeMenu.IDS.BACKGROUNDS_IMAGE_MENU)
          .getElementsByClassName('selected')
          .length === 0);
};

// ******************************* HELPERS *******************************

/**
 * Initialize the local NTP and mock the EmbeddedSearchAPI.
 */
init = function() {
  // Mock chrome.embeddedSearch functions. This must be done right before
  // initializing the local NTP.
  const toggleMostVisitedOrCustomLinks = () => {
    test.customizeMenu.toggleMostVisitedOrCustomLinksCount++
  };
  const toggleShortcutsVisibility = () => {
    test.customizeMenu.toggleShortcutsVisibilityCount++
  };
  const timesCustomBackgroundWasSet = () => {
    test.customizeMenu.timesCustomBackgroundWasSet++;
  };
  // We want to keep some EmbeddedSearchAPI functions, so save and add them to
  // our mock API.
  const getColorsInfo = chrome.embeddedSearch.newTabPage.getColorsInfo;
  const themeBackgroundInfo =
      chrome.embeddedSearch.newTabPage.themeBackgroundInfo;

  test.customizeMenu.stubs.replace(chrome.embeddedSearch, 'newTabPage', {
    toggleMostVisitedOrCustomLinks: toggleMostVisitedOrCustomLinks,
    toggleShortcutsVisibility: toggleShortcutsVisibility,
    isUsingMostVisited: test.customizeMenu.isUsingMostVisited,
    areShortcutsVisible: test.customizeMenu.areShortcutsVisible,
    setBackgroundURLWithAttributions: timesCustomBackgroundWasSet,
    resetCustomLinks: () => {},
    selectLocalBackgroundImage: () => {},
    setBackgroundURL: timesCustomBackgroundWasSet,
    logEvent: (a) => {},
    getColorsInfo: getColorsInfo,
    themeBackgroundInfo: themeBackgroundInfo,
  });

  initLocalNTP(/*isGooglePage=*/ true);
};

/**
 * Assert that the correct shortcut options are selected.
 * @param {boolean} clSelected True if custom links are selected.
 * @param {boolean} mvSelected True if most visited are selected.
 * @param {boolean} isHidden True if the hide shortcuts toggle should be on.
 */
assertShortcutOptionsSelected = function(clSelected, mvSelected, isHidden) {
  const clOption = $(test.customizeMenu.IDS.SHORTCUTS_OPTION_CUSTOM_LINKS);
  const mvOption = $(test.customizeMenu.IDS.SHORTCUTS_OPTION_MOST_VISITED);
  const hiddenToggle = $(test.customizeMenu.IDS.SHORTCUTS_HIDE_TOGGLE);
  assertEquals(
      clSelected,
      clOption.parentElement.classList.contains(
          test.customizeMenu.CLASSES.SELECTED));
  assertEquals(
      mvSelected,
      mvOption.parentElement.classList.contains(
          test.customizeMenu.CLASSES.SELECTED));
  assertEquals(isHidden, hiddenToggle.checked);
};

/**
 * Fake the loading of the Chrome Backgrounds collections so it happens
 * synchronously.
 */
setupFakeAsyncCollectionLoad = function() {
  // Override the collection loading script.
  customize.loadChromeBackgrounds = function() {
    const collScript = document.createElement('script');
    collScript.id = 'ntp-collection-loader';
    document.body.appendChild(collScript);
    coll = [
      {
        collectionId: 'collection1',
        collectionName: 'Collection 1',
        previewImageUrl: 'chrome-search://local-ntp/background.jpg'
      },
      {
        collectionId: 'collection2',
        collectionName: 'Collection 2',
        previewImageUrl: 'chrome-search://local-ntp/background.jpg'
      },
      {
        collectionId: 'collection3',
        collectionName: 'Collection 3',
        previewImageUrl: 'chrome-search://local-ntp/background.jpg'
      }
    ];
    collErrors = {};
    customize.showCollectionSelectionDialog();
  };
};

/**
 * Fake the loading of the a collection's images so it happens synchronously.
 */
setupFakeAsyncImageLoad = function(tile_id) {
  // Append the creation of the image data and a call to onload to the
  // end of the click handler.
  const oldImageLoader = $(tile_id).onclick;
  $(tile_id).onclick = function(event) {
    oldImageLoader(event);
    collImg = [
      {
        attributionActionUrl: 'https://www.google.com',
        attributions: ['test1', 'attribution1'],
        collectionId: 'collection1',
        imageUrl: 'chrome-search://local-ntp/background1.jpg',
        thumbnailImageUrl: 'chrome-search://local-ntp/background_thumbnail.jpg1'
      },
      {
        attributionActionUrl: 'https://www.google.com',
        attributions: ['test2', 'attribution2'],
        collectionId: 'collection1',
        imageUrl: 'chrome-search://local-ntp/background2.jpg',
        thumbnailImageUrl: 'chrome-search://local-ntp/background_thumbnail.jpg2'
      },
      {
        attributionActionUrl: 'https://www.google.com',
        attributions: ['test3', 'attribution3'],
        collectionId: 'collection1',
        imageUrl: 'chrome-search://local-ntp/background3.jpg',
        thumbnailImageUrl: 'chrome-search://local-ntp/background_thumbnail.jpg3'
      },
      {
        attributionActionUrl: 'https://www.google.com',
        attributions: ['test4', 'attribution4'],
        collectionId: 'collection1',
        imageUrl: 'chrome-search://local-ntp/background4.jpg',
        thumbnailImageUrl: 'chrome-search://local-ntp/background_thumbnail.jpg4'
      },
    ];
    collImgErrors = {};
    $('ntp-images-loader').onload();
  }
};
