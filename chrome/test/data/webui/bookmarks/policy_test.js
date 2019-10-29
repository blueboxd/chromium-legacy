// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('Bookmarks policies', function() {
  let store;
  let app;

  setup(function() {
    const nodes = testTree(createFolder('1', [
      createItem('11'),
    ]));
    store = new bookmarks.TestStore({
      nodes: nodes,
      folderOpenState: getAllFoldersOpenState(nodes),
      selectedFolder: '1',
    });
    store.setReducersEnabled(true);
    store.expectAction('set-incognito-availability');
    store.expectAction('set-can-edit');
    store.replaceSingleton();

    app = document.createElement('bookmarks-app');
    replaceBody(app);
  });

  test('incognito availability updates when changed', async function() {
    const commandManager = bookmarks.CommandManager.getInstance();
    // Incognito is disabled during testGenPreamble(). Wait for the front-end to
    // load the config.
    const action = await store.waitForAction('set-incognito-availability');
    assertEquals(
        IncognitoAvailability.DISABLED, store.data.prefs.incognitoAvailability);
    assertFalse(
        commandManager.canExecute(Command.OPEN_INCOGNITO, new Set(['11'])));

    await cr.sendWithPromise('testSetIncognito', IncognitoAvailability.ENABLED);
    assertEquals(
        IncognitoAvailability.ENABLED, store.data.prefs.incognitoAvailability);
    assertTrue(
        commandManager.canExecute(Command.OPEN_INCOGNITO, new Set(['11'])));
  });

  test('canEdit updates when changed', async function() {
    const commandManager = bookmarks.CommandManager.getInstance();
    const action = await store.waitForAction('set-can-edit');
    assertFalse(store.data.prefs.canEdit);
    assertFalse(commandManager.canExecute(Command.DELETE, new Set(['11'])));

    await cr.sendWithPromise('testSetCanEdit', true);
    assertTrue(store.data.prefs.canEdit);
    assertTrue(commandManager.canExecute(Command.DELETE, new Set(['11'])));
  });
});
