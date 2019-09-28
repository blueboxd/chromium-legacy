// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/dom_distiller_test_util.h"

#include "components/dom_distiller/core/dom_distiller_store.h"
#include "components/dom_distiller/core/fake_distiller.h"

using leveldb_proto::test::FakeDB;

namespace dom_distiller {
namespace test {
namespace util {

namespace {

std::vector<ArticleEntry> EntryMapToList(
    const FakeDB<ArticleEntry>::EntryMap& entries) {
  std::vector<ArticleEntry> entry_list;
  for (auto it = entries.begin(); it != entries.end(); ++it) {
    entry_list.push_back(it->second);
  }
  return entry_list;
}
}  // namespace

// static
DomDistillerStore* CreateStoreWithFakeDB(
    FakeDB<ArticleEntry>* fake_db,
    const FakeDB<ArticleEntry>::EntryMap& store_model) {
  return new DomDistillerStore(
      std::unique_ptr<leveldb_proto::ProtoDatabase<ArticleEntry>>(fake_db),
      EntryMapToList(store_model));
}

}  // namespace util
}  // namespace test
}  // namespace dom_distiller
