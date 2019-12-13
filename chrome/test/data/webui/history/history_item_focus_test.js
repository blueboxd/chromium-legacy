// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<history-item> focus test', function() {
  let item;

  setup(function() {
    PolymerTest.clearBody();
    history.BrowserService.instance_ = new TestBrowserService();

    item = document.createElement('history-item');
    item.item = createHistoryEntry('2016-03-16 10:00', 'http://www.google.com');
    document.body.appendChild(item);
    return test_util.waitAfterNextRender(item);
  });

  test('refocus checkbox on click', async () => {
    await test_util.flushTasks();
    item.$['menu-button'].focus();
    assertEquals(item.$['menu-button'], item.root.activeElement);

    const whenCheckboxSelected =
        test_util.eventToPromise('history-checkbox-select', item);
    item.$['time-accessed'].click();

    await whenCheckboxSelected;
    assertEquals(item.$['checkbox'], item.root.activeElement);
  });
});
