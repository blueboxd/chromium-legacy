// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/invalidator_registrar_with_memory.h"

#include <memory>

#include "base/macros.h"
#include "components/invalidation/impl/fake_invalidation_handler.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/invalidation/public/topic_invalidation_map.h"
#include "components/prefs/testing_pref_service.h"
#include "google/cacheinvalidation/include/types.h"
#include "google/cacheinvalidation/types.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

// Initialize the invalidator, register a handler, register some topics for that
// handler, and then unregister the handler, dispatching invalidations in
// between. The handler should only see invalidations when it's registered and
// its topics are registered.
TEST(InvalidatorRegistrarWithMemoryTest, Basic) {
  const invalidation::ObjectId id1(ipc::invalidation::ObjectSource::TEST, "a");
  const invalidation::ObjectId id2(ipc::invalidation::ObjectSource::TEST, "b");
  const invalidation::ObjectId id3(ipc::invalidation::ObjectSource::TEST, "c");
  const invalidation::ObjectId id4(ipc::invalidation::ObjectSource::TEST, "d");

  TestingPrefServiceSimple pref_service;
  InvalidatorRegistrarWithMemory::RegisterProfilePrefs(pref_service.registry());

  auto invalidator = std::make_unique<InvalidatorRegistrarWithMemory>(
      &pref_service, "sender_id", /*migrate_old_prefs=*/false);

  FakeInvalidationHandler handler;
  invalidator->RegisterHandler(&handler);

  TopicInvalidationMap invalidation_map;
  invalidation_map.Insert(Invalidation::Init(id1, 1, "1"));
  invalidation_map.Insert(Invalidation::Init(id2, 2, "2"));
  invalidation_map.Insert(Invalidation::Init(id3, 3, "3"));

  // Should be ignored since no topics are registered to |handler|.
  invalidator->DispatchInvalidationsToHandlers(invalidation_map);
  EXPECT_EQ(0, handler.GetInvalidationCount());

  EXPECT_TRUE(invalidator->UpdateRegisteredTopics(
      &handler, ConvertIdsToTopics({id1, id2}, &handler)));

  invalidator->UpdateInvalidatorState(INVALIDATIONS_ENABLED);
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler.GetInvalidatorState());

  TopicInvalidationMap expected_invalidations;
  expected_invalidations.Insert(Invalidation::Init(id1, 1, "1"));
  expected_invalidations.Insert(Invalidation::Init(id2, 2, "2"));

  invalidator->DispatchInvalidationsToHandlers(invalidation_map);
  EXPECT_EQ(1, handler.GetInvalidationCount());
  EXPECT_EQ(expected_invalidations,
            ConvertObjectIdInvalidationMapToTopicInvalidationMap(
                handler.GetLastInvalidationMap()));

  // Remove id1, add id3.
  EXPECT_TRUE(invalidator->UpdateRegisteredTopics(
      &handler, ConvertIdsToTopics({id2, id3}, &handler)));

  expected_invalidations = TopicInvalidationMap();
  expected_invalidations.Insert(Invalidation::Init(id2, 2, "2"));
  expected_invalidations.Insert(Invalidation::Init(id3, 3, "3"));

  // Removed topic should not be notified, newly-added ones should.
  invalidator->DispatchInvalidationsToHandlers(invalidation_map);
  EXPECT_EQ(2, handler.GetInvalidationCount());
  EXPECT_EQ(expected_invalidations,
            ConvertObjectIdInvalidationMapToTopicInvalidationMap(
                handler.GetLastInvalidationMap()));

  invalidator->UpdateInvalidatorState(TRANSIENT_INVALIDATION_ERROR);
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler.GetInvalidatorState());

  invalidator->UpdateInvalidatorState(INVALIDATION_CREDENTIALS_REJECTED);
  EXPECT_EQ(INVALIDATION_CREDENTIALS_REJECTED, handler.GetInvalidatorState());

  invalidator->UnregisterHandler(&handler);

  // Should be ignored since |handler| isn't registered anymore.
  invalidator->DispatchInvalidationsToHandlers(invalidation_map);
  EXPECT_EQ(2, handler.GetInvalidationCount());
}

