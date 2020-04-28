'use strict';

promise_test(async t => {
   const iterator = navigator.fonts.query();
   assert_true(iterator instanceof Object,
               'query() should return an Object');
  assert_true(!!iterator[Symbol.asyncIterator],
              'query() has an asyncIterator method');

  const availableFonts = [];
  for await (const f of iterator) {
    availableFonts.push(f);
  }

  assert_fonts_exist(availableFonts, getExpectedFontSet());
}, 'query(): standard fonts returned');

promise_test(async t => {
  const iterator = navigator.fonts.query();
  assert_true(iterator instanceof Object,
              'query() should return an Object');

  const expectations = getExpectedFontSet();
  const expectedFonts = await filterFontSet(iterator, expectations);
  const additionalExpectedTables = getMoreExpectedTables(expectations);
  for (const f of expectedFonts) {
    const tables = await f.getTables();
    assert_font_has_tables(f.postscriptName, tables, BASE_TABLES);
    if (f.postscriptName in additionalExpectedTables) {
      assert_font_has_tables(f.postscriptName,
                             tables,
                             additionalExpectedTables[f.postscriptName]);
    }
  }
}, 'getTables(): all fonts have expected tables that are not empty');

promise_test(async t => {
  const iterator = navigator.fonts.query();
  assert_true(iterator instanceof Object,
              'query() should return an Object');

  const expectedFonts = await filterFontSet(iterator, getExpectedFontSet());
  const inputs = [
    ['cmap'], // Single item.
    ['cmap', 'head'], // Multiple items.
    ['head', 'head'], // Duplicate items.
  ];
  for (const f of expectedFonts) {
    for (const tableQuery of inputs) {
      const tables = await f.getTables(tableQuery);
      assert_font_has_tables(f.postscriptName, tables, tableQuery);

      const querySet = new Set(tableQuery);
      assert_true(tables.size == querySet.size,
                  `Got ${tables.size} entries, want ${querySet.size}.`);
    }
  }
}, 'getTables([...]) returns tables');

promise_test(async t => {
  const iterator = navigator.fonts.query();
  assert_true(iterator instanceof Object,
              'query() should return an Object');

  const expectedFonts = await filterFontSet(iterator, getExpectedFontSet());
  const inputs = [
    // Nonexistent Tag.
    ['ABCD'],

    // Boundary testing.
    ['\x20\x20\x20\x20'],
    ['\x7E\x7E\x7E\x7E'],
  ];
  for (const f of expectedFonts) {
    for (const tableQuery of inputs) {
      const tables = await f.getTables(tableQuery);
      assert_true(tables.size == 0, `Got {tables.size} entries, want zero.`);
    }
  }
}, 'getTables([tableName,...]) returns if a table name does not exist');

promise_test(async t => {
  const iterator = navigator.fonts.query();
  assert_true(iterator instanceof Object,
              'query() should return an Object');

  const expectedFonts = await filterFontSet(iterator, getExpectedFontSet());
  const inputs = [
    // empty string
    [''],

    // Invalid length.
    [' '],
    ['A'],
    ['AB'],
    ['ABC'],

    // Boundary testing.
    ['\x19\x19\x19\x19'],
    ['\x7F\x7F\x7F\x7F'],

    // Tag too long.
    ['cmap is useful'],

    // Not ByteString.
    ['\u6568\u6461'], // UTF-16LE, encodes to the same first four bytes as "head" in ASCII.
    ['\u6568\u6461\x00\x00'], // 4-character, 8-byte string.
    ['\u6C34'], // U+6C34 CJK UNIFIED IDEOGRAPH (water)
    ['\uD834\uDD1E'], // U+1D11E MUSICAL SYMBOL G-CLEF (UTF-16 surrogate pair)
    ['\uFFFD'], // U+FFFD REPLACEMENT CHARACTER
    ['\uD800'], // UTF-16 surrogate lead
    ['\uDC00'], // UTF-16 surrogate trail
  ];
  for (const f of expectedFonts) {
    for (const tableQuery of inputs) {
      await promise_rejects_js(t, TypeError, f.getTables(tableQuery));
    }
  }
}, 'getTables([tableName,...]) rejects for invalid input');

promise_test(async t => {
  const iterator = navigator.fonts.query();
  assert_true(iterator instanceof Object,
              'query() should return an Object');

  const expectedFonts = await filterFontSet(iterator, getExpectedFontSet());
  const inputs = [
    {
      // One exists, one doesn't.
      tableQuery: ['head', 'blah'],
      expected: ['head'],
    },
    {
      tableQuery: ['blah', 'head'],
      expected: ['head'],
    },
    {
      tableQuery: ['cmap', 'head', 'blah'],
      expected: ['head', 'cmap'],
    },
    {
      // Duplicate entry.
      tableQuery: ['cmap', 'head', 'blah', 'cmap'],
      expected: ['head', 'cmap'],
    },
  ];
  for (const f of expectedFonts) {
    for (const input of inputs) {
      const tables = await f.getTables(input.tableQuery);
      assert_font_has_tables(f.postscriptName, tables, input.expected);
    }
  }
}, 'getTables([tableName,...]) returns even if a requested table is not found');
