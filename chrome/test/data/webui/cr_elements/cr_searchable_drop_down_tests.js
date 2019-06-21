// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('cr-searchable-drop-down', function() {
  let dropDown;

  /**
   * @param {!Array<string>} items The list of items to be populated in the
   *   drop down.
   */
  function setItems(items) {
    dropDown.items = items;
    Polymer.dom.flush();
  }

  /** @return {!NodeList} */
  function getList() {
    return dropDown.shadowRoot.querySelectorAll('.list-item');
  }

  /**
   * @param {string} searchTerm The string used to filter the list of items in
   *  the drop down.
   */
  function search(searchTerm) {
    let input = dropDown.shadowRoot.querySelector('cr-input');
    input.value = searchTerm;
    input.fire('input');
    Polymer.dom.flush();
  }

  setup(function() {
    PolymerTest.clearBody();
    document.body.innerHTML = `
      <cr-searchable-drop-down label="test drop down">
      </cr-searchable-drop-down>
    `;
    dropDown = document.querySelector('cr-searchable-drop-down');
    Polymer.dom.flush();
  });

  test('correct list items', function() {
    setItems(['one', 'two', 'three']);

    let itemList = getList();

    assertEquals(3, itemList.length);
    assertEquals('one', itemList[0].textContent);
    assertEquals('two', itemList[1].textContent);
    assertEquals('three', itemList[2].textContent);
  });

  test('filter works correctly', function() {
    setItems(['cat', 'hat', 'rat', 'rake']);

    search('c');
    assertEquals(1, getList().length);
    assertEquals('cat', getList()[0].textContent);

    search('at');
    assertEquals(3, getList().length);
    assertEquals('cat', getList()[0].textContent);
    assertEquals('hat', getList()[1].textContent);
    assertEquals('rat', getList()[2].textContent);

    search('ra');
    assertEquals(2, getList().length);
    assertEquals('rat', getList()[0].textContent);
    assertEquals('rake', getList()[1].textContent);
  });

  test('value is set on click', function() {
    setItems(['dog', 'cat', 'mouse']);

    assertNotEquals('dog', dropDown.value);

    getList()[0].click();

    assertEquals('dog', dropDown.value);

    // Make sure final value does not change while searching.
    search('ta');
    assertEquals('dog', dropDown.value);
  });

  // If the update-value-on-input flag is passed, final value should be whatever
  // is in the search box.
  test('value is set on click and on search', function() {
    dropDown.updateValueOnInput = true;
    setItems(['dog', 'cat', 'mouse']);

    assertNotEquals('dog', dropDown.value);

    getList()[0].click();

    assertEquals('dog', dropDown.value);

    // Make sure final value does change while searching.
    search('ta');
    assertEquals('ta', dropDown.value);
  });

  // If the error-message-allowed flag is passed and the |errorMessage| property
  // is set, then the error message should be displayed. If the |errorMessage|
  // property is not set or |errorMessageAllowed| is false, no error message
  // should be displayed.
  test('error message is displayed if set and allowed', function() {
    dropDown.errorMessageAllowed = true;
    dropDown.errorMessage = 'error message';

    const input = dropDown.$.search;

    assertEquals(dropDown.errorMessage, input.$.error.textContent);
    assertTrue(input.invalid);

    // Set |errorMessageAllowed| to false and verify no error message is shown.
    dropDown.errorMessageAllowed = false;

    assertNotEquals(dropDown.errorMessage, input.$.error.textContent);
    assertFalse(input.invalid);

    // Set |errorMessageAllowed| to true and verify it is displayed again.
    dropDown.errorMessageAllowed = true;

    assertEquals(dropDown.errorMessage, input.$.error.textContent);
    assertTrue(input.invalid);

    // Clearing |errorMessage| hides the error.
    dropDown.errorMessage = '';

    assertEquals(dropDown.errorMessage, input.$.error.textContent);
    assertFalse(input.invalid);
  });

  // The show-loading attribute should determine whether or not the loading
  // progress bar is shown.
  test('progress bar is shown and hidden', function() {
    const progress = dropDown.$.loading;
    assertTrue(progress.hidden);

    dropDown.showLoading = true;
    assertFalse(progress.hidden);

    dropDown.showLoading = false;
    assertTrue(progress.hidden);
  });


  // The readonly attribute is passed through to the inner cr-input.
  test('readonly attribute', function() {
    const input = dropDown.shadowRoot.querySelector('cr-input');

    dropDown.readonly = true;
    assertTrue(input.readonly);

    dropDown.readonly = false;
    assertFalse(input.readonly);
  });
});
