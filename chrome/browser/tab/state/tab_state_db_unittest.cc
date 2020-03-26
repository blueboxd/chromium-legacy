// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/state/tab_state_db.h"

#include <map>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace {
const char kMockKey[] = "key";
const char kMockKeyPrefix[] = "k";
const std::vector<uint8_t> kMockValue = {0xfa, 0x5b, 0x4c, 0x12};
}  // namespace

class TabStateDBTest : public testing::Test {
 public:
  TabStateDBTest() : content_db_(nullptr) {}

  // Initialize the test database
  void InitDatabase() {
    auto storage_db = std::make_unique<
        leveldb_proto::test::FakeDB<tab_state_db::TabStateContentProto>>(
        &content_db_storage_);
    content_db_ = storage_db.get();
    base::RunLoop run_loop;
    tab_state_db_ = base::WrapUnique(new TabStateDB(
        std::move(storage_db),
        base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock(),
                                         base::TaskPriority::USER_VISIBLE}),
        run_loop.QuitClosure()));

    MockInitCallback(content_db_, leveldb_proto::Enums::InitStatus::kOK);
    run_loop.Run();
  }

  // Wait for all tasks to be cleared off the queue
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void MockInitCallback(leveldb_proto::test::FakeDB<
                            tab_state_db::TabStateContentProto>* storage_db,
                        leveldb_proto::Enums::InitStatus status) {
    storage_db->InitStatusCallback(status);
    RunUntilIdle();
  }

  void MockInsertCallback(leveldb_proto::test::FakeDB<
                              tab_state_db::TabStateContentProto>* storage_db,
                          bool result) {
    storage_db->UpdateCallback(result);
    RunUntilIdle();
  }

  void MockLoadCallback(leveldb_proto::test::FakeDB<
                            tab_state_db::TabStateContentProto>* storage_db,
                        bool res) {
    storage_db->LoadCallback(res);
    RunUntilIdle();
  }

  void MockDeleteCallback(leveldb_proto::test::FakeDB<
                              tab_state_db::TabStateContentProto>* storage_db,
                          bool res) {
    storage_db->UpdateCallback(res);
    RunUntilIdle();
  }

  void OperationEvaluation(base::OnceClosure closure,
                           bool expected_success,
                           bool actual_success) {
    EXPECT_EQ(expected_success, actual_success);
    std::move(closure).Run();
  }

  void GetEvaluation(base::OnceClosure closure,
                     std::vector<TabStateDB::KeyAndValue> expected,
                     bool result,
                     std::vector<TabStateDB::KeyAndValue> found) {
    for (size_t i = 0; i < expected.size(); i++) {
      EXPECT_EQ(found[i].first, expected[i].first);
      EXPECT_EQ(found[i].second, expected[i].second);
    }
    std::move(closure).Run();
  }

  TabStateDB* tab_state_db() { return tab_state_db_.get(); }
  leveldb_proto::test::FakeDB<tab_state_db::TabStateContentProto>*
  content_db() {
    return content_db_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::map<std::string, tab_state_db::TabStateContentProto> content_db_storage_;
  leveldb_proto::test::FakeDB<tab_state_db::TabStateContentProto>* content_db_;
  std::unique_ptr<TabStateDB> tab_state_db_;

  DISALLOW_COPY_AND_ASSIGN(TabStateDBTest);
};

TEST_F(TabStateDBTest, TestInit) {
  InitDatabase();
  EXPECT_EQ(true, tab_state_db()->IsInitialized());
}

TEST_F(TabStateDBTest, TestKeyInsertionSucceeded) {
  InitDatabase();
  base::RunLoop run_loop[2];
  tab_state_db()->InsertContent(
      kMockKey, kMockValue,
      base::BindOnce(&TabStateDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  MockInsertCallback(content_db(), true);
  run_loop[0].Run();
  std::vector<TabStateDB::KeyAndValue> expected;
  expected.emplace_back(kMockKey, kMockValue);
  tab_state_db()->LoadContent(
      kMockKey,
      base::BindOnce(&TabStateDBTest::GetEvaluation, base::Unretained(this),
                     run_loop[1].QuitClosure(), expected));
  MockLoadCallback(content_db(), true);
  run_loop[1].Run();
}

TEST_F(TabStateDBTest, TestKeyInsertionFailed) {
  InitDatabase();
  base::RunLoop run_loop[2];
  tab_state_db()->InsertContent(
      kMockKey, kMockValue,
      base::BindOnce(&TabStateDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), false));
  MockInsertCallback(content_db(), false);
  run_loop[0].Run();
  std::vector<TabStateDB::KeyAndValue> expected;
  tab_state_db()->LoadContent(
      kMockKey,
      base::BindOnce(&TabStateDBTest::GetEvaluation, base::Unretained(this),
                     run_loop[1].QuitClosure(), expected));
  MockLoadCallback(content_db(), true);
  run_loop[1].Run();
}

TEST_F(TabStateDBTest, TestKeyInsertionPrefix) {
  InitDatabase();
  base::RunLoop run_loop[2];
  tab_state_db()->InsertContent(
      kMockKey, kMockValue,
      base::BindOnce(&TabStateDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  MockInsertCallback(content_db(), true);
  run_loop[0].Run();
  std::vector<TabStateDB::KeyAndValue> expected;
  expected.emplace_back(kMockKey, kMockValue);
  tab_state_db()->LoadContent(
      kMockKeyPrefix,
      base::BindOnce(&TabStateDBTest::GetEvaluation, base::Unretained(this),
                     run_loop[1].QuitClosure(), expected));
  MockLoadCallback(content_db(), true);
  run_loop[1].Run();
}

TEST_F(TabStateDBTest, TestDelete) {
  InitDatabase();
  base::RunLoop run_loop[4];
  tab_state_db()->InsertContent(
      kMockKey, kMockValue,
      base::BindOnce(&TabStateDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  MockInsertCallback(content_db(), true);
  run_loop[0].Run();
  std::vector<TabStateDB::KeyAndValue> expected;
  expected.emplace_back(kMockKey, kMockValue);
  tab_state_db()->LoadContent(
      kMockKey,
      base::BindOnce(&TabStateDBTest::GetEvaluation, base::Unretained(this),
                     run_loop[1].QuitClosure(), expected));
  MockLoadCallback(content_db(), true);
  run_loop[1].Run();

  tab_state_db()->DeleteContent(
      kMockKey,
      base::BindOnce(&TabStateDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[2].QuitClosure(), true));
  MockDeleteCallback(content_db(), true);
  run_loop[2].Run();

  std::vector<TabStateDB::KeyAndValue> expected_after_delete;
  tab_state_db()->LoadContent(
      kMockKey,
      base::BindOnce(&TabStateDBTest::GetEvaluation, base::Unretained(this),
                     run_loop[3].QuitClosure(), expected_after_delete));
  MockLoadCallback(content_db(), true);
  run_loop[3].Run();
}
