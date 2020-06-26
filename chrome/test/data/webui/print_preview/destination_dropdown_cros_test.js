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
  ClickRemovesSelected: 'click removes selected',
  ClickCloses: 'click closes dropdown',
  TabCloses: 'tab closes dropdown',
  SelectedAfterUpDown: 'selected after keyboard press up and down',
  EnterOpensCloses: 'enter opens and closes dropdown',
  SelectedFollowsMouse: 'selected follows mouse',
  Disabled: 'disabled',
  HiddenDestinationBadge: 'hidden destination badge',
  NewStatusUpdatesDestinationIcon: 'new status updates destination icon',
  ChangingDestinationUpdatesIcon: 'changing destination updates icon',
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
  function getSelectedElement() {
    return dropdown.$$('[selected_]');
  }

  /** @return {string} */
  function getSelectedElementText() {
    return getSelectedElement().textContent.trim();
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
      assert(destination_dropdown_cros_test.TestNames.ClickRemovesSelected),
      function() {
        setItemList([
          createDestination('One', DestinationOrigin.CROS),
          createDestination('Two', DestinationOrigin.CROS)
        ]);

        getList()[1].setAttribute('selected_', '');
        assertTrue(getList()[1].hasAttribute('selected_'));

        getList()[1].click();
        assertFalse(getList()[1].hasAttribute('selected_'));
      });

  test(
      assert(destination_dropdown_cros_test.TestNames.ClickCloses), function() {
        setItemList([createDestination('One', DestinationOrigin.CROS)]);
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
    setItemList([createDestination('One', DestinationOrigin.CROS)]);
    const ironDropdown = dropdown.$$('iron-dropdown');

    pointerDown(dropdown.$$('#dropdownInput'));
    assertTrue(ironDropdown.opened);

    tab();
    assertFalse(ironDropdown.opened);
  });

  test(
      assert(destination_dropdown_cros_test.TestNames.SelectedAfterUpDown),
      function() {
        setItemList([createDestination('One', DestinationOrigin.CROS)]);

        pointerDown(dropdown.$$('#dropdownInput'));

        down();
        assertEquals('One', getSelectedElementText());
        down();
        assertEquals('Save as PDF', getSelectedElementText());
        down();
        assertEquals('Save to Google Drive', getSelectedElementText());
        down();
        assertEquals('See more…', getSelectedElementText());
        down();
        assertEquals('One', getSelectedElementText());

        up();
        assertEquals('See more…', getSelectedElementText());
        up();
        assertEquals('Save to Google Drive', getSelectedElementText());
        up();
        assertEquals('Save as PDF', getSelectedElementText());
        up();
        assertEquals('One', getSelectedElementText());
      });

  test(
      assert(destination_dropdown_cros_test.TestNames.EnterOpensCloses),
      function() {
        setItemList([createDestination('One', DestinationOrigin.CROS)]);

        assertFalse(dropdown.$$('iron-dropdown').opened);
        enter();
        assertTrue(dropdown.$$('iron-dropdown').opened);
        enter();
        assertFalse(dropdown.$$('iron-dropdown').opened);
      });

  test(
      assert(destination_dropdown_cros_test.TestNames.SelectedFollowsMouse),
      function() {
        setItemList([
          createDestination('One', DestinationOrigin.CROS),
          createDestination('Two', DestinationOrigin.CROS),
          createDestination('Three', DestinationOrigin.CROS)
        ]);

        pointerDown(dropdown.$$('#dropdownInput'));

        move(getList()[1], {x: 0, y: 0}, {x: 0, y: 0}, 1);
        assertEquals('Two', getSelectedElementText());
        move(getList()[2], {x: 0, y: 0}, {x: 0, y: 0}, 1);
        assertEquals('Three', getSelectedElementText());

        // Interacting with the keyboard should update the selected element.
        up();
        assertEquals('Two', getSelectedElementText());

        // When the user moves the mouse again, the selected element should
        // change.
        move(getList()[0], {x: 0, y: 0}, {x: 0, y: 0}, 1);
        assertEquals('One', getSelectedElementText());
      });

  test(assert(destination_dropdown_cros_test.TestNames.Disabled), function() {
    setItemList([createDestination('One', DestinationOrigin.CROS)]);
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
});
