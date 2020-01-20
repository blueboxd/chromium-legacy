// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_IDL_TYPES_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_IDL_TYPES_H_

#include <type_traits>

#include "base/optional.h"
#include "base/template_util.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types_base.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_string_resource.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class EventListener;
class ScriptPromise;
class ScriptValue;

// The type names below are named as "IDL" prefix + Web IDL type name.
// https://heycam.github.io/webidl/#dfn-type-name

// Boolean
struct IDLBoolean final : public IDLBaseHelper<bool> {};

// Integers
struct IDLByte final : public IDLBaseHelper<int8_t> {};
struct IDLOctet final : public IDLBaseHelper<uint8_t> {};
struct IDLShort final : public IDLBaseHelper<int16_t> {};
struct IDLUnsignedShort final : public IDLBaseHelper<uint16_t> {};
struct IDLLong final : public IDLBaseHelper<int32_t> {};
struct IDLUnsignedLong final : public IDLBaseHelper<uint32_t> {};
struct IDLLongLong final : public IDLBaseHelper<int64_t> {};
struct IDLUnsignedLongLong final : public IDLBaseHelper<uint64_t> {};

// [Clamp] Integers
struct IDLByteClamp final : public IDLBaseHelper<int8_t> {};
struct IDLOctetClamp final : public IDLBaseHelper<uint8_t> {};
struct IDLShortClamp final : public IDLBaseHelper<int16_t> {};
struct IDLUnsignedShortClamp final : public IDLBaseHelper<uint16_t> {};
struct IDLLongClamp final : public IDLBaseHelper<int32_t> {};
struct IDLUnsignedLongClamp final : public IDLBaseHelper<uint32_t> {};
struct IDLLongLongClamp final : public IDLBaseHelper<int64_t> {};
struct IDLUnsignedLongLongClamp final : public IDLBaseHelper<uint64_t> {};

// [EnforceRange] Integers
struct IDLByteEnforceRange final : public IDLBaseHelper<int8_t> {};
struct IDLOctetEnforceRange final : public IDLBaseHelper<uint8_t> {};
struct IDLShortEnforceRange final : public IDLBaseHelper<int16_t> {};
struct IDLUnsignedShortEnforceRange final : public IDLBaseHelper<uint16_t> {};
struct IDLLongEnforceRange final : public IDLBaseHelper<int32_t> {};
struct IDLUnsignedLongEnforceRange final : public IDLBaseHelper<uint32_t> {};
struct IDLLongLongEnforceRange final : public IDLBaseHelper<int64_t> {};
struct IDLUnsignedLongLongEnforceRange final : public IDLBaseHelper<uint64_t> {
};

// Strings
// The "Base" classes are always templatized and require users to specify how JS
// null and/or undefined are supposed to be handled.
template <V8StringResourceMode Mode>
struct IDLByteStringBase final : public IDLBaseHelper<String> {};
template <V8StringResourceMode Mode>
struct IDLStringBase final : public IDLBaseHelper<String> {};
template <V8StringResourceMode Mode>
struct IDLUSVStringBase final : public IDLBaseHelper<String> {};

// Define non-template versions of the above for simplicity.
using IDLByteString = IDLByteStringBase<V8StringResourceMode::kDefaultMode>;
using IDLString = IDLStringBase<V8StringResourceMode::kDefaultMode>;
using IDLUSVString = IDLUSVStringBase<V8StringResourceMode::kDefaultMode>;

// Nullable strings
using IDLByteStringOrNull =
    IDLByteStringBase<V8StringResourceMode::kTreatNullAndUndefinedAsNullString>;
using IDLStringOrNull =
    IDLStringBase<V8StringResourceMode::kTreatNullAndUndefinedAsNullString>;
using IDLUSVStringOrNull =
    IDLUSVStringBase<V8StringResourceMode::kTreatNullAndUndefinedAsNullString>;

// [TreatNullAs] Strings
using IDLStringTreatNullAsEmptyString =
    IDLStringBase<V8StringResourceMode::kTreatNullAsEmptyString>;

// Strings for the new bindings generator

namespace bindings {

enum class IDLStringConvMode {
  kDefault,
  kNullable,
  kTreatNullAsEmptyString,
};

}  // namespace bindings

// ByteString
template <bindings::IDLStringConvMode mode>
struct IDLByteStringBaseV2 final : public IDLBaseHelper<String> {};
using IDLByteStringV2 =
    IDLByteStringBaseV2<bindings::IDLStringConvMode::kDefault>;

// DOMString
template <bindings::IDLStringConvMode mode>
struct IDLStringBaseV2 final : public IDLBaseHelper<String> {};
using IDLStringV2 = IDLStringBaseV2<bindings::IDLStringConvMode::kDefault>;
using IDLStringTreatNullAsV2 =
    IDLStringBaseV2<bindings::IDLStringConvMode::kTreatNullAsEmptyString>;

// USVString
template <bindings::IDLStringConvMode mode>
struct IDLUSVStringBaseV2 final : public IDLBaseHelper<String> {};
using IDLUSVStringV2 =
    IDLUSVStringBaseV2<bindings::IDLStringConvMode::kDefault>;

// Double
struct IDLDouble final : public IDLBaseHelper<double> {};
struct IDLUnrestrictedDouble final : public IDLBaseHelper<double> {};

// Float
struct IDLFloat final : public IDLBaseHelper<float> {};
struct IDLUnrestrictedFloat final : public IDLBaseHelper<float> {};

// object
struct IDLObject final : public IDLBaseHelper<ScriptValue> {};

// Promise
struct IDLPromise final : public IDLBaseHelper<ScriptPromise> {};

// Sequence
template <typename T>
struct IDLSequence final : public IDLBase {
  using ImplType =
      VectorOf<std::remove_pointer_t<typename NativeValueTraits<T>::ImplType>>;
};

// Frozen array types
template <typename T>
using IDLArray = IDLSequence<T>;

// Record
template <typename Key, typename Value>
struct IDLRecord final : public IDLBase {
  static_assert(std::is_same<typename Key::ImplType, String>::value,
                "IDLRecord keys must be of a WebIDL string type");
  static_assert(
      std::is_same<typename NativeValueTraits<Key>::ImplType, String>::value,
      "IDLRecord keys must be of a WebIDL string type");

  using ImplType = VectorOfPairs<
      String,
      std::remove_pointer_t<typename NativeValueTraits<Value>::ImplType>>;
};

// Nullable
template <typename InnerType>
struct IDLNullable final : public IDLBase {
  using ImplType = std::conditional_t<
      NativeValueTraits<InnerType>::has_null_value,
      typename NativeValueTraits<InnerType>::ImplType,
      base::Optional<typename NativeValueTraits<InnerType>::ImplType>>;
};

// Date
struct IDLDate final : public IDLBaseHelper<base::Time> {};

// EventHandler types
struct IDLEventHandler final : public IDLBaseHelper<EventListener*> {};
struct IDLOnBeforeUnloadEventHandler final
    : public IDLBaseHelper<EventListener*> {};
struct IDLOnErrorEventHandler final : public IDLBaseHelper<EventListener*> {};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_IDL_TYPES_H_
