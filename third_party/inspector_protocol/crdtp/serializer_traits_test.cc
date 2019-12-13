// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "serializer_traits.h"

#include <array>
#include "test_platform.h"

// The purpose of this test is to ensure that the SerializerTraits<X>::Serialize
// methods invoke the appropriate functions from cbor.h; so, it's usually
// sufficient to compare with what cbor.h function invocations would produce,
// rather than making assertions on the specific bytes emitted by the
// SerializerTraits code.

namespace crdtp {
namespace {
TEST(SerializerTraits, Bool) {
  std::vector<uint8_t> out;
  SerializerTraits<bool>::Serialize(true, &out);
  SerializerTraits<bool>::Serialize(false, &out);
  EXPECT_THAT(out,
              testing::ElementsAre(cbor::EncodeTrue(), cbor::EncodeFalse()));
}

TEST(SerializerTraits, Double) {
  std::vector<uint8_t> out;
  SerializerTraits<double>::Serialize(1.00001, &out);

  std::vector<uint8_t> expected;
  cbor::EncodeDouble(1.00001, &expected);

  EXPECT_THAT(out, testing::ElementsAreArray(expected));
}

TEST(SerializerTraits, Int32) {
  std::vector<uint8_t> out;
  SerializerTraits<int32_t>::Serialize(42, &out);

  std::vector<uint8_t> expected;
  cbor::EncodeInt32(42, &expected);

  EXPECT_THAT(out, testing::ElementsAreArray(expected));
}

TEST(SerializerTraits, VectorOfInt32) {
  std::vector<int32_t> ints = {1, 2, 3};

  std::vector<uint8_t> out;
  SerializerTraits<std::vector<int32_t>>::Serialize(ints, &out);

  std::vector<uint8_t> expected;
  expected.push_back(cbor::EncodeIndefiniteLengthArrayStart());
  for (int32_t v : ints)
    cbor::EncodeInt32(v, &expected);
  expected.push_back(cbor::EncodeStop());

  EXPECT_THAT(out, testing::ElementsAreArray(expected));
}

// Foo is an example for a domain specific type.
class Foo : public Serializable {
 public:
  Foo(int32_t value) : value(value) {}

  int32_t value;

  void AppendSerialized(std::vector<uint8_t>* out) const override {
    // In production, this would be generated code which emits a
    // CBOR map that has STRING8 keys corresponding to the field names
    // and field values encoded using SerializerTraits::Serialize.
    //
    // For the test we simplify this drastically and just emit the field
    // value, for conveniently testing std::vector<std::unique_ptr<Foo>>,
    // as well as the convenience methods for raw pointer and unique_ptr.
    SerializerTraits<int32_t>::Serialize(value, out);
  }
};

TEST(SerializerTraits, VectorOfDomainSpecificType) {
  std::vector<std::unique_ptr<Foo>> foos;
  foos.push_back(std::make_unique<Foo>(1));
  foos.push_back(std::make_unique<Foo>(2));
  foos.push_back(std::make_unique<Foo>(3));

  std::vector<uint8_t> out;
  SerializerTraits<std::vector<std::unique_ptr<Foo>>>::Serialize(foos, &out);

  std::vector<uint8_t> expected;
  expected.push_back(cbor::EncodeIndefiniteLengthArrayStart());
  for (int32_t v : {1, 2, 3})
    cbor::EncodeInt32(v, &expected);
  expected.push_back(cbor::EncodeStop());

  EXPECT_THAT(out, testing::ElementsAreArray(expected));
}

TEST(SerializerTraits, ConvenienceMethods) {
  // Shows that SerializerTraits<Foo> allows unique_ptr and raw pointers.
  Foo foo(42);
  auto bar = std::make_unique<Foo>(21);

  std::vector<uint8_t> out;
  SerializerTraits<Foo>::Serialize(foo, &out);   // const Foo&
  SerializerTraits<Foo>::Serialize(bar, &out);   // std::unqique_ptr<Foo>
  SerializerTraits<Foo>::Serialize(&foo, &out);  // &Foo

  std::vector<uint8_t> expected;
  for (int32_t v : {42, 21, 42})
    cbor::EncodeInt32(v, &expected);

  EXPECT_THAT(out, testing::ElementsAreArray(expected));
}

TEST(SerializerTraits, UTF8String) {
  std::string msg = "Hello, 🌎.";

  std::vector<uint8_t> out;
  SerializerTraits<std::string>::Serialize(msg, &out);

  std::vector<uint8_t> expected;
  cbor::EncodeString8(SpanFrom(msg), &expected);

  EXPECT_THAT(out, testing::ElementsAreArray(expected));
}

// A trivial model of Exported.
// (see
// https://cs.chromium.org/chromium/src/out/Debug/gen/v8/include/inspector/Debugger.h).
struct Exported {
  std::string msg;
  void writeBinary(std::vector<uint8_t>* out) {
    cbor::EncodeString8(SpanFrom(msg), out);
  }
};

TEST(SerializerTraits, Exported) {
  Exported exported;
  exported.msg = "Hello, world.";

  std::vector<uint8_t> out;
  SerializerTraits<std::string>::Serialize(exported.msg, &out);

  std::vector<uint8_t> expected;
  cbor::EncodeString8(SpanFrom(exported.msg), &expected);

  EXPECT_THAT(out, testing::ElementsAreArray(expected));
}
}  // namespace
}  // namespace crdtp
