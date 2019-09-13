// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

test.realbox = {};

/**
 * @enum {string}
 * @const
 */
test.realbox.IDS = {
  REALBOX: 'realbox',
  REALBOX_INPUT_WRAPPER: 'realbox-input-wrapper',
  REALBOX_MATCHES: 'realbox-matches',
};

/**
 * @enum {string}
 * @const
 */
test.realbox.CLASSES = {
  SHOW_MATCHES: 'show-matches',
};

/** @enum {number} */
test.realbox.AutocompleteResultStatus = {
  SUCCESS: 0,
  SKIPPED: 1,
};

/**
 * @param {string} name
 * @param {!ClipboardEvent}
 */
test.realbox.clipboardEvent = function(name) {
  return new ClipboardEvent(
      name, {cancelable: true, clipboardData: new DataTransfer()});
};

/**
 * @param {!Object=} modifiers Things to override about the returned result.
 * @return {!AutocompleteResult}
 */
test.realbox.getUrlMatch = function(modifiers = {}) {
  return Object.assign(
      {
        allowedToBeDefaultMatch: true,
        contents: 'helloworld.com',
        contentsClass: [{offset: 0, style: 1}],
        description: '',
        descriptionClass: [],
        destinationUrl: 'https://helloworld.com/',
        inlineAutocompletion: '',
        isSearchType: false,
        fillIntoEdit: 'https://helloworld.com',
        swapContentsAndDescription: true,
        type: 'url-what-you-typed',
      },
      modifiers);
};

/**
 * @param {!Object=} modifiers Things to override about the returned result.
 * @return {!AutocompleteResult}
 */
test.realbox.getSearchMatch = function(modifiers = {}) {
  return Object.assign(
      {
        allowedToBeDefaultMatch: true,
        contents: 'hello world',
        contentsClass: [{offset: 0, style: 0}],
        description: 'Google search',
        descriptionClass: [{offset: 0, style: 4}],
        destinationUrl: 'https://www.google.com/search?q=hello+world',
        inlineAutocompletion: '',
        isSearchType: true,
        fillIntoEdit: 'hello world',
        swapContentsAndDescription: false,
        type: 'search-what-you-typed',
      },
      modifiers);
};

/** @type {!Array<string>} */
test.realbox.queries;

/** @type {!Element} */
test.realbox.realboxEl;

/**
 * Sets up the page for each individual test.
 */
test.realbox.setUp = function() {
  setUpPage('local-ntp-template');

  configData.realboxEnabled = true;

  chrome.embeddedSearch = {
    newTabPage: {},
    searchBox: {
      queryAutocomplete(query) {
        test.realbox.queries.push(query);
      },
    },
  };

  test.realbox.queries = [];

  initLocalNTP(/*isGooglePage=*/ true);

  test.realbox.realboxEl = $(test.realbox.IDS.REALBOX);
  assertTrue(!!test.realbox.realboxEl);

  test.realbox.wrapperEl = $(test.realbox.IDS.REALBOX_INPUT_WRAPPER);
  assertTrue(!!test.realbox.wrapperEl);

  assertFalse(test.realbox.wrapperEl.classList.contains(
      test.realbox.CLASSES.SHOW_MATCHES));
};

test.realbox.testEmptyValueDoesntQueryAutocomplete = function() {
  test.realbox.realboxEl.value = '';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));
  assertEquals(test.realbox.queries.length, 0);
};

test.realbox.testSpacesDontQueryAutocomplete = function() {
  test.realbox.realboxEl.value = '   ';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));
  assertEquals(test.realbox.queries.length, 0);
};

test.realbox.testInputSentAsQuery = function() {
  test.realbox.realboxEl.value = 'hello realbox';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));
  assertEquals(1, test.realbox.queries.length);
  assertEquals('hello realbox', test.realbox.queries[0]);
};

