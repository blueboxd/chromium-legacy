// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/bookmarks/bookmarks_browsertest.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"

BookmarksBrowserTest::BookmarksBrowserTest() {}

BookmarksBrowserTest::~BookmarksBrowserTest() {}

void BookmarksBrowserTest::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "testSetIncognito",
      base::BindRepeating(&BookmarksBrowserTest::HandleSetIncognitoAvailability,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "testSetCanEdit",
      base::BindRepeating(&BookmarksBrowserTest::HandleSetCanEditBookmarks,
                          base::Unretained(this)));
}

void BookmarksBrowserTest::SetIncognitoAvailability(int availability) {
  ASSERT_TRUE(availability >= 0 &&
              availability < IncognitoModePrefs::AVAILABILITY_NUM_TYPES);
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kIncognitoModeAvailability, availability);
}

void BookmarksBrowserTest::SetCanEditBookmarks(bool canEdit) {
  browser()->profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kEditBookmarksEnabled, canEdit);
}

void BookmarksBrowserTest::SetupExtensionAPITest() {
  // Add managed bookmarks.
  Profile* profile = browser()->profile();
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  bookmarks::ManagedBookmarkService* managed =
      ManagedBookmarkServiceFactory::GetForProfile(profile);
  bookmarks::test::WaitForBookmarkModelToLoad(model);

  base::ListValue list;
  std::unique_ptr<base::DictionaryValue> node(new base::DictionaryValue());
  node->SetString("name", "Managed Bookmark");
  node->SetString("url", "http://www.chromium.org");
  list.Append(std::move(node));
  node.reset(new base::DictionaryValue());
  node->SetString("name", "Managed Folder");
  node->Set("children", std::make_unique<base::ListValue>());
  list.Append(std::move(node));
  profile->GetPrefs()->Set(bookmarks::prefs::kManagedBookmarks, list);
  ASSERT_EQ(2u, managed->managed_node()->children().size());
}

void BookmarksBrowserTest::SetupExtensionAPIEditDisabledTest() {
  Profile* profile = browser()->profile();

  // Provide some testing data here, since bookmark editing will be disabled
  // within the extension.
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  bookmarks::test::WaitForBookmarkModelToLoad(model);
  const bookmarks::BookmarkNode* bar = model->bookmark_bar_node();
  const bookmarks::BookmarkNode* folder =
      model->AddFolder(bar, 0, base::ASCIIToUTF16("Folder"));
  model->AddURL(bar, 1, base::ASCIIToUTF16("AAA"),
                GURL("http://aaa.example.com"));
  model->AddURL(folder, 0, base::ASCIIToUTF16("BBB"),
                GURL("http://bbb.example.com"));

  PrefService* prefs = user_prefs::UserPrefs::Get(profile);
  prefs->SetBoolean(bookmarks::prefs::kEditBookmarksEnabled, false);
}

void BookmarksBrowserTest::HandleSetIncognitoAvailability(
    const base::ListValue* args) {
  AllowJavascript();

  ASSERT_EQ(2U, args->GetSize());
  const base::Value* callback_id;
  ASSERT_TRUE(args->Get(0, &callback_id));
  int pref_value;
  ASSERT_TRUE(args->GetInteger(1, &pref_value));

  SetIncognitoAvailability(pref_value);

  ResolveJavascriptCallback(*callback_id, base::Value());
}

void BookmarksBrowserTest::HandleSetCanEditBookmarks(
    const base::ListValue* args) {
  AllowJavascript();

  ASSERT_EQ(2U, args->GetSize());
  const base::Value* callback_id;
  ASSERT_TRUE(args->Get(0, &callback_id));
  bool pref_value;
  ASSERT_TRUE(args->GetBoolean(1, &pref_value));

  SetCanEditBookmarks(pref_value);

  ResolveJavascriptCallback(*callback_id, base::Value());
}

content::WebUIMessageHandler* BookmarksBrowserTest::GetMockMessageHandler() {
  return this;
}
