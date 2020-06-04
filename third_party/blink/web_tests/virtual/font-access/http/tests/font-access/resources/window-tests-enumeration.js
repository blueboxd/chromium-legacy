'use strict';

promise_test(async t => {
   const iterator = navigator.fonts.query();
  assert_equals(typeof iterator, 'object', 'query() should return an Object');
  assert_true(!!iterator[Symbol.asyncIterator],
              'query() has an asyncIterator method');

  const availableFonts = [];
  for await (const f of iterator) {
    availableFonts.push(f);
  }

  assert_fonts_exist(availableFonts, getEnumerationTestSet());
}, 'query(): standard fonts returned');
