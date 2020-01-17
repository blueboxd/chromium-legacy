// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Allow a function to be provided by tests, which will be called when
// the page has been populated with media history details.
const pageIsPopulatedResolver = new PromiseResolver();
function whenPageIsPopulatedForTest() {
  return pageIsPopulatedResolver.promise;
}

(function() {

let store = null;
let statsTableBody = null;

/**
 * Creates a single row in the stats table.
 * @param {string} name The name of the table.
 * @param {string} count The row count of the table.
 * @return {!HTMLElement}
 */
function createStatsRow(name, count) {
  const template = $('stats-row');
  const td = template.content.querySelectorAll('td');
  td[0].textContent = name;
  td[1].textContent = count;
  return document.importNode(template.content, true);
}

/**
 * Regenerates the stats table.
 * @param {!MediaHistoryStats} stats The stats for the Media History store.
 */

function renderStatsTable(stats) {
  statsTableBody.innerHTML = '';

  Object.keys(stats.tableRowCounts).forEach((key) => {
    statsTableBody.appendChild(createStatsRow(key, stats.tableRowCounts[key]));
  });
}

/**
 * Retrieve stats from the backend and then render the table.
 */
function updateTable() {
  store.getMediaHistoryStats().then(response => {
    renderStatsTable(response.stats);
    pageIsPopulatedResolver.resolve();
  });
}

document.addEventListener('DOMContentLoaded', function() {
  store = mediaHistory.mojom.MediaHistoryStore.getRemote(
      /*useBrowserInterfaceBroker=*/ true);

  statsTableBody = $('stats-table-body');
  updateTable();

  // Add handler to 'copy all to clipboard' button
  const copyAllToClipboardButton = $('copy-all-to-clipboard');
  copyAllToClipboardButton.addEventListener('click', (e) => {
    // Make sure nothing is selected
    window.getSelection().removeAllRanges();

    document.execCommand('selectAll');
    document.execCommand('copy');

    // And deselect everything at the end.
    window.getSelection().removeAllRanges();
  });
});
})();
