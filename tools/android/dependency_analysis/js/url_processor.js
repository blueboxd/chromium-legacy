// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keys for identifying URL params.
const URL_PARAM_KEYS = {
  FILTER: 'filter',
};

/**
 * Converts a URL to the node filter contained within its querystring.
 * @param {string} url The URL to convert.
 * @return {!Array<string>} The array of node names in the URL's filter, or an
 *     empty array if there was no filter in the URL.
 */
function generateFilterFromUrl(url) {
  const pageUrl = new URL(url);
  const filterNodes = pageUrl.searchParams.get(URL_PARAM_KEYS.FILTER);
  return (filterNodes === null) ? [] : filterNodes.split(',');
}

/**
 * Converts a node filter into a URL containing the filter information. The
 * filter information will be stored in the querystring of the supplied URL.
 * @param {!Array<string>} filter The node name filter to store in the URL.
 * @param {string} currentUrl The URL of the current page.
 * @return {string} The new URL containing the filter information.
 */
function generateUrlFromFilter(filter, currentUrl) {
  const pageUrl = new URL(currentUrl);
  const searchParams = new URLSearchParams();
  if (filter.length > 0) {
    searchParams.append(URL_PARAM_KEYS.FILTER, filter.join(','));
  }
  return `${pageUrl.origin}${pageUrl.pathname}?${searchParams.toString()}`;
}

export {
  generateFilterFromUrl,
  generateUrlFromFilter,
};
