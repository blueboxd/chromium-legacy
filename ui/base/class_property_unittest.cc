// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/class_property.h"

#include <limits.h>

#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(const char*)
DEFINE_UI_CLASS_PROPERTY_TYPE(int)

namespace {

class TestProperty {
 public:
  TestProperty() {}
  ~TestProperty() {
    last_deleted_ = this;
  }
  static void* last_deleted() { return last_deleted_; }

 private:
  static void* last_deleted_;
  DISALLOW_COPY_AND_ASSIGN(TestProperty);
};

void* TestProperty::last_deleted_ = nullptr;

class AssignableTestProperty {
 public:
  AssignableTestProperty() {}
  AssignableTestProperty(int value) : value_(value) {}
  AssignableTestProperty(const AssignableTestProperty& other)
      : value_(other.value_) {}
  AssignableTestProperty(AssignableTestProperty&& other)
      : value_(std::move(other.value_)), was_move_assigned_(true) {}
  AssignableTestProperty& operator=(const AssignableTestProperty& other) {
    value_ = other.value_;
    was_move_assigned_ = false;
    return *this;
  }
  AssignableTestProperty& operator=(AssignableTestProperty&& other) {
    value_ = std::move(other.value_);
    was_move_assigned_ = true;
    return *this;
  }

  int value() const { return value_; }
  bool was_move_assigned() const { return was_move_assigned_; }

 private:
  int value_ = 0;
  bool was_move_assigned_ = false;
};

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(TestProperty, kOwnedKey, nullptr)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(AssignableTestProperty,
                                   kAssignableKey,
                                   nullptr)

}  // namespace

DEFINE_UI_CLASS_PROPERTY_TYPE(TestProperty*)
DEFINE_UI_CLASS_PROPERTY_TYPE(AssignableTestProperty*)

namespace ui {
namespace test {

namespace {

class TestPropertyHandler : public PropertyHandler {
 public:
  int num_events() const { return num_events_; }

 protected:
  void AfterPropertyChange(const void* key, int64_t old_value) override {
    ++num_events_;
  }

