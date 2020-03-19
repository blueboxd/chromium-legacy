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
  td[2].textContent = convertMojoTimeToJS(rowInfo.lastDiscoveryTime).toString();

  return document.importNode(template.content, true);
}

/**
 * Converts a mojo time to a JS time.
 * @param {!mojoBase.mojom.Time} mojoTime
 * @return {Date}
 */
function convertMojoTimeToJS(mojoTime) {
  // The new Date().getTime() returns the number of milliseconds since the
  // UNIX epoch (1970-01-01 00::00:00 UTC), while |internalValue| of the
  // device.mojom.Geoposition represents the value of microseconds since the
  // Windows FILETIME epoch (1601-01-01 00:00:00 UTC). So add the delta when
  // sets the |internalValue|. See more info in //base/time/time.h.
  const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
  const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
  // |epochDeltaInMs| equals to base::Time::kTimeTToMicrosecondsOffset.
  const epochDeltaInMs = unixEpoch - windowsEpoch;
  const timeInMs = Number(mojoTime.internalValue) / 1000;

  return new Date(timeInMs - epochDeltaInMs);
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
  } else if (sortKey == 'lastDiscoveryTime') {
    return (
        a.lastDiscoveryTime.internalValue > b.lastDiscoveryTime.internalValue);
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