test.realbox.testReplyWithMatches = function() {
  test.realbox.realboxEl.value = 'hello world';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));
  assertEquals(1, test.realbox.queries.length);
  assertEquals('hello world', test.realbox.queries[0]);

  const matches = [test.realbox.getSearchMatch(), test.realbox.getUrlMatch()];
  chrome.embeddedSearch.searchBox.onqueryautocompletedone(
      {input: test.realbox.realboxEl.value, matches});

  assertTrue(test.realbox.wrapperEl.classList.contains(
      test.realbox.CLASSES.SHOW_MATCHES));

  const matchesEl = $(test.realbox.IDS.REALBOX_MATCHES);
  assertTrue(!!matchesEl);
  assertEquals(matches.length, matchesEl.children.length);
  assertEquals(matches[0].destinationUrl, matchesEl.children[0].href);
  assertEquals(matches[1].destinationUrl, matchesEl.children[1].href);
  assertEquals(matches[1].textContent, matchesEl.children[1].contents);
};

test.realbox.testReplyWithInlineAutocompletion = function() {
  test.realbox.realboxEl.value = 'hello ';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));
  assertEquals(1, test.realbox.queries.length);
  assertEquals('hello ', test.realbox.queries[0]);

  const match = test.realbox.getSearchMatch({
    contents: 'hello ',
    inlineAutocompletion: 'world',
  });
  chrome.embeddedSearch.searchBox.onqueryautocompletedone({
    input: test.realbox.realboxEl.value,
    matches: [match],
  });

  assertTrue(test.realbox.wrapperEl.classList.contains(
      test.realbox.CLASSES.SHOW_MATCHES));

  const matchesEl = $(test.realbox.IDS.REALBOX_MATCHES);
  assertTrue(!!matchesEl);
  assertEquals(1, matchesEl.children.length);
  assertEquals(match.destinationUrl, matchesEl.children[0].href);

  const realboxValue = test.realbox.realboxEl.value;
  assertEquals('hello world', realboxValue);
  const start = test.realbox.realboxEl.selectionStart;
  const end = test.realbox.realboxEl.selectionEnd;
  assertEquals('world', realboxValue.substring(start, end));
};

test.realbox.testDeleteWithInlineAutocompletion = function() {
  test.realbox.realboxEl.value = 'supercal';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));
  assertEquals(1, test.realbox.queries.length);
  assertEquals('supercal', test.realbox.queries[0]);

  const match = test.realbox.getSearchMatch({
    contents: 'supercal',
    inlineAutocompletion: 'ifragilisticexpialidocious',
  });
  chrome.embeddedSearch.searchBox.onqueryautocompletedone({
    input: test.realbox.realboxEl.value,
    matches: [match],
  });

  const realboxValue = test.realbox.realboxEl.value;
  assertEquals('supercalifragilisticexpialidocious', realboxValue);

  test.realbox.realboxEl.value = 'superca';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  match.contents = 'superca';
  match.inlineAutocompletion = 'lifragilisticexpialidocious';
  chrome.embeddedSearch.searchBox.onqueryautocompletedone({
    input: test.realbox.realboxEl.value,
    matches: [match],
  });

  // Ensure that removing characters from input doesn't just inline autocomplete
  // to the same contents on backspace/cut/etc.
  assertEquals('superca', test.realbox.realboxEl.value);

  test.realbox.realboxEl.value = 'super';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  match.contents = 'super';
  match.inlineAutocompletion = 'califragilisticexpialidocious';
  chrome.embeddedSearch.searchBox.onqueryautocompletedone({
    input: test.realbox.realboxEl.value,
    matches: [match],
  });
  assertEquals('super', test.realbox.realboxEl.value);
};

