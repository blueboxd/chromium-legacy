// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationConnectionStatus, DestinationOrigin, DestinationType, PrinterState, PrinterStatusReason, PrinterStatusSeverity} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {keyDownOn, move} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';

window.destination_dropdown_cros_test = {};
const destination_dropdown_cros_test = window.destination_dropdown_cros_test;
destination_dropdown_cros_test.suiteName =
    'PrintPreviewDestinationDropdownCrosTest';
/** @enum {string} */
destination_dropdown_cros_test.TestNames = {
  CorrectListItems: 'correct list items',
  ClickRemovesHighlight: 'click removes highlight',
  ClickCloses: 'click closes dropdown',
  TabCloses: 'tab closes dropdown',
  HighlightedAfterUpDown: 'highlighted after keyboard press up and down',
  EnterOpensCloses: 'enter opens and closes dropdown',
  HighlightedFollowsMouse: 'highlighted follows mouse',
  Disabled: 'disabled',
  HiddenDestinationBadge: 'hidden destination badge',
  NewStatusUpdatesDestinationIcon: 'new status updates destination icon',
  ChangingDestinationUpdatesIcon: 'changing destination updates icon',
  HighlightedWhenOpened: 'highlighted when opened',
};

suite(destination_dropdown_cros_test.suiteName, function() {
  /** @type {!PrintPreviewDestinationDropdownCrosElement} */
  let dropdown;

  /** @param {!Array<!Destination>} items */
  function setItemList(items) {
    dropdown.itemList = items;
    flush();
  }

  /** @return {!NodeList} */
  function getList() {
    return dropdown.shadowRoot.querySelectorAll('.list-item');
  }

  /** @param {?Element} element */
  function pointerDown(element) {
    element.dispatchEvent(new PointerEvent('pointerdown', {
      bubbles: true,
      cancelable: true,
      composed: true,
      buttons: 1,
    }));
  }

  function down() {
    keyDownOn(dropdown.$$('#dropdownInput'), 'ArrowDown', [], 'ArrowDown');
  }

  function up() {
    keyDownOn(dropdown.$$('#dropdownInput'), 'ArrowUp', [], 'ArrowUp');
  }

  function enter() {
    keyDownOn(dropdown.$$('#dropdownInput'), 'Enter', [], 'Enter');
  }

  function tab() {
    keyDownOn(dropdown.$$('#dropdownInput'), 'Tab', [], 'Tab');
  }

  /** @return {?Element} */
  function getHighlightedElement() {
    return dropdown.$$('[highlighted_]');
  }

  /** @return {string} */
  function getHighlightedElementText() {
    return getHighlightedElement().textContent.trim();
  }

  /**
   * @param {string} displayName
   * @param {!DestinationOrigin} destinationOrigin
   * @return {!Destination}
   */
  function createDestination(displayName, destinationOrigin) {
    return new Destination(
        displayName, DestinationType.LOCAL, destinationOrigin, displayName,
        DestinationConnectionStatus.ONLINE);
  }

  /** @override */
  setup(function() {
    document.body.innerHTML = '';

    dropdown =
        /** @type {!PrintPreviewDestinationDropdownCrosElement} */
        (document.createElement('print-preview-destination-dropdown-cros'));
    document.body.appendChild(dropdown);
    dropdown.noDestinations = false;
  });

  test(
      assert(destination_dropdown_cros_test.TestNames.CorrectListItems),
      function() {
        setItemList([
          createDestination('One', DestinationOrigin.CROS),
          createDestination('Two', DestinationOrigin.CROS),
          createDestination('Three', DestinationOrigin.CROS)
        ]);

        const itemList = getList();
        assertEquals(7, itemList.length);
        assertEquals('One', itemList[0].textContent.trim());
        assertEquals('Two', itemList[1].textContent.trim());
        assertEquals('Three', itemList[2].textContent.trim());
      });

  test(
      assert(destination_dropdown_cros_test.TestNames.ClickRemovesHighlight),
      function() {
        const destinationOne = createDestination('One', DestinationOrigin.CROS);
        setItemList([destinationOne]);
        dropdown.value = destinationOne;

        getList()[0].toggleAttribute('highlighted_', true);
        assertTrue(getList()[0].hasAttribute('highlighted_'));

        getList()[0].click();
        assertFalse(getList()[0].hasAttribute('highlighted_'));
      });

  test(
      assert(destination_dropdown_cros_test.TestNames.ClickCloses), function() {
        const destinationOne = createDestination('One', DestinationOrigin.CROS);
        setItemList([destinationOne]);
        dropdown.value = destinationOne;
        const ironDropdown = dropdown.$$('iron-dropdown');

        pointerDown(dropdown.$$('#dropdownInput'));
        assertTrue(ironDropdown.opened);

        getList()[0].click();
        assertFalse(ironDropdown.opened);

        pointerDown(dropdown.$$('#dropdownInput'));
        assertTrue(ironDropdown.opened);

        // Click outside dropdown to close the dropdown.
        pointerDown(document.body);
        assertFalse(ironDropdown.opened);
      });

  test(assert(destination_dropdown_cros_test.TestNames.TabCloses), function() {
    const destinationOne = createDestination('One', DestinationOrigin.CROS);
    setItemList([destinationOne]);
    dropdown.value = destinationOne;
    const ironDropdown = dropdown.$$('iron-dropdown');

    pointerDown(dropdown.$$('#dropdownInput'));
    assertTrue(ironDropdown.opened);

    tab();
    assertFalse(ironDropdown.opened);
  });

  test(
      assert(destination_dropdown_cros_test.TestNames.HighlightedAfterUpDown),
      function() {
        const destinationOne = createDestination('One', DestinationOrigin.CROS);
        setItemList([destinationOne]);
        dropdown.value = destinationOne;
        pointerDown(dropdown.$$('#dropdownInput'));

        assertEquals('One', getHighlightedElementText());
        down();
        assertEquals('Save as PDF', getHighlightedElementText());
        down();
        assertEquals('Save to Google Drive', getHighlightedElementText());
        down();
        assertEquals('See more…', getHighlightedElementText());
        down();
        assertEquals('One', getHighlightedElementText());

        up();
        assertEquals('See more…', getHighlightedElementText());
        up();
        assertEquals('Save to Google Drive', getHighlightedElementText());
        up();
        assertEquals('Save as PDF', getHighlightedElementText());
        up();
        assertEquals('One', getHighlightedElementText());
      });

  test(
      assert(destination_dropdown_cros_test.TestNames.EnterOpensCloses),
      function() {
        const destinationOne = createDestination('One', DestinationOrigin.CROS);
        setItemList([destinationOne]);
        dropdown.value = destinationOne;

        assertFalse(dropdown.$$('iron-dropdown').opened);
        enter();
        assertTrue(dropdown.$$('iron-dropdown').opened);
        enter();
        assertFalse(dropdown.$$('iron-dropdown').opened);
      });

  test(
      assert(destination_dropdown_cros_test.TestNames.HighlightedFollowsMouse),
      function() {
        const destinationOne = createDestination('One', DestinationOrigin.CROS);
        setItemList([
          destinationOne, createDestination('Two', DestinationOrigin.CROS),
          createDestination('Three', DestinationOrigin.CROS)
        ]);
        dropdown.value = destinationOne;
        pointerDown(dropdown.$$('#dropdownInput'));

        move(getList()[1], {x: 0, y: 0}, {x: 0, y: 0}, 1);
        assertEquals('Two', getHighlightedElementText());
        move(getList()[2], {x: 0, y: 0}, {x: 0, y: 0}, 1);
        assertEquals('Three', getHighlightedElementText());

        // Interacting with the keyboard should update the highlighted element.
        up();
        assertEquals('Two', getHighlightedElementText());

        // When the user moves the mouse again, the highlighted element should
        // change.
        move(getList()[0], {x: 0, y: 0}, {x: 0, y: 0}, 1);
        assertEquals('One', getHighlightedElementText());
      });

  test(assert(destination_dropdown_cros_test.TestNames.Disabled), function() {
    const destinationOne = createDestination('One', DestinationOrigin.CROS);
    setItemList([destinationOne]);
    dropdown.value = destinationOne;
    dropdown.disabled = true;

    pointerDown(dropdown.$$('#dropdownInput'));
    assertFalse(dropdown.$$('iron-dropdown').opened);

    dropdown.disabled = false;
    pointerDown(dropdown.$$('#dropdownInput'));
    assertTrue(dropdown.$$('iron-dropdown').opened);
  });

  test(
      assert(destination_dropdown_cros_test.TestNames.HiddenDestinationBadge),
      function() {
        setItemList([
          createDestination('One', DestinationOrigin.CROS),
          createDestination('Two', DestinationOrigin.PRIVET)
        ]);

        // A DestinationOrigin.CROS printer destination.
        dropdown.value = dropdown.itemList[0];
        assertFalse(dropdown.$$('#destination-badge').hidden);
        assertTrue(dropdown.$$('iron-icon').hidden);

        // A non-local printer destination that should not have a printer status
        // icon.
        dropdown.value = dropdown.itemList[1];
        assertTrue(dropdown.$$('#destination-badge').hidden);
        assertFalse(dropdown.$$('iron-icon').hidden);
      });

  test(
      assert(destination_dropdown_cros_test.TestNames
                 .NewStatusUpdatesDestinationIcon),
      function() {
        const destinationBadge = dropdown.$$('#destination-badge');
        dropdown.value = createDestination('One', DestinationOrigin.CROS);

        dropdown.value.printerStatusReason = PrinterStatusReason.NO_ERROR;
        dropdown.notifyPath(`value.printerStatusReason`);
        assertEquals(PrinterState.GOOD, destinationBadge.state);

        dropdown.value.printerStatusReason = PrinterStatusReason.OUT_OF_INK;
        dropdown.notifyPath(`value.printerStatusReason`);
        assertEquals(PrinterState.ERROR, destinationBadge.state);

        dropdown.value.printerStatusReason = PrinterStatusReason.UNKNOWN_REASON;
        dropdown.notifyPath(`value.printerStatusReason`);
        assertEquals(PrinterState.UNKNOWN, destinationBadge.state);
      });

  test(
      assert(destination_dropdown_cros_test.TestNames
                 .ChangingDestinationUpdatesIcon),
      function() {
        const goodDestination =
            createDestination('One', DestinationOrigin.CROS);
        goodDestination.printerStatusReason = PrinterStatusReason.NO_ERROR;
        const errorDestination =
            createDestination('Two', DestinationOrigin.CROS);
        errorDestination.printerStatusReason = PrinterStatusReason.OUT_OF_INK;
        const unknownDestination =
            createDestination('Three', DestinationOrigin.CROS);
        unknownDestination.printerStatusReason =
            PrinterStatusReason.UNKNOWN_REASON;
        const destinationBadge = dropdown.$$('#destination-badge');

        dropdown.value = goodDestination;
        assertEquals(PrinterState.GOOD, destinationBadge.state);

        dropdown.value = errorDestination;
        assertEquals(PrinterState.ERROR, destinationBadge.state);

        dropdown.value = unknownDestination;
        assertEquals(PrinterState.UNKNOWN, destinationBadge.state);
      });

  test(
      assert(destination_dropdown_cros_test.TestNames.HighlightedWhenOpened),
      function() {
        const destinationTwo = createDestination('Two', DestinationOrigin.CROS);
        const destinationThree =
            createDestination('Three', DestinationOrigin.CROS);
        setItemList([
          createDestination('One', DestinationOrigin.CROS),
          destinationTwo,
          destinationThree,
        ]);

        dropdown.value = destinationTwo;
        pointerDown(dropdown.$$('#dropdownInput'));
        assertEquals('Two', getHighlightedElementText());
        pointerDown(dropdown.$$('#dropdownInput'));

        dropdown.value = destinationThree;
        pointerDown(dropdown.$$('#dropdownInput'));
        assertEquals('Three', getHighlightedElementText());
      });
});
