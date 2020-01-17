// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/opentype/font_settings.h"

#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hasher.h"

namespace blink {

uint32_t AtomicStringToFourByteTag(AtomicString tag) {
  DCHECK_EQ(tag.length(), 4u);
  return (((tag[0]) << 24) | ((tag[1]) << 16) | ((tag[2]) << 8) | (tag[3]));
}

AtomicString FourByteTagToAtomicString(uint32_t tag) {
  constexpr size_t tag_size = 4;
  LChar tag_string[tag_size] = {(tag >> 24) & 0xFF, (tag >> 16) & 0xFF,
                                (tag >> 8) & 0xFF, tag & 0xFF};
  return AtomicString(tag_string, tag_size);
}

unsigned FontVariationSettings::GetHash() const {
  unsigned computed_hash = size() ? 5381 : 0;
  unsigned num_features = size();
  for (unsigned i = 0; i < num_features; ++i) {
    StringHasher string_hasher;
    const AtomicString& tag = at(i).Tag();
    for (unsigned j = 0; j < tag.length(); j++) {
      string_hasher.AddCharacter(tag[j]);
    }
    WTF::AddIntToHash(computed_hash, string_hasher.GetHash());
    WTF::AddFloatToHash(computed_hash, at(i).Value());
  }
  return computed_hash;
}

}  // namespace blink
