// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Allow a function to be provided by tests, which will be called when
// the page has been populated with media feeds details.
const mediaFeedsPageIsPopulatedResolver = new PromiseResolver();
function whenPageIsPopulatedForTest() {
  return mediaFeedsPageIsPopulatedResolver.promise;
}

(function() {

let delegate = null;
let feedsTable = null;
let store = null;

/** @implements {cr.ui.MediaDataTableDelegate} */
class MediaFeedsTableDelegate {
  /**
   * Formats a field to be displayed in the data table and inserts it into the
   * element.
   * @param {Element} td
   * @param {?Object} data
   * @param {string} key
   */
  insertDataField(td, data, key) {
    if (data === undefined || data === null) {
      return;
    }

    if (key === 'url') {
      // Format a mojo GURL.
      td.textContent = data.url;
    } else if (
        key === 'lastDiscoveryTime' || key === 'lastFetchTime' ||
        key === 'cacheExpiryTime') {
      // Format a mojo time.
      td.textContent =
          convertMojoTimeToJS(/** @type {mojoBase.mojom.Time} */ (data))
              .toString();
    } else if (key === 'userStatus') {
      // Format a FeedUserStatus.
      if (data == mediaFeeds.mojom.FeedUserStatus.kAuto) {
        td.textContent = 'Auto';
      } else if (data == mediaFeeds.mojom.FeedUserStatus.kDisabled) {
        td.textContent = 'Disabled';
      }
    } else if (key === 'lastFetchResult') {
      // Format a FetchResult.
      if (data == mediaFeeds.mojom.FetchResult.kNone) {
        td.textContent = 'None';
      } else if (data == mediaFeeds.mojom.FetchResult.kSuccess) {
        td.textContent = 'Success';
      } else if (data == mediaFeeds.mojom.FetchResult.kFailedBackendError) {
        td.textContent = 'Failed (Backend Error)';
      } else if (data == mediaFeeds.mojom.FetchResult.kFailedNetworkError) {
        td.textContent = 'Failed (Network Error)';
      }
    } else if (key === 'lastFetchContentTypes') {
      // Format a MediaFeedItemType.
      const contentTypes = [];
      const itemType = parseInt(data, 10);

      if (itemType & mediaFeeds.mojom.MediaFeedItemType.kVideo) {
        contentTypes.push('Video');
      } else if (itemType & mediaFeeds.mojom.MediaFeedItemType.kTVSeries) {
        contentTypes.push('TV Series');
      } else if (itemType & mediaFeeds.mojom.MediaFeedItemType.kMovie) {
        contentTypes.push('Movie');
      }

      td.textContent =
          contentTypes.length === 0 ? 'None' : contentTypes.join(',');
    } else if (key === 'logos') {
      // Format an array of mojo media images.
      data.forEach((image) => {
        const a = document.createElement('a');
        a.href = image.src.url;
        a.textContent = image.src.url;
        a.target = '_blank';
        td.appendChild(a);
        td.appendChild(document.createElement('br'));
      });
    } else {
      td.textContent = data;
    }
  }

  /**
   * Compares two objects based on |sortKey|.
   * @param {string} sortKey The name of the property to sort by.
   * @param {?Object} a The first object to compare.
   * @param {?Object} b The second object to compare.
   * @return {number} A negative number if |a| should be ordered
   *     before |b|, a positive number otherwise.
   */
  compareTableItem(sortKey, a, b) {
    const val1 = a[sortKey];
    const val2 = b[sortKey];

    if (sortKey === 'url') {
      return val1.url > val2.url ? 1 : -1;
    } else if (
        sortKey === 'id' || sortKey === 'displayName' ||
        sortKey === 'userStatus' || sortKey === 'lastFetchResult' ||
        sortKey === 'fetchFailedCount' || sortKey === 'lastFetchItemCount' ||
        sortKey === 'lastFetchPlayNextCount' ||
        sortKey === 'lastFetchContentTypes') {
      return val1 > val2 ? 1 : -1;
    } else if (
        sortKey === 'lastDiscoveryTime' || sortKey === 'lastFetchTime' ||
        sortKey === 'cacheExpiryTime') {
      return val1.internalValue > val2.internalValue ? 1 : -1;
    }

    assertNotReached('Unsupported sort key: ' + sortKey);
    return 0;
  }
}

/**
 * Converts a mojo time to a JS time.
 * @param {mojoBase.mojom.Time} mojoTime
 * @return {Date}
 */
function convertMojoTimeToJS(mojoTime) {
  if (mojoTime === null) {
    return new Date();
  }

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
 * Retrieve feed info and render the feed table.
 */
function updateFeedsTable() {
  store.getMediaFeeds().then(response => {
    feedsTable.setData(response.feeds);
    mediaFeedsPageIsPopulatedResolver.resolve();
  });
}

document.addEventListener('DOMContentLoaded', () => {
  store = mediaFeeds.mojom.MediaFeedsStore.getRemote();

  delegate = new MediaFeedsTableDelegate();
  feedsTable = new cr.ui.MediaDataTable($('feeds-table'), delegate);

  updateFeedsTable();

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
