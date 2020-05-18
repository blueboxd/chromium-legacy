// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Checks that a correct sharing action source is extracted from an event.
 */
function testGetSharingActionSource() {
  const testData = [
    {
      event: {target: {id: CommandUtil.SharingActionElementId.CONTEXT_MENU}},
      expected: FileTasks.SharingActionSourceForUMA.CONTEXT_MENU,
    },
    {
      event: {target: {id: CommandUtil.SharingActionElementId.SHARE_BUTTON}},
      expected: FileTasks.SharingActionSourceForUMA.SHARE_BUTTON,
    },
    {
      event: {target: {id: '__no_such_id__'}},
      expected: FileTasks.SharingActionSourceForUMA.UNKNOWN,
    },
    {
      event: {target: {id: null}},
      expected: FileTasks.SharingActionSourceForUMA.UNKNOWN,
    },
  ];
  for (const data of testData) {
    const source = CommandUtil.getSharingActionSource(data.event);
    assertEquals(data.expected, source);
  }
}