test.realbox.testTypeInlineAutocompletion = function() {
  test.realbox.realboxEl.value = 'what are the';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  chrome.embeddedSearch.searchBox.onqueryautocompletedone({
    input: test.realbox.realboxEl.value,
    matches: [
      test.realbox.getSearchMatch({
        contents: 'what are the',
        inlineAutocompletion: 'se strawberries',
      }),
    ],
  });

  assertEquals('what are these strawberries', test.realbox.realboxEl.value);
  assertEquals('what are the'.length, test.realbox.realboxEl.selectionStart);
  assertEquals(
      'what are these strawberries'.length,
      test.realbox.realboxEl.selectionEnd);

  let wasValueSetterCalled = false;
  const originalValue =
      Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, 'value');

  Object.defineProperty(test.realbox.realboxEl, 'value', {
    get: originalValue.get,
    set: value => {
      wasValueSetterCalled = true;
      originalValue.set.call(test.realbox.realboxEl, value);
    }
  });

  // Pretend the user typed the next character of the inline autocompletion.
  const keyEvent =
      new KeyboardEvent('keydown', {bubbles: true, cancelable: true, key: 's'});
  test.realbox.realboxEl.dispatchEvent(keyEvent);
  assertTrue(keyEvent.defaultPrevented);

  chrome.embeddedSearch.searchBox.onqueryautocompletedone({
    input: test.realbox.realboxEl.value,
    matches: [
      test.realbox.getSearchMatch({
        contents: 'what are thes',
        inlineAutocompletion: 'e strawberries',
      }),
    ],
  });


  assertEquals('what are these strawberries', test.realbox.realboxEl.value);
  assertEquals('what are thes'.length, test.realbox.realboxEl.selectionStart);
  assertEquals(
      'what are these strawberries'.length,
      test.realbox.realboxEl.selectionEnd);
  assertFalse(wasValueSetterCalled);
};

test.realbox.testResultsPreserveCursorPosition = function() {
  test.realbox.realboxEl.value = 'z';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  chrome.embeddedSearch.searchBox.onqueryautocompletedone({
    input: test.realbox.realboxEl.value,
    matches: [test.realbox.getSearchMatch({contents: 'z'})],
  });

  test.realbox.realboxEl.value = 'az';
  test.realbox.realboxEl.selectionStart = 1;
  test.realbox.realboxEl.selectionEnd = 1;
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  chrome.embeddedSearch.searchBox.onqueryautocompletedone({
    input: test.realbox.realboxEl.value,
    matches: [test.realbox.getSearchMatch({contents: 'az'})],
  });

  assertEquals(1, test.realbox.realboxEl.selectionStart);
  assertEquals(1, test.realbox.realboxEl.selectionEnd);
};

test.realbox.testCopyEmptyInputFails = function() {
  const copyEvent = test.realbox.clipboardEvent('copy');
  test.realbox.realboxEl.dispatchEvent(copyEvent);
  assertFalse(copyEvent.defaultPrevented);
};

test.realbox.testCopySearchResultFails = function() {
  test.realbox.realboxEl.value = 'skittles!';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  chrome.embeddedSearch.searchBox.onqueryautocompletedone({
    input: test.realbox.realboxEl.value,
    matches: [test.realbox.getSearchMatch({contents: 'skittles!'})],
  });

  test.realbox.realboxEl.setSelectionRange(0, 'skittles!'.length);

  const copyEvent = test.realbox.clipboardEvent('copy');
  test.realbox.realboxEl.dispatchEvent(copyEvent);
  assertFalse(copyEvent.defaultPrevented);
};

test.realbox.testCopyUrlSucceeds = function() {
  test.realbox.realboxEl.value = 'go';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  chrome.embeddedSearch.searchBox.onqueryautocompletedone({
    input: test.realbox.realboxEl.value,
    matches: [test.realbox.getUrlMatch({
      contents: 'go',
      inlineAutocompletion: 'ogle.com',
      destinationUrl: 'https://www.google.com/',
    })]
  });

  assertEquals('google.com', test.realbox.realboxEl.value);

  test.realbox.realboxEl.setSelectionRange(0, 'google.com'.length);

  const copyEvent = test.realbox.clipboardEvent('copy');
  test.realbox.realboxEl.dispatchEvent(copyEvent);
  assertTrue(copyEvent.defaultPrevented);
  assertEquals(
      'https://www.google.com/', copyEvent.clipboardData.getData('text/plain'));
  assertFalse(test.realbox.realboxEl.value === '');
};

