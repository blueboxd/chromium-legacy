// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/discardable_memory/client/client_discardable_shared_memory_manager.h"
#include "base/memory/discardable_memory.h"
#include "base/memory/discardable_shared_memory.h"
#include "base/process/process_metrics.h"
#include "base/synchronization/lock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace discardable_memory {
namespace {

using base::Location;
using base::OnceClosure;
using base::TimeDelta;

class TestSingleThreadTaskRunner : public base::SingleThreadTaskRunner {
  ~TestSingleThreadTaskRunner() override = default;
  bool PostTask(const Location& from_here, OnceClosure task) { return true; }
  template <class T>
  bool DeleteSoon(const Location& from_here, const T* object) {
    return true;
  }
  bool PostDelayedTask(const Location& from_here,
                       OnceClosure task,
                       TimeDelta delay) override {
    return true;
  }
  bool PostNonNestableDelayedTask(const Location& from_here,
                                  OnceClosure task,
                                  TimeDelta delay) override {
    return true;
  }
  bool RunsTasksInCurrentSequence() const override { return true; }
};

class TestClientDiscardableSharedMemoryManager
    : public ClientDiscardableSharedMemoryManager {
 public:
  TestClientDiscardableSharedMemoryManager()
      : ClientDiscardableSharedMemoryManager(
            base::MakeRefCounted<TestSingleThreadTaskRunner>()) {}

  ~TestClientDiscardableSharedMemoryManager() override = default;

  std::unique_ptr<base::DiscardableSharedMemory>
  AllocateLockedDiscardableSharedMemory(size_t size, int32_t id) override {
    auto shared_memory = std::make_unique<base::DiscardableSharedMemory>();
    shared_memory->CreateAndMap(size);
    return shared_memory;
  }

  void DeletedDiscardableSharedMemory(int32_t id) override {}

  size_t GetSize() const {
    base::AutoLock lock(lock_);
    return heap_->GetSize();
  }

  size_t GetSizeOfFreeLists() const {
    base::AutoLock lock(lock_);
    return heap_->GetSizeOfFreeLists();
  }
};

// This test allocates a single piece of memory, then verifies that calling
// |PurgeUnlockedMemory| only affects the memory when it is unlocked.
TEST(ClientDiscardableSharedMemoryManagerTest, Simple) {
  const size_t page_size = base::GetPageSize();
  TestClientDiscardableSharedMemoryManager client;

  // Initially, we should have no memory allocated
  ASSERT_EQ(client.GetBytesAllocated(), 0u);
  ASSERT_EQ(client.GetSizeOfFreeLists(), 0u);

  auto mem = client.AllocateLockedDiscardableMemory(page_size);

  // After allocation, we should have allocated a single piece of memory.
  EXPECT_EQ(client.GetBytesAllocated(), page_size);

  client.PurgeUnlockedMemory();

  // All our memory is locked, so calling |PurgeUnlockedMemory| should have no
  // effect.
  EXPECT_EQ(client.GetBytesAllocated(), base::GetPageSize());

  mem->Unlock();

  // Unlocking has no effect on the amount of memory we have allocated.
  EXPECT_EQ(client.GetBytesAllocated(), base::GetPageSize());

  client.PurgeUnlockedMemory();

  // Now that |mem| is unlocked, the call to |PurgeUnlockedMemory| will
  // remove it.
  EXPECT_EQ(client.GetBytesAllocated(), 0u);
}

// This test allocates multiple pieces of memory, then unlocks them one by one,
// verifying that |PurgeUnlockedMemory| only affects the unlocked pieces of
// memory.
TEST(ClientDiscardableSharedMemoryManagerTest, MultipleOneByOne) {
  const size_t page_size = base::GetPageSize();
  TestClientDiscardableSharedMemoryManager client;

  ASSERT_EQ(client.GetBytesAllocated(), 0u);
  ASSERT_EQ(client.GetSizeOfFreeLists(), 0u);

  auto mem1 = client.AllocateLockedDiscardableMemory(page_size * 2.2);
  auto mem2 = client.AllocateLockedDiscardableMemory(page_size * 1.1);
  auto mem3 = client.AllocateLockedDiscardableMemory(page_size * 3.5);
  auto mem4 = client.AllocateLockedDiscardableMemory(page_size * 0.2);

  EXPECT_EQ(client.GetBytesAllocated(), 10 * page_size);

  // Does nothing because everything is locked.
  client.PurgeUnlockedMemory();

  EXPECT_EQ(client.GetBytesAllocated(), 10 * page_size);

  mem1->Unlock();

  // Does nothing, since we don't have any free memory, just unlocked memory.
  client.ReleaseFreeMemory();

  EXPECT_EQ(client.GetBytesAllocated(), 10 * page_size);

  // This gets rid of |mem1| (which is unlocked), but not the rest of the
  // memory.
  client.PurgeUnlockedMemory();

  EXPECT_EQ(client.GetBytesAllocated(), 7 * page_size);

  // We do similar checks to above for the rest of the memory.
  mem2->Unlock();

  client.PurgeUnlockedMemory();

  EXPECT_EQ(client.GetBytesAllocated(), 5 * page_size);

  mem3->Unlock();

  client.PurgeUnlockedMemory();
  EXPECT_EQ(client.GetBytesAllocated(), 1 * page_size);

  mem4->Unlock();

  client.PurgeUnlockedMemory();
  EXPECT_EQ(client.GetBytesAllocated(), 0 * page_size);
}

// This test allocates multiple pieces of memory, then unlocks them all,
// verifying that |PurgeUnlockedMemory| only affects the unlocked pieces of
// memory.
TEST(ClientDiscardableSharedMemoryManagerTest, MultipleAtOnce) {
  const size_t page_size = base::GetPageSize();
  TestClientDiscardableSharedMemoryManager client;

  ASSERT_EQ(client.GetBytesAllocated(), 0u);
  ASSERT_EQ(client.GetSizeOfFreeLists(), 0u);

  auto mem1 = client.AllocateLockedDiscardableMemory(page_size * 2.2);
  auto mem2 = client.AllocateLockedDiscardableMemory(page_size * 1.1);
  auto mem3 = client.AllocateLockedDiscardableMemory(page_size * 3.5);
  auto mem4 = client.AllocateLockedDiscardableMemory(page_size * 0.2);

  EXPECT_EQ(client.GetBytesAllocated(), 10 * page_size);

  // Does nothing because everything is locked.
  client.PurgeUnlockedMemory();

  EXPECT_EQ(client.GetBytesAllocated(), 10 * page_size);

  // Unlock all pieces of memory at once.
  mem1->Unlock();
  mem2->Unlock();
  mem3->Unlock();
  mem4->Unlock();

  client.PurgeUnlockedMemory();
  EXPECT_EQ(client.GetBytesAllocated(), 0 * page_size);
}

// Tests that FreeLists are only released once all memory has been released.
TEST(ClientDiscardableSharedMemoryManagerTest, Release) {
  const size_t page_size = base::GetPageSize();
  TestClientDiscardableSharedMemoryManager client;

  ASSERT_EQ(client.GetBytesAllocated(), 0u);
  ASSERT_EQ(client.GetSizeOfFreeLists(), 0u);

  auto mem1 = client.AllocateLockedDiscardableMemory(page_size * 3);
  auto mem2 = client.AllocateLockedDiscardableMemory(page_size * 2);

  size_t freelist_size = client.GetSizeOfFreeLists();
  EXPECT_EQ(client.GetBytesAllocated(), 5 * page_size);

  mem1 = nullptr;

  // Less memory is now allocated, but freelists are grown.
  EXPECT_EQ(client.GetBytesAllocated(), page_size * 2);
  EXPECT_EQ(client.GetSizeOfFreeLists(), freelist_size + page_size * 3);

  client.PurgeUnlockedMemory();

  // Purging doesn't remove any memory since none is unlocked, also doesn't
  // remove freelists since we still have some.
  EXPECT_EQ(client.GetBytesAllocated(), page_size * 2);
  EXPECT_EQ(client.GetSizeOfFreeLists(), freelist_size + page_size * 3);

  mem2 = nullptr;

  // No memory is allocated, but freelists are grown.
  EXPECT_EQ(client.GetBytesAllocated(), 0u);
  EXPECT_EQ(client.GetSizeOfFreeLists(), freelist_size + page_size * 5);

  client.PurgeUnlockedMemory();

  // Purging now shrinks freelists as well.
  EXPECT_EQ(client.GetBytesAllocated(), 0u);
  EXPECT_EQ(client.GetSizeOfFreeLists(), 0u);
}

// Similar to previous test, but makes sure that freelist still shrinks when
// last piece of memory was just unlocked instead of released.
TEST(ClientDiscardableSharedMemoryManagerTest, ReleaseUnlocked) {
  const size_t page_size = base::GetPageSize();
  TestClientDiscardableSharedMemoryManager client;

  ASSERT_EQ(client.GetBytesAllocated(), 0u);
  ASSERT_EQ(client.GetSizeOfFreeLists(), 0u);

  auto mem1 = client.AllocateLockedDiscardableMemory(page_size * 3);
  auto mem2 = client.AllocateLockedDiscardableMemory(page_size * 2);

  size_t freelist_size = client.GetSizeOfFreeLists();
  EXPECT_EQ(client.GetBytesAllocated(), 5 * page_size);

  mem1 = nullptr;

  // Less memory is now allocated, but freelists are grown.
  EXPECT_EQ(client.GetBytesAllocated(), page_size * 2);
  EXPECT_EQ(client.GetSizeOfFreeLists(), freelist_size + page_size * 3);

  client.PurgeUnlockedMemory();

  // Purging doesn't remove any memory since none is unlocked, also doesn't
  // remove freelists since we still have some.
  EXPECT_EQ(client.GetBytesAllocated(), page_size * 2);
  EXPECT_EQ(client.GetSizeOfFreeLists(), freelist_size + page_size * 3);

  mem2->Unlock();

  // No change in memory usage, since memory was only unlocked not released.
  EXPECT_EQ(client.GetBytesAllocated(), page_size * 2);
  EXPECT_EQ(client.GetSizeOfFreeLists(), freelist_size + page_size * 3);

  client.PurgeUnlockedMemory();

  // Purging now shrinks freelists as well.
  EXPECT_EQ(client.GetBytesAllocated(), 0u);
  EXPECT_EQ(client.GetSizeOfFreeLists(), 0u);
}

}  // namespace
}  // namespace discardable_memory
