// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationConnectionStatus, DestinationOrigin, DestinationType} from 'chrome://print/print_preview.js';
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
   * @return {!Destination}
   */
  function createDestination(displayName) {
    return new Destination(
        displayName, DestinationType.LOCAL, DestinationOrigin.LOCAL,
        displayName, DestinationConnectionStatus.ONLINE);
  }

  /** @override */
  setup(function() {
    document.body.innerHTML = '';

    dropdown =
        /** @type {!PrintPreviewDestinationDropdownCrosElement} */
        (document.createElement('print-preview-destination-dropdown-cros'));
    document.body.appendChild(dropdown);
  });

  test(
      assert(destination_dropdown_cros_test.TestNames.CorrectListItems),
      function() {
        setItemList([
          createDestination('One'), createDestination('Two'),
          createDestination('Three')
        ]);

        const itemList = getList();
        assertEquals(3, itemList.length);
        assertEquals('One', itemList[0].textContent.trim());
        assertEquals('Two', itemList[1].textContent.trim());
        assertEquals('Three', itemList[2].textContent.trim());
      });

  test(
      assert(destination_dropdown_cros_test.TestNames.ClickRemovesSelected),
      function() {
        setItemList([createDestination('One'), createDestination('Two')]);
        dropdown.destination = createDestination('One');

        getList()[1].setAttribute('selected_', '');
        assertTrue(getList()[1].hasAttribute('selected_'));

        getList()[1].click();
        assertFalse(getList()[1].hasAttribute('selected_'));
      });

  test(
      assert(destination_dropdown_cros_test.TestNames.ClickCloses), function() {
        setItemList([createDestination('One')]);
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
    setItemList([createDestination('One')]);
    const ironDropdown = dropdown.$$('iron-dropdown');

    pointerDown(dropdown.$$('#dropdownInput'));
    assertTrue(ironDropdown.opened);

    tab();
    assertFalse(ironDropdown.opened);
  });

  test(
      assert(destination_dropdown_cros_test.TestNames.SelectedAfterUpDown),
      function() {
        setItemList([
          createDestination('One'), createDestination('Two'),
          createDestination('Three')
        ]);

        pointerDown(dropdown.$$('#dropdownInput'));

        down();
        assertEquals('One', getSelectedElementText());
        down();
        assertEquals('Two', getSelectedElementText());
        down();
        assertEquals('Three', getSelectedElementText());
        down();
        assertEquals('One', getSelectedElementText());

        up();
        assertEquals('Three', getSelectedElementText());
        up();
        assertEquals('Two', getSelectedElementText());
        up();
        assertEquals('One', getSelectedElementText());
        up();
        assertEquals('Three', getSelectedElementText());
      });

  test(
      assert(destination_dropdown_cros_test.TestNames.EnterOpensCloses),
      function() {
        setItemList([createDestination('One')]);

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
          createDestination('One'), createDestination('Two'),
          createDestination('Three')
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
});
