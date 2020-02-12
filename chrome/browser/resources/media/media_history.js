// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Allow a function to be provided by tests, which will be called when
// the page has been populated.
const pageIsPopulatedResolver = new PromiseResolver();
function whenPageIsPopulatedForTest() {
  return pageIsPopulatedResolver.promise;
}

(function() {

let store = null;
let statsTableBody = null;
let originsTable = null;
let playbacksTable = null;

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

  // Compare the url property.
  if (sortKey == 'url') {
    return val1.url > val2.url ? 1 : -1;
  }

  // Compare mojo_base.mojom.TimeDelta microseconds value.
  if (sortKey == 'cachedAudioVideoWatchtime' ||
      sortKey == 'actualAudioVideoWatchtime' || sortKey == 'watchtime') {
    return val1.microseconds - val2.microseconds;
  }

  if (sortKey == 'lastUpdatedTime') {
    return val1 - val2;
  }

  assertNotReached('Unsupported sort key: ' + sortKey);
  return 0;
}

/**
 * Formats a field to be displayed in the data table.
 * @param {?object} data
 * @param {string} key
 * @returns {?string}
 */
function formatField(data, key) {
  if (data === undefined || data === null) {
    return;
  }

  if (key == 'origin') {
    let origin = data.scheme + '://' + data.host;
    if (data.scheme == 'http' && data.port != '80') {
      origin += ':' + data.port;
    } else if (data.scheme == 'https' && data.port != '443') {
      origin += ':' + data.port;
    }

    return origin;
  } else if (key == 'lastUpdatedTime') {
    return data ? new Date(data).toISOString() : '';
  } else if (
      key == 'cachedAudioVideoWatchtime' ||
      key == 'actualAudioVideoWatchtime' || key == 'watchtime') {
    const secs = (data.microseconds / 1000000);
    return secs.toString().replace(/(\d)(?=(\d{3})+(?!\d))/g, '$1,');
  } else if (key == 'url') {
    return data.url;
  } else if (key == 'hasAudio' || key == 'hasVideo') {
    return data ? 'Yes' : 'No';
  }

  return data;
}

class DataTable {
  /**
   * @param {!HTMLTableElement} table
   */
  constructor(table) {
    /** @private {!HTMLTableElement} */
    this.table_ = table;

    /** @private {Object[]} */
    this.data_ = [];

    // Set table header sort handlers.
    const headers = this.table_.querySelectorAll('th[sort-key]');
    headers.forEach((header) => {
      header.addEventListener('click', this.handleSortClick.bind(this));
    }, this);
  }

  handleSortClick(e) {
    const isCurrentSortColumn = e.target.classList.contains('sort-column');

    // If we are not the sort column then we should become the sort column.
    if (!isCurrentSortColumn) {
      const oldSortColumn = document.querySelector('.sort-column');
      oldSortColumn.classList.remove('sort-column');
      e.target.classList.add('sort-column');
    }

    // If we are the current sort column then we should toggle the reverse
    // attribute to sort in reverse.
    if (isCurrentSortColumn && e.target.hasAttribute('sort-reverse')) {
      e.target.removeAttribute('sort-reverse');
    } else {
      e.target.setAttribute('sort-reverse', '');
    }

    this.render();
  }

  render() {
    // Find the body of the table and clear it.
    const body = this.table_.querySelectorAll('tbody')[0];
    body.innerHTML = '';

    // Get the sort key from the columns to determine which data should be in
    // which column.
    const headerCells = Array.from(this.table_.querySelectorAll('thead th'));
    const sortKeys = headerCells.map((e) => e.getAttribute('sort-key'));

    const currentSortCol = this.table_.querySelectorAll('.sort-column')[0];
    const currentSortKey = currentSortCol.getAttribute('sort-key');
    const currentSortReverse = currentSortCol.hasAttribute('sort-reverse');

    // Sort the data based on the current sort key.
    this.data_.sort((a, b) => {
      return (currentSortReverse ? -1 : 1) *
          compareTableItem(currentSortKey, a, b);
    });

    // Generate the table rows.
    this.data_.forEach((dataRow) => {
      const tr = document.createElement('tr');
      body.appendChild(tr);

      sortKeys.forEach((key) => {
        const td = document.createElement('td');
        td.textContent = formatField(dataRow[key], key);
        tr.appendChild(td);
      });
    });
  }

  /**
   * @param {object[]} data The data to update
   */
  setData(data) {
    this.data_ = data;
    this.render();
  }
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
 * @param {!string} name The name of the tab to show.
 * @return {Promise}
 */
function showTab(name) {
  switch (name) {
    case 'stats':
      return store.getMediaHistoryStats().then(response => {
        renderStatsTable(response.stats);
      });
    case 'origins':
      return store.getMediaHistoryOriginRows().then(response => {
        originsTable.setData(response.rows);
      });
    case 'playbacks':
      return store.getMediaHistoryPlaybackRows().then(response => {
        playbacksTable.setData(response.rows);
      });
  }

  // Return an empty promise if there is no tab.
  return new Promise();
}

document.addEventListener('DOMContentLoaded', function() {
  store = mediaHistory.mojom.MediaHistoryStore.getRemote();

  statsTableBody = $('stats-table-body');

  originsTable = new DataTable($('origins-table'));
  playbacksTable = new DataTable($('playbacks-table'));

  cr.ui.decorate('tabbox', cr.ui.TabBox);

  // Allow tabs to be navigated to by fragment. The fragment with be of the
  // format "#tab-<tab id>".
  window.onhashchange = function() {
    showTab(window.location.hash.substr(5));
  };

  // Default to the stats tab.
  if (!window.location.hash.substr(5)) {
    window.location.hash = 'tab-stats';
  } else {
    showTab(window.location.hash.substr(5))
        .then(pageIsPopulatedResolver.resolve);
  }

  // When the tab updates, update the anchor.
  $('tabbox').addEventListener('selectedChange', function() {
    const tabbox = $('tabbox');
    const tabs = tabbox.querySelector('tabs').children;
    const selectedTab = tabs[tabbox.selectedIndex];
    window.location.hash = 'tab-' + selectedTab.id;
  }, true);

  // Add handler to 'copy all to clipboard' button.
  const copyAllToClipboardButtons =
      document.querySelectorAll('.copy-all-to-clipboard');

  copyAllToClipboardButtons.forEach((button) => {
    button.addEventListener('click', (e) => {
      // Make sure nothing is selected.
      window.getSelection().removeAllRanges();

      document.execCommand('selectAll');
      document.execCommand('copy');

      // And deselect everything at the end.
      window.getSelection().removeAllRanges();
    });
  });
});
})();
