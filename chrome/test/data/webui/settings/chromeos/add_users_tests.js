// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('AddPersonDialog', function() {
  let dialog = null;

  setup(function() {
    PolymerTest.clearBody();

    dialog = document.createElement('settings-users-add-user-dialog');

    document.body.appendChild(dialog);

    dialog.open();
  });

  teardown(function() {
    dialog.remove();
    dialog = null;
  });


  /**
   * Test that the dialog reacts to valid and invalid input correctly.
   */
  test('Add user', function() {
    const userInputBox = dialog.$$('#addUserInput');
    assertTrue(!!userInputBox);

    const addButton = dialog.$$('.action-button');
    assertTrue(!!addButton);
    assertTrue(addButton.disabled);
    assertTrue(!userInputBox.invalid);

    // Try to add a valid username without domain
    userInputBox.value = 'abcdef';
    assertTrue(!addButton.disabled);
    assertTrue(!userInputBox.invalid);

    // Try to add a valid username with domain
    userInputBox.value = 'abcdef@xyz.com';
    assertTrue(!addButton.disabled);
    assertTrue(!userInputBox.invalid);

    // Try to add an invalid username
    userInputBox.value = 'abcdef@';
    assertTrue(addButton.disabled);
    assertTrue(userInputBox.invalid);
  });
});
