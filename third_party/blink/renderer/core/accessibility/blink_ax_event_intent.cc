// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/accessibility/blink_ax_event_intent.h"

#include <limits>

#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "ui/accessibility/ax_enums.mojom-blink.h"

namespace blink {

// Creates an empty (uninitialized) instance.
BlinkAXEventIntent::BlinkAXEventIntent() = default;

BlinkAXEventIntent::BlinkAXEventIntent(
    ax::mojom::blink::Command command,
    ax::mojom::blink::TextBoundary text_boundary,
    ax::mojom::blink::MoveDirection move_direction)
    : intent_(command, text_boundary, move_direction), is_initialized_(true) {}

BlinkAXEventIntent::BlinkAXEventIntent(WTF::HashTableDeletedValueType type)
    : is_initialized_(true), is_deleted_(true) {}

BlinkAXEventIntent::~BlinkAXEventIntent() = default;

BlinkAXEventIntent::BlinkAXEventIntent(const BlinkAXEventIntent& intent) =
    default;

BlinkAXEventIntent& BlinkAXEventIntent::operator=(
    const BlinkAXEventIntent& intent) = default;

bool operator==(const BlinkAXEventIntent& a, const BlinkAXEventIntent& b) {
  return BlinkAXEventIntentHash::GetHash(a) ==
         BlinkAXEventIntentHash::GetHash(b);
}

bool operator!=(const BlinkAXEventIntent& a, const BlinkAXEventIntent& b) {
  return !(a == b);
}

bool BlinkAXEventIntent::IsHashTableDeletedValue() const {
  return is_deleted_;
}

std::string BlinkAXEventIntent::ToString() const {
  if (!is_initialized())
    return "AXEventIntent(uninitialized)";
  if (IsHashTableDeletedValue())
    return "AXEventIntent(is_deleted)";
  return intent().ToString();
}

// static
unsigned int BlinkAXEventIntentHash::GetHash(const BlinkAXEventIntent& key) {
  // If the intent is uninitialized, it is not safe to rely on the memory being
  // initialized to zero, because any uninitialized field that might be
  // accidentally added in the future will produce a potentially non-zero memory
  // value especially in the hard to control "intent_" member.
  if (!key.is_initialized())
    return 0u;
  if (key.IsHashTableDeletedValue())
    return std::numeric_limits<unsigned>::max();

  unsigned hash = 0u;
  WTF::AddIntToHash(hash, static_cast<const unsigned>(key.intent().command));
  WTF::AddIntToHash(hash,
                    static_cast<const unsigned>(key.intent().text_boundary));
  WTF::AddIntToHash(hash,
                    static_cast<const unsigned>(key.intent().move_direction));
  return hash;
}

// static
bool BlinkAXEventIntentHash::Equal(const BlinkAXEventIntent& a,
                                   const BlinkAXEventIntent& b) {
  return a == b;
}

}  // namespace blink