 private:
  int num_events_ = 0;
};

const int kDefaultIntValue = -2;
const char* kDefaultStringValue = "squeamish";
const char* kTestStringValue = "ossifrage";

DEFINE_UI_CLASS_PROPERTY_KEY(int, kIntKey, kDefaultIntValue)
DEFINE_UI_CLASS_PROPERTY_KEY(const char*, kStringKey, kDefaultStringValue)
}

TEST(PropertyTest, Property) {
  PropertyHandler h;

  // Non-existent properties should return the default values.
  EXPECT_EQ(kDefaultIntValue, h.GetProperty(kIntKey));
  EXPECT_EQ(std::string(kDefaultStringValue), h.GetProperty(kStringKey));

  // A set property value should be returned again (even if it's the default
  // value).
  h.SetProperty(kIntKey, INT_MAX);
  EXPECT_EQ(INT_MAX, h.GetProperty(kIntKey));
  h.SetProperty(kIntKey, kDefaultIntValue);
  EXPECT_EQ(kDefaultIntValue, h.GetProperty(kIntKey));
  h.SetProperty(kIntKey, INT_MIN);
  EXPECT_EQ(INT_MIN, h.GetProperty(kIntKey));

  h.SetProperty<const char*>(kStringKey, nullptr);
  EXPECT_EQ(NULL, h.GetProperty(kStringKey));
  h.SetProperty(kStringKey, kDefaultStringValue);
  EXPECT_EQ(std::string(kDefaultStringValue), h.GetProperty(kStringKey));
  h.SetProperty(kStringKey, kTestStringValue);
  EXPECT_EQ(std::string(kTestStringValue), h.GetProperty(kStringKey));

  // ClearProperty should restore the default value.
  h.ClearProperty(kIntKey);
  EXPECT_EQ(kDefaultIntValue, h.GetProperty(kIntKey));
  h.ClearProperty(kStringKey);
  EXPECT_EQ(std::string(kDefaultStringValue), h.GetProperty(kStringKey));
}

TEST(PropertyTest, OwnedProperty) {
  TestProperty* p3;
  {
    PropertyHandler h;

    EXPECT_EQ(NULL, h.GetProperty(kOwnedKey));
    void* last_deleted = TestProperty::last_deleted();
    TestProperty* p1 = new TestProperty();
    h.SetProperty(kOwnedKey, p1);
    EXPECT_EQ(p1, h.GetProperty(kOwnedKey));
    EXPECT_EQ(last_deleted, TestProperty::last_deleted());

    TestProperty* p2 = new TestProperty();
    h.SetProperty(kOwnedKey, p2);
    EXPECT_EQ(p2, h.GetProperty(kOwnedKey));
    EXPECT_EQ(p1, TestProperty::last_deleted());

    h.ClearProperty(kOwnedKey);
    EXPECT_EQ(NULL, h.GetProperty(kOwnedKey));
    EXPECT_EQ(p2, TestProperty::last_deleted());

    p3 = new TestProperty();
    h.SetProperty(kOwnedKey, p3);
    EXPECT_EQ(p3, h.GetProperty(kOwnedKey));
    EXPECT_EQ(p2, TestProperty::last_deleted());
  }
  EXPECT_EQ(p3, TestProperty::last_deleted());
}

TEST(PropertyTest, AssignableProperty) {
  PropertyHandler h;

  // Verify that assigning a property by value allocates a value.
  EXPECT_EQ(nullptr, h.GetProperty(kAssignableKey));
  const AssignableTestProperty kTestProperty{1};
  h.SetProperty(kAssignableKey, kTestProperty);
  AssignableTestProperty* p = h.GetProperty(kAssignableKey);
  EXPECT_NE(nullptr, p);
  EXPECT_EQ(1, p->value());

  // Verify that assigning by value updates the existing value without
  // additional allocation.
  h.SetProperty(kAssignableKey, AssignableTestProperty{2});
  EXPECT_EQ(p, h.GetProperty(kAssignableKey));
  EXPECT_EQ(2, p->value());

  // Same as the above case, but with a const reference instead of a move.
  const AssignableTestProperty kTestProperty2{3};
  h.SetProperty(kAssignableKey, kTestProperty2);
  EXPECT_EQ(p, h.GetProperty(kAssignableKey));
  EXPECT_EQ(3, p->value());

  // Verify that clearing the property deallocates the value.
  h.ClearProperty(kAssignableKey);
  EXPECT_EQ(nullptr, h.GetProperty(kAssignableKey));

  // Verify that setting by value after clearing allocates a new value.
  h.SetProperty(kAssignableKey, AssignableTestProperty{4});
  EXPECT_EQ(4, h.GetProperty(kAssignableKey)->value());
}

TEST(PropertyTest, SetProperty_ForwardsParametersCorrectly) {
  PropertyHandler h;

  // New property from a const ref.
  const AssignableTestProperty kTestProperty{1};
  h.SetProperty(kAssignableKey, kTestProperty);
  EXPECT_FALSE(h.GetProperty(kAssignableKey)->was_move_assigned());

  // Set property from inline rvalue ref.
  h.SetProperty(kAssignableKey, AssignableTestProperty{2});
  EXPECT_TRUE(h.GetProperty(kAssignableKey)->was_move_assigned());

  // Set property from lvalue.
  AssignableTestProperty test_property{3};
  h.SetProperty(kAssignableKey, test_property);
  EXPECT_FALSE(h.GetProperty(kAssignableKey)->was_move_assigned());

  // Set property from lvalue ref.
  AssignableTestProperty& ref = test_property;
  h.SetProperty(kAssignableKey, ref);
  EXPECT_FALSE(h.GetProperty(kAssignableKey)->was_move_assigned());

  // Set property from moved rvalue ref.
  h.SetProperty(kAssignableKey, std::move(test_property));
  EXPECT_TRUE(h.GetProperty(kAssignableKey)->was_move_assigned());

  // Set property from const ref.
  const AssignableTestProperty& const_ref = kTestProperty;
  h.SetProperty(kAssignableKey, const_ref);
  EXPECT_FALSE(h.GetProperty(kAssignableKey)->was_move_assigned());

  // New property from rvalue ref.
  h.ClearProperty(kAssignableKey);
  h.SetProperty(kAssignableKey, AssignableTestProperty{4});
  EXPECT_TRUE(h.GetProperty(kAssignableKey)->was_move_assigned());

  // New property from lvalue.
  h.ClearProperty(kAssignableKey);
  test_property = AssignableTestProperty{5};
  h.SetProperty(kAssignableKey, test_property);
  EXPECT_FALSE(h.GetProperty(kAssignableKey)->was_move_assigned());
}

TEST(PropertyTest, PropertyChangedEvent) {
  TestPropertyHandler h;

  // Verify that initially setting the value creates an event.
  const AssignableTestProperty kTestProperty{1};
  h.SetProperty(kAssignableKey, kTestProperty);
  EXPECT_EQ(1, h.num_events());

  // Verify that assigning by value sends an event.
  h.SetProperty(kAssignableKey, AssignableTestProperty{2});
  EXPECT_EQ(2, h.num_events());

  // Same as the above case, but with a const reference instead of a move.
  const AssignableTestProperty kTestProperty2{3};
  h.SetProperty(kAssignableKey, kTestProperty2);
  EXPECT_EQ(3, h.num_events());

  // Verify that clearing the property creates an event.
  h.ClearProperty(kAssignableKey);
  EXPECT_EQ(4, h.num_events());

  // Verify that setting a heap-allocated value also ticks the event counter.
  h.SetProperty(kAssignableKey, new AssignableTestProperty{4});
  EXPECT_EQ(5, h.num_events());

  // Verify that overwriting a heap-allocated value ticks the event counter.
  h.SetProperty(kAssignableKey, new AssignableTestProperty{5});
  EXPECT_EQ(6, h.num_events());
}

} // namespace test
} // namespace ui
