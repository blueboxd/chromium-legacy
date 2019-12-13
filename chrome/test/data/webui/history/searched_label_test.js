// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('<history-searched-label> unit test', function() {
  /** @type {?HistorySearchedLabelElement} */
  let label;

  setup(function() {
    PolymerTest.clearBody();
    label = document.createElement('history-searched-label');
    document.body.appendChild(label);
  });

  test('matching query sets bold', function() {
    assertEquals(0, document.querySelectorAll('b').length);
    // Note: When the page is reloaded with a search query, |searchTerm| will be
    // initialized before |title|. Keep this ordering as a regression test for
    // https://crbug.com/921455.
    label.searchTerm = 'f';
    label.title = 'foo';

    Polymer.dom.flush();
    const boldItems = document.querySelectorAll('b');
    assertEquals(1, boldItems.length);
    assertEquals(label.searchTerm, boldItems[0].textContent);

    label.searchTerm = 'g';
    Polymer.dom.flush();
    assertEquals(0, document.querySelectorAll('b').length);
  });
});
