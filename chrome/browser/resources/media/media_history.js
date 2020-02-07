// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Allow a function to be provided by tests, which will be called when
// the page has been populated with media history details.
const pageIsPopulatedResolver = new PromiseResolver();
let pageIsPopulatedCounter = 0;
function whenPageIsPopulatedForTest() {
  return pageIsPopulatedResolver.promise;
}

function maybeResolvePageIsPopulated() {
  pageIsPopulatedCounter--;

  if (pageIsPopulatedCounter == 0) {
    pageIsPopulatedResolver.resolve();
  }
}

(function() {

let data = null;
let dataTableBody = null;
let sortReverse = true;
let sortKey = 'cachedAudioVideoWatchtime';
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
 * Remove all rows from the data table.
 */
function clearDataTable() {
  dataTableBody.innerHTML = '';
}

/**
 * Compares two MediaHistoryOriginRow objects based on |sortKey|.
 * @param {string} sortKey The name of the property to sort by.
 * @param {number|mojo_base.mojom.TimeDelta|url.mojom.Origin} The first object
 *     to compare.
 * @param {number|mojo_base.mojom.TimeDelta|url.mojom.Origin} The second object
 *     to compare.
 * @return {number} A negative number if |a| should be ordered before
 *     |b|, a positive number otherwise.
 */
function compareTableItem(sortKey, a, b) {
  const val1 = a[sortKey];
  const val2 = b[sortKey];

  // Compare the hosts of the origin ignoring schemes.
  if (sortKey == 'origin') {
    return val1.host > val2.host ? 1 : -1;
  }

  // Compare mojo_base.mojom.TimeDelta microseconds value.
  if (sortKey == 'cachedAudioVideoWatchtime' ||
      sortKey == 'actualAudioVideoWatchtime') {
    return val1.microseconds - val2.microseconds;
  }

  if (sortKey == 'lastUpdatedTime') {
    return val1 - val2;
  }

  assertNotReached('Unsupported sort key: ' + sortKey);
  return 0;
}

/**
 * Sort the data based on |sortKey| and |sortReverse|.
 */
function sortData() {
  data.sort((a, b) => {
    return (sortReverse ? -1 : 1) * compareTableItem(sortKey, a, b);
  });
}

/**
 * Regenerates the data table from |data|.
 */
function renderDataTable() {
  clearDataTable();
  sortData();
  data.forEach(rowInfo => dataTableBody.appendChild(createDataRow(rowInfo)));
}

/**
 * @param {mojo_base.mojom.TimeDelta} The time delta to format.
 * @return {string} The time in seconds with commas added.
 */
function formatWatchtime(time) {
  const secs = (time.microseconds / 1000000);
  return secs.toString().replace(/(\d)(?=(\d{3})+(?!\d))/g, '$1,');
}

/**
 * Creates a single row in the data table.
 * @param {!MediaHistoryOriginRow} data The data to create the row.
 * @return {!HTMLElement}
 */
function createDataRow(data) {
  const template = $('data-row');
  const td = template.content.querySelectorAll('td');

  td[0].textContent = data.origin.scheme + '://' + data.origin.host;
  if (data.origin.scheme == 'http' && data.origin.port != '80') {
    td[0].textContent += ':' + data.origin.port;
  } else if (data.origin.scheme == 'https' && data.origin.port != '443') {
    td[0].textContent += ':' + data.origin.port;
  }

  td[1].textContent =
      data.lastUpdatedTime ? new Date(data.lastUpdatedTime).toISOString() : '';
  td[2].textContent = formatWatchtime(data.cachedAudioVideoWatchtime);
  td[3].textContent = formatWatchtime(data.actualAudioVideoWatchtime);
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
  pageIsPopulatedCounter += 2;

  // Populate stats table.
  store.getMediaHistoryStats().then(response => {
    renderStatsTable(response.stats);
    maybeResolvePageIsPopulated();
  });

  // Populate origin table.
  store.getMediaHistoryOriginRows().then(response => {
    data = response.rows;
    renderDataTable();
    maybeResolvePageIsPopulated();
  });
}

document.addEventListener('DOMContentLoaded', function() {
  store = mediaHistory.mojom.MediaHistoryStore.getRemote();

  dataTableBody = $('data-table-body');
  statsTableBody = $('stats-table-body');
  updateTable();

  // Set table header sort handlers.
  const dataTableHeader = $('data-table-header');
  const headers = dataTableHeader.children;
  for (let i = 0; i < headers.length; i++) {
    headers[i].addEventListener('click', (e) => {
      const newSortKey = e.target.getAttribute('sort-key');
      if (sortKey == newSortKey) {
        sortReverse = !sortReverse;
      } else {
        sortKey = newSortKey;
        sortReverse = false;
      }
      const oldSortColumn = document.querySelector('.sort-column');
      oldSortColumn.classList.remove('sort-column');
      e.target.classList.add('sort-column');
      if (sortReverse) {
        e.target.setAttribute('sort-reverse', '');
      } else {
        e.target.removeAttribute('sort-reverse');
      }
      renderDataTable();
    });
  }

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
