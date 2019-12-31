// Copyright 2019 The Chromium Authors. All rights reserved.cd
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests accessibility in the Search drawer devtool using the axe-core linter.');
  await TestRunner.loadModule('axe_core_test_runner');
  await TestRunner.showPanel('sources');
  const view = 'sources.search-sources-tab';
  await UI.viewManager.showView(view);
  const searchView = await UI.viewManager.view(view).widget();

  const tests = [
    testSearchNoResults,
    testSearchMultipleResults,
    testSearchShowMoreResults,
    testSearchMultipleHighlightedResults
  ];

  async function testSearchNoResults() {
    TestRunner.addResult('Testing search tool with no results.');
    searchView._nothingFound();
    await AxeCoreTestRunner.runValidation(searchView.element);
  }

  async function testSearchMultipleResults() {
    TestRunner.addResult('Testing search tool with multiple results.');
    const searchMatches = [
      new Common.ContentProvider.SearchMatch(1, 'elle'),
      new Common.ContentProvider.SearchMatch(2, 'letter'),
      new Common.ContentProvider.SearchMatch(3, 'couple'),
      new Common.ContentProvider.SearchMatch(4, 'mole'),
      new Common.ContentProvider.SearchMatch(5, 'tolerate'),
      new Common.ContentProvider.SearchMatch(6, 'personable')
    ];
    await runTestValidation(searchMatches, 'le');
  }

  async function testSearchShowMoreResults() {
    TestRunner.addResult('Testing search tool with many results (generates "Show more" button).');
    const searchMatches = [
      new Common.ContentProvider.SearchMatch(1, 'jump'),
      new Common.ContentProvider.SearchMatch(2, 'maze'),
      new Common.ContentProvider.SearchMatch(3, 'zoom'),
      new Common.ContentProvider.SearchMatch(4, 'minx'),
      new Common.ContentProvider.SearchMatch(5, 'mux'),
      new Common.ContentProvider.SearchMatch(6, 'comics'),
      new Common.ContentProvider.SearchMatch(7, 'jimmy'),
      new Common.ContentProvider.SearchMatch(8, 'jumps'),
      new Common.ContentProvider.SearchMatch(9, 'bumpy'),
      new Common.ContentProvider.SearchMatch(10, 'major'),
      new Common.ContentProvider.SearchMatch(11, 'menace'),
      new Common.ContentProvider.SearchMatch(12, 'mom'),
      new Common.ContentProvider.SearchMatch(13, 'animal'),
      new Common.ContentProvider.SearchMatch(14, 'ham'),
      new Common.ContentProvider.SearchMatch(15, 'tame'),
      new Common.ContentProvider.SearchMatch(16, 'pygmy'),
      new Common.ContentProvider.SearchMatch(17, 'blimp'),
      new Common.ContentProvider.SearchMatch(18, 'dummy'),
      new Common.ContentProvider.SearchMatch(19, 'mumsy'),
      new Common.ContentProvider.SearchMatch(20, 'milch'),
      new Common.ContentProvider.SearchMatch(21, 'femme'),
      new Common.ContentProvider.SearchMatch(22, 'grump'),
      new Common.ContentProvider.SearchMatch(23, 'cramp'),
      new Common.ContentProvider.SearchMatch(24, 'lumps'),
      new Common.ContentProvider.SearchMatch(25, 'mumus'),
      new Common.ContentProvider.SearchMatch(26, 'skelm')
    ];
    await runTestValidation(searchMatches, 'm');
  }

  async function testSearchMultipleHighlightedResults() {
    TestRunner.addResult('Testing search tool with multiple highlighted results.');
    const searchMatches = [
      new Common.ContentProvider.SearchMatch(1, 'test      test'),
      new Common.ContentProvider.SearchMatch(2, 'testaaaaaaaaaaaaatest'),
      new Common.ContentProvider.SearchMatch(3, 'test      test'),
      new Common.ContentProvider.SearchMatch(4, 'g test'),
    ];
    await runTestValidation(searchMatches, 'test');
  }

  async function runTestValidation(searchMatches, queryText) {
    const uiSourceCode = new Workspace.UISourceCode(/* project */ null, 'test', /* contentType */ null);
    uiSourceCode.fullDisplayName = function() {return 'test';};
    const searchResult = new Sources.FileBasedSearchResult(uiSourceCode, searchMatches);
    const searchConfig = new Search.SearchConfig(queryText, /* ignoreCase */ true, /* isRegex */ false);
    const searchResultsPane = new Search.SearchResultsPane(searchConfig);
    searchResultsPane.addSearchResult(searchResult);
    searchView._showPane(searchResultsPane);
    await AxeCoreTestRunner.runValidation(searchView.element);
  }

  TestRunner.runAsyncTestSuite(tests);
})();
