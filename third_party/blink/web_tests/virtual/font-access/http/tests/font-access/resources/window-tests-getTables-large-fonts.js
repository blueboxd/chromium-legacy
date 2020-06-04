'use strict';

promise_test(async t => {
  const iterator = navigator.fonts.query();

  const expectations = getEnumerationTestSet({labelFilter: [TEST_SIZE_CATEGORY.large]});
  const expectedFonts = await filterEnumeration(iterator, expectations);
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
}, 'getTables(): large fonts have expected non-empty tables');
