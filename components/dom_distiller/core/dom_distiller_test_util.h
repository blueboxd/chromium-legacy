// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_TEST_UTIL_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_TEST_UTIL_H_

#include <vector>

#include "components/dom_distiller/core/article_entry.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace dom_distiller {

class DomDistillerStore;

namespace test {
namespace util {

// Creates a simple DomDistillerStore backed by |fake_db| and initialized
// with |store_model|.
DomDistillerStore* CreateStoreWithFakeDB(
    leveldb_proto::test::FakeDB<ArticleEntry>* fake_db,
    const leveldb_proto::test::FakeDB<ArticleEntry>::EntryMap& store_model);

}  // namespace util
}  // namespace test
}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_TEST_UTIL_H_