// Register handlers and some topics for those handlers, register a handler with
// no topics, and register a handler with some topics but unregister it. Then,
// dispatch some invalidations. Handlers that are not registered should not get
// invalidations, and the ones that have registered topics should receive
// invalidations for those topics.
TEST(InvalidatorRegistrarWithMemoryTest, MultipleHandlers) {
  const invalidation::ObjectId id1(ipc::invalidation::ObjectSource::TEST, "a");
  const invalidation::ObjectId id2(ipc::invalidation::ObjectSource::TEST, "b");
  const invalidation::ObjectId id3(ipc::invalidation::ObjectSource::TEST, "c");
  const invalidation::ObjectId id4(ipc::invalidation::ObjectSource::TEST, "d");

  TestingPrefServiceSimple pref_service;
  InvalidatorRegistrarWithMemory::RegisterProfilePrefs(pref_service.registry());

  auto invalidator = std::make_unique<InvalidatorRegistrarWithMemory>(
      &pref_service, "sender_id", /*migrate_old_prefs=*/false);

  FakeInvalidationHandler handler1;
  FakeInvalidationHandler handler2;
  FakeInvalidationHandler handler3;
  FakeInvalidationHandler handler4;

  invalidator->RegisterHandler(&handler1);
  invalidator->RegisterHandler(&handler2);
  invalidator->RegisterHandler(&handler3);
  invalidator->RegisterHandler(&handler4);

  EXPECT_TRUE(invalidator->UpdateRegisteredTopics(
      &handler1, ConvertIdsToTopics({id1, id2}, &handler1)));
  EXPECT_TRUE(invalidator->UpdateRegisteredTopics(
      &handler2, ConvertIdsToTopics({id3}, &handler2)));
  // Don't register any IDs for handler3.
  EXPECT_TRUE(invalidator->UpdateRegisteredTopics(
      &handler4, ConvertIdsToTopics({id4}, &handler4)));

  invalidator->UnregisterHandler(&handler4);

  invalidator->UpdateInvalidatorState(INVALIDATIONS_ENABLED);
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler1.GetInvalidatorState());
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler2.GetInvalidatorState());
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler3.GetInvalidatorState());
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler4.GetInvalidatorState());

  TopicInvalidationMap invalidation_map;
  invalidation_map.Insert(Invalidation::Init(id1, 1, "1"));
  invalidation_map.Insert(Invalidation::Init(id2, 2, "2"));
  invalidation_map.Insert(Invalidation::Init(id3, 3, "3"));
  invalidation_map.Insert(Invalidation::Init(id4, 4, "4"));

  invalidator->DispatchInvalidationsToHandlers(invalidation_map);

  TopicInvalidationMap expected_invalidations1;
  expected_invalidations1.Insert(Invalidation::Init(id1, 1, "1"));
  expected_invalidations1.Insert(Invalidation::Init(id2, 2, "2"));

  EXPECT_EQ(1, handler1.GetInvalidationCount());
  EXPECT_EQ(expected_invalidations1,
            ConvertObjectIdInvalidationMapToTopicInvalidationMap(
                handler1.GetLastInvalidationMap()));

  TopicInvalidationMap expected_invalidations2;
  expected_invalidations2.Insert(Invalidation::Init(id3, 3, "3"));

  EXPECT_EQ(1, handler2.GetInvalidationCount());
  EXPECT_EQ(expected_invalidations2,
            ConvertObjectIdInvalidationMapToTopicInvalidationMap(
                handler2.GetLastInvalidationMap()));

  EXPECT_EQ(0, handler3.GetInvalidationCount());
  EXPECT_EQ(0, handler4.GetInvalidationCount());

  invalidator->UpdateInvalidatorState(TRANSIENT_INVALIDATION_ERROR);
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler1.GetInvalidatorState());
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler2.GetInvalidatorState());
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler3.GetInvalidatorState());
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler4.GetInvalidatorState());

  invalidator->UnregisterHandler(&handler3);
  invalidator->UnregisterHandler(&handler2);
  invalidator->UnregisterHandler(&handler1);
}