test.realbox.testCutEmptyInputFails = function() {
  const cutEvent = test.realbox.clipboardEvent('cut');
  test.realbox.realboxEl.dispatchEvent(cutEvent);
  assertFalse(cutEvent.defaultPrevented);
};

test.realbox.testCutSearchResultFails = function() {
  test.realbox.realboxEl.value = 'skittles!';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  chrome.embeddedSearch.searchBox.onqueryautocompletedone({
    input: test.realbox.realboxEl.value,
    matches: [test.realbox.getSearchMatch({contents: 'skittles!'})],
  });

  test.realbox.realboxEl.setSelectionRange(0, 'skittles!'.length);

  const cutEvent = test.realbox.clipboardEvent('cut');
  test.realbox.realboxEl.dispatchEvent(cutEvent);
  assertFalse(cutEvent.defaultPrevented);
};

test.realbox.testCutUrlSucceeds = function() {
  test.realbox.realboxEl.value = 'go';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  chrome.embeddedSearch.searchBox.onqueryautocompletedone({
    input: test.realbox.realboxEl.value,
    matches: [
      test.realbox.getUrlMatch({
        contents: 'go',
        inlineAutocompletion: 'ogle.com',
        destinationUrl: 'https://www.google.com/',
      }),
    ],
  });

  assertEquals('google.com', test.realbox.realboxEl.value);

  test.realbox.realboxEl.setSelectionRange(0, 'google.com'.length);

  const cutEvent = test.realbox.clipboardEvent('cut');
  test.realbox.realboxEl.dispatchEvent(cutEvent);
  assertTrue(cutEvent.defaultPrevented);
  assertEquals(
      'https://www.google.com/', cutEvent.clipboardData.getData('text/plain'));
  assertTrue(test.realbox.realboxEl.value === '');
};

test.realbox.testStaleAutocompleteResult = function() {
  assertEquals('', test.realbox.realboxEl.value);
  chrome.embeddedSearch.searchBox.onqueryautocompletedone({
    input: test.realbox.realboxEl.value + 'a',  // Simulate stale response.
    matches: [test.realbox.getSearchMatch(), test.realbox.getUrlMatch()],
  });
};

test.realbox.testAutocompleteResultStatus = function() {
  test.realbox.realboxEl.value = 'g';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  test.realbox.realboxEl.value += 'o';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  chrome.embeddedSearch.searchBox.onqueryautocompletedone({
    input: test.realbox.realboxEl.value,
    matches: [],
    status: test.realbox.AutocompleteResultStatus.SKIPPED,
  });
  assertEquals(0, $(test.realbox.IDS.REALBOX_MATCHES).children.length);

  let RESULTS = [test.realbox.getSearchMatch(), test.realbox.getUrlMatch()];
  chrome.embeddedSearch.searchBox.onqueryautocompletedone({
    input: test.realbox.realboxEl.value,
    matches: RESULTS,
    status: test.realbox.AutocompleteResultStatus.SUCCESS,
  });

  const matchesEl = $(test.realbox.IDS.REALBOX_MATCHES);
  assertEquals(RESULTS.length, matchesEl.children.length);

  test.realbox.realboxEl.value += 'o';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  test.realbox.realboxEl.value += 'g';
  test.realbox.realboxEl.dispatchEvent(new CustomEvent('input'));

  chrome.embeddedSearch.searchBox.onqueryautocompletedone({
    input: test.realbox.realboxEl.value,
    matches: [],
    status: test.realbox.AutocompleteResultStatus.SKIPPED,
  });

  // Checks to see that the matches UI wasn't re-rendered.
  assertTrue(matchesEl === $(test.realbox.IDS.REALBOX_MATCHES));

  chrome.embeddedSearch.searchBox.onqueryautocompletedone({
    input: test.realbox.realboxEl.value,
    matches: RESULTS,
    status: test.realbox.AutocompleteResultStatus.SUCCESS,
  });

  const newMatchesEl = $(test.realbox.IDS.REALBOX_MATCHES);
  assertEquals(RESULTS.length, newMatchesEl.children.length);
  assertFalse(matchesEl === newMatchesEl);
};
