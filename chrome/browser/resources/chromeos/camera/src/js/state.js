// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {function(boolean, cca.PerfInformation=)}
 */
let StateObserver;  // eslint-disable-line no-unused-vars

/**
 * @type {!Map<string, Set<!StateObserver>>}
 */
const allObservers = new Map();

/**
 * Adds observer function to be called on any state change.
 * @param {string} state State to be observed.
 * @param {!StateObserver} observer Observer function called with
 *     newly changed value.
 */
export function addObserver(state, observer) {
  let observers = allObservers.get(state);
  if (observers === undefined) {
    observers = new Set();
    allObservers.set(state, observers);
  }
  observers.add(observer);
}

/**
 * Removes observer function to be called on state change.
 * @param {string} state State to remove observer from.
 * @param {!StateObserver} observer Observer function to be removed.
 * @return {boolean} Whether the observer is in the set and is removed
 *     successfully or not.
 */
export function removeObserver(state, observer) {
  const observers = allObservers.get(state);
  if (observers === undefined) {
    return false;
  }
  return observers.delete(observer);
}

/**
 * Checks if the specified state exists.
 * @param {string} state State to be checked.
 * @return {boolean} Whether the state exists.
 */
export function get(state) {
  return document.body.classList.contains(state);
}

/**
 * Sets the specified state on or off. Optionally, pass the information for
 * performance measurement.
 * @param {string} state State to be set.
 * @param {boolean} val True to set the state on, false otherwise.
 * @param {cca.PerfInformation=} perfInfo Optional information of this state for
 *     performance measurement.
 */
export function set(state, val, perfInfo = {}) {
  const oldVal = get(state);
  if (oldVal === val) {
    return;
  }

  document.body.classList.toggle(state, val);
  const observers = allObservers.get(state) || [];
  observers.forEach((f) => f(val, perfInfo));
}

/** @const */
cca.state.addObserver = addObserver;
/** @const */
cca.state.removeObserver = removeObserver;
/** @const */
cca.state.get = get;
/** @const */
cca.state.set = set;
