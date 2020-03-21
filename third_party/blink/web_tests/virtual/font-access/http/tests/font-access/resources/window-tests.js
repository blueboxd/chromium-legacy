'use strict';

promise_test(async t => {
   const iterator = navigator.fonts.query();
   assert_true(iterator instanceof Object,
               'query() should return an Object');
   assert_true(!!iterator[Symbol.asyncIterator], 'query() has an asyncIterator method');

  const availableFonts = [];
  for await (const f of iterator) {
    availableFonts.push(f);
  }

  assert_fonts_exist(availableFonts, getExpectedFontSet());
}, 'query(): standard fonts returned');