// Multiple registrations by different handlers on the same topic should
// return false.
TEST(InvalidatorRegistrarWithMemoryTest, MultipleRegistrations) {
  const invalidation::ObjectId id1(ipc::invalidation::ObjectSource::TEST, "a");

  TestingPrefServiceSimple pref_service;
  InvalidatorRegistrarWithMemory::RegisterProfilePrefs(pref_service.registry());

  auto invalidator = std::make_unique<InvalidatorRegistrarWithMemory>(
      &pref_service, "sender_id", /*migrate_old_prefs=*/false);

  FakeInvalidationHandler handler1;
  FakeInvalidationHandler handler2;

  invalidator->RegisterHandler(&handler1);
  invalidator->RegisterHandler(&handler2);

  // Registering both handlers for the same topic. First call should succeed,
  // second should fail.
  EXPECT_TRUE(invalidator->UpdateRegisteredTopics(
      &handler1, ConvertIdsToTopics({id1}, &handler1)));
  EXPECT_FALSE(invalidator->UpdateRegisteredTopics(
      &handler2, ConvertIdsToTopics({id1}, &handler2)));

  invalidator->UnregisterHandler(&handler2);
  invalidator->UnregisterHandler(&handler1);
}

// Make sure that passing an empty set to UpdateRegisteredTopics clears the
// corresponding entries for the handler.
TEST(InvalidatorRegistrarWithMemoryTest, EmptySetUnregisters) {
  const invalidation::ObjectId id1(ipc::invalidation::ObjectSource::TEST, "a");
  const invalidation::ObjectId id2(ipc::invalidation::ObjectSource::TEST, "b");
  const invalidation::ObjectId id3(ipc::invalidation::ObjectSource::TEST, "c");

  TestingPrefServiceSimple pref_service;
  InvalidatorRegistrarWithMemory::RegisterProfilePrefs(pref_service.registry());

  auto invalidator = std::make_unique<InvalidatorRegistrarWithMemory>(
      &pref_service, "sender_id", /*migrate_old_prefs=*/false);

  FakeInvalidationHandler handler1;

  // Control observer.
  FakeInvalidationHandler handler2;

  invalidator->RegisterHandler(&handler1);
  invalidator->RegisterHandler(&handler2);

  EXPECT_TRUE(invalidator->UpdateRegisteredTopics(
      &handler1, ConvertIdsToTopics({id1, id2}, &handler1)));
  EXPECT_TRUE(invalidator->UpdateRegisteredTopics(
      &handler2, ConvertIdsToTopics({id3}, &handler2)));

  // Unregister the topics for the first observer. It should not receive any
  // further invalidations.
  EXPECT_TRUE(invalidator->UpdateRegisteredTopics(&handler1, {}));

  invalidator->UpdateInvalidatorState(INVALIDATIONS_ENABLED);
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler1.GetInvalidatorState());
  EXPECT_EQ(INVALIDATIONS_ENABLED, handler2.GetInvalidatorState());

  {
    TopicInvalidationMap invalidation_map;
    invalidation_map.Insert(Invalidation::Init(id1, 1, "1"));
    invalidation_map.Insert(Invalidation::Init(id2, 2, "2"));
    invalidation_map.Insert(Invalidation::Init(id3, 3, "3"));
    invalidator->DispatchInvalidationsToHandlers(invalidation_map);
    EXPECT_EQ(0, handler1.GetInvalidationCount());
    EXPECT_EQ(1, handler2.GetInvalidationCount());
  }

  invalidator->UpdateInvalidatorState(TRANSIENT_INVALIDATION_ERROR);
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler1.GetInvalidatorState());
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, handler2.GetInvalidatorState());

  invalidator->UnregisterHandler(&handler2);
  invalidator->UnregisterHandler(&handler1);
}

}  // namespace

}  // namespace syncer
