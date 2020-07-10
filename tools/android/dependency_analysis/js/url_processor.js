// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Various utilities to (de)serialize the page model to and from
 * the URL. Currently assumes the page is hosted on localhost, will likely need
 * to be changed if the hosting changes.
 */

/**
 * The different possible absolute pathnames for the visualization page.
 * @readonly @enum {string}
 */
const PagePathName = {
  PACKAGE: '/package_view.html',
  CLASS: '/class_view.html',
};

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
 * @param {string} originUrl The URL to use as the origin for the generated URL.
 * @param {PagePathName} pathName The pathname for the generated URL.
 * @param {!Array<string>} filter The node name filter to store in the URL.
 * @return {string} The new URL containing the filter information.
 */
function generateUrlFromFilter(originUrl, pathName, filter) {
  const pageUrl = new URL(originUrl);
  const searchParams = new URLSearchParams();
  if (filter.length > 0) {
    searchParams.append(URL_PARAM_KEYS.FILTER, filter.join(','));
  }
  return `${pageUrl.origin}${pathName}?${searchParams.toString()}`;
}

export {
  PagePathName,
  generateFilterFromUrl,
  generateUrlFromFilter,
};
