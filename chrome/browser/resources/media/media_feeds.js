// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(https://crbug.com/1059352): Factor out the sortable table.

'use strict';

// Allow a function to be provided by tests, which will be called when
// the page has been populated with media feeds details.
const pageIsPopulatedResolver = new PromiseResolver();
function whenPageIsPopulatedForTest() {
  return pageIsPopulatedResolver.promise;
}

(function() {

let detailsProvider = null;
let info = null;
let feedTableBody = null;
let sortReverse = true;
let sortKey = 'id';

/**
 * Creates a single row in the feeds table.
 * @param {!mediaFeeds.mojom.MediaFeed} rowInfo The info to create the row.
 * @return {!Node}
 */
function createRow(rowInfo) {
  const template = $('datarow');
  const td = template.content.querySelectorAll('td');

  td[0].textContent = rowInfo.id;
  td[1].textContent = rowInfo.url.url;

  return document.importNode(template.content, true);
}

/**
 * Remove all rows from the feeds table.
 */
function clearTable() {
  feedTableBody.innerHTML = '';
}

/**
 * Sort the feed info based on |sortKey| and |sortReverse|.
 */
function sortInfo() {
  info.sort((a, b) => {
    return (sortReverse ? -1 : 1) * compareTableItem(sortKey, a, b);
  });
}

/**
 * Compares two MediaFeed objects based on |sortKey|.
 * @param {string} sortKey The name of the property to sort by.
 * @param {mediaFeeds.mojom.MediaFeed} a First object to compare.
 * @param {mediaFeeds.mojom.MediaFeed} b Second object to compare.
 * @return {number|boolean} A negative number if |a| should be ordered before
 *     |b|, a positive number otherwise.
 */
function compareTableItem(sortKey, a, b) {
  if (sortKey == 'url') {
    return a.url.url > b.url.url ? 1 : -1;
  } else if (sortKey == 'id') {
    return a.id > b.id;
  }

  assertNotReached('Unsupported sort key: ' + sortKey);
  return 0;
}

/**
 * Regenerates the feed table from |info|.
 */
function renderTable() {
  clearTable();
  sortInfo();
  info.forEach(rowInfo => feedTableBody.appendChild(createRow(rowInfo)));
}

/**
 * Retrieve feed info and render the feed table.
 */
function updateFeedTable() {
  detailsProvider.getMediaFeeds().then(response => {
    info = response.feeds;
    renderTable();
    pageIsPopulatedResolver.resolve();
  });
}

document.addEventListener('DOMContentLoaded', () => {
  detailsProvider = mediaFeeds.mojom.MediaFeedsStore.getRemote();

  feedTableBody = $('feed-table-body');
  updateFeedTable();

  // Set table header sort handlers.
  const feedTableHeader = $('feed-table-header');
  const headers = feedTableHeader.children;
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
      renderTable();
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
