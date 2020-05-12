// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Utility function that appends value under a given name in the store.
 * @param {!Map<string, !Array<string|number>>} store
 * @param {string} name
 * @param {string|number} value
 */
function record(store, name, value) {
  let recorded = store.get(name);
  if (!recorded) {
    recorded = [];
    store.set(name, recorded);
  }
  recorded.push(value);
}

/**
 * Checks that a correct sharing action source is extracted from an event.
 */
function testGetSharingActionSource() {
  const testData = [
    {
      event: {target: {id: CommandUtil.SharingActionElementId.CONTEXT_MENU}},
      expected: CommandUtil.SharingActionSourceForUMA.CONTEXT_MENU,
    },
    {
      event: {target: {id: CommandUtil.SharingActionElementId.SHARE_BUTTON}},
      expected: CommandUtil.SharingActionSourceForUMA.SHARE_BUTTON,
    },
    {
      event: {target: {id: '__no_such_id__'}},
      expected: CommandUtil.SharingActionSourceForUMA.UNKNOWN,
    },
    {
      event: {target: {id: null}},
      expected: CommandUtil.SharingActionSourceForUMA.UNKNOWN,
    },
  ];
  for (const data of testData) {
    const source = CommandUtil.getSharingActionSource(data.event);
    assertEquals(data.expected, source);
  }
}

/**
 * Checks that we are correctly recording UMA about Share action.
 */
function testReportSharingAction() {
  // Setup: create a fake metrics object that can be examined for content.
  const enumMap = new Map();
  const countMap = new Map();
  window.metrics = {
    recordEnum: (name, value, valid) => {
      assertTrue(valid.includes(value));
      record(enumMap, name, value);
    },
    recordSmallCount: (name, value) => {
      record(countMap, name, value);
    },
  };
  const mockFileSystem = new MockFileSystem('volumeId');

  // Actual tests.
  CommandHandler.recordSharingAction(
      /** @type {!Event} */ (
          {target: {id: CommandUtil.SharingActionElementId.CONTEXT_MENU}}),
      [
        MockFileEntry.create(mockFileSystem, '/test.log'),
        MockFileEntry.create(mockFileSystem, '/test.doc'),
        MockFileEntry.create(mockFileSystem, '/test.__no_such_extension__'),
      ]);
  assertArrayEquals(
      enumMap.get('Share.ActionSource'),
      [CommandUtil.SharingActionSourceForUMA.CONTEXT_MENU]);
  assertArrayEquals(countMap.get('Share.FileCount'), [3]);
  assertArrayEquals(enumMap.get('Share.FileType'), ['.log', '.doc', 'other']);

  CommandHandler.recordSharingAction(
      /** @type {!Event} */ (
          {target: {id: CommandUtil.SharingActionElementId.SHARE_BUTTON}}),
      [
        MockFileEntry.create(mockFileSystem, '/test.log'),
      ]);
  assertArrayEquals(enumMap.get('Share.ActionSource'), [
    CommandUtil.SharingActionSourceForUMA.CONTEXT_MENU,
    CommandUtil.SharingActionSourceForUMA.SHARE_BUTTON,
  ]);
  assertArrayEquals(countMap.get('Share.FileCount'), [3, 1]);
  assertArrayEquals(
      enumMap.get('Share.FileType'), ['.log', '.doc', 'other', '.log']);
}
