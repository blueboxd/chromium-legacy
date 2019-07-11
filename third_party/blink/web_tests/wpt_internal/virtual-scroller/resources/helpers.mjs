/**
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * @fileoverview Helpers for testing virtual-scroller code.
 * @package
 */

/**
 * Creates a DIV with id and textContent set to |id|.
 */
export function div(id) {
  const d = document.createElement('div');
  d.id = id;
  d.textContent = id;
  return d;
}

/**
 * Creates a container DIV with |n| child DIVs with their margin=|margin|.
 */
export function appendDivs(container, n, margin) {
  for (let i = 0; i < n; i++) {
    const d = div('d' + i);
    d.style.margin = margin;
    container.append(d);
  }
}

/*
 * Creates an element, appends it to the BODY, passes it to |callback| and
 * removes it in a finally.
 */
export function withElement(name, callback) {
  const element = document.createElement(name);
  try {
    document.body.appendChild(element);
    callback(element);
  } finally {
    element.remove();
  }
}

/**
 * Remove the reftest-wait class from the HTML element.
*/
export function stopWaiting() {
  document.documentElement.classList.remove('reftest-wait');
}

/**
 * Generate a string with |n| words.
 */
export function words(n) {
  let w = '';
  for (let i = 0; i < n; i++) {
    w += 'word ';
  }
  return w;
}

/**
 * Allow the current frame to end and then call |callback| asap in the next
 * frame.
*/
export function nextFrame(callback) {
  window.requestAnimationFrame(() => {
    window.setTimeout(callback, 0);
  });
}
