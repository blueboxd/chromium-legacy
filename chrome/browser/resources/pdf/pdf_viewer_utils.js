// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserApi} from './browser_api.js';
import {MessageData} from './controller.js';
import {PdfNavigator} from './navigator.js';
import {LayoutOptions} from './viewport.js';

/**
 * @typedef {{
 *   source: Object,
 *   origin: string,
 *   data: !MessageData,
 * }}
 */
export let MessageObject;

/**
 * @typedef {{
 *   type: string,
 *   height: number,
 *   width: number,
 *   layoutOptions: (!LayoutOptions|undefined),
 *   pageDimensions: Array
 * }}
 */
export let DocumentDimensionsMessageData;

/**
 * @typedef {{
 *   type: string,
 *   url: string,
 *   disposition: !PdfNavigator.WindowOpenDisposition,
 * }}
 */
export let NavigateMessageData;

/**
 * @typedef {{
 *   type: string,
 *   page: number,
 *   x: number,
 *   y: number,
 *   zoom: number
 * }}
 */
export let DestinationMessageData;

/**
 * @typedef {{
 *   hasUnsavedChanges: (boolean|undefined),
 *   fileName: string,
 *   dataToSave: !ArrayBuffer
 * }}
 */
export let RequiredSaveResult;

/** @return {number} Width of a scrollbar in pixels */
export function getScrollbarWidth() {
  const div = document.createElement('div');
  div.style.visibility = 'hidden';
  div.style.overflow = 'scroll';
  div.style.width = '50px';
  div.style.height = '50px';
  div.style.position = 'absolute';
  document.body.appendChild(div);
  const result = div.offsetWidth - div.clientWidth;
  div.parentNode.removeChild(div);
  return result;
}

/**
 * Return the filename component of a URL, percent decoded if possible.
 * @param {string} url The URL to get the filename from.
 * @return {string} The filename component.
 */
export function getFilenameFromURL(url) {
  // Ignore the query and fragment.
  const mainUrl = url.split(/#|\?/)[0];
  const components = mainUrl.split(/\/|\\/);
  const filename = components[components.length - 1];
  try {
    return decodeURIComponent(filename);
  } catch (e) {
    if (e instanceof URIError) {
      return filename;
    }
    throw e;
  }
}

/**
 * Whether keydown events should currently be ignored. Events are ignored when
 * an editable element has focus, to allow for proper editing controls.
 * @param {Element} activeElement The currently selected DOM node.
 * @return {boolean} True if keydown events should be ignored.
 */
export function shouldIgnoreKeyEvents(activeElement) {
  while (activeElement.shadowRoot != null &&
         activeElement.shadowRoot.activeElement != null) {
    activeElement = activeElement.shadowRoot.activeElement;
  }

  return (
      activeElement.isContentEditable ||
      (activeElement.tagName === 'INPUT' && activeElement.type !== 'radio') ||
      activeElement.tagName === 'TEXTAREA');
}

/**
 * @param {!BrowserApi} browserApi
 * @param {number} topToolbarHeight
 * @param {string} backgroundColor
 * @param {string} originalUrl
 * @return {!HTMLEmbedElement} The plugin
 */
export function createPlugin(
    browserApi, topToolbarHeight, backgroundColor, originalUrl) {
  const plugin =
      /** @type {!HTMLEmbedElement} */ (document.createElement('embed'));

  // NOTE: The plugin's 'id' field must be set to 'plugin' since
  // chrome/renderer/printing/print_render_frame_helper.cc actually
  // references it.
  plugin.id = 'plugin';
  plugin.type = 'application/x-google-chrome-pdf';

  plugin.setAttribute('src', originalUrl);
  plugin.setAttribute('stream-url', browserApi.getStreamInfo().streamUrl);
  let headers = '';
  for (const header in browserApi.getStreamInfo().responseHeaders) {
    headers += header + ': ' +
        browserApi.getStreamInfo().responseHeaders[header] + '\n';
  }
  plugin.setAttribute('headers', headers);

  plugin.setAttribute('background-color', backgroundColor);
  plugin.setAttribute('top-toolbar-height', topToolbarHeight);

  const javascript = browserApi.getStreamInfo().javascript || 'block';
  plugin.setAttribute('javascript', javascript);

  if (browserApi.getStreamInfo().embedded) {
    plugin.setAttribute('top-level-url', browserApi.getStreamInfo().tabUrl);
  } else {
    plugin.setAttribute('full-frame', '');
  }

  return plugin;
}
