// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/privacy_budget/identifiable_token_builder.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/hash/legacy_hash.h"

namespace blink {

namespace {

// A big random prime. It's also the digest returned for an empty block.
constexpr uint64_t kChainingValueSeed = UINT64_C(6544625333304541877);

}  // namespace

const size_t IdentifiableTokenBuilder::kBlockSizeInBytes;

IdentifiableTokenBuilder::IdentifiableTokenBuilder()
    : partial_size_(0), chaining_value_(kChainingValueSeed) {}

IdentifiableTokenBuilder::IdentifiableTokenBuilder(
    const IdentifiableTokenBuilder& other) {
  partial_size_ = other.partial_size_;
  chaining_value_ = other.chaining_value_;
  memcpy(partial_, other.partial_, partial_size_);
}

IdentifiableTokenBuilder::IdentifiableTokenBuilder(ByteBuffer buffer)
    : IdentifiableTokenBuilder() {
  AddBytes(buffer);
}

IdentifiableTokenBuilder& IdentifiableTokenBuilder::AddBytes(
    ByteBuffer message) {
  DCHECK_LE(partial_size_, kBlockSizeInBytes);
  // Phase 1:
  //    Slurp in as much of the message as necessary if there's a partial block
  //    already assembled. Copying is expensive, so |partial_| is only involved
  //    when there's some left over bytes from a prior round.
  if (partial_size_ > 0 && !message.empty())
    message = SkimIntoPartial(message);

  if (message.empty())
    return *this;

  // Phase 2:
  //    Consume as many full blocks as possible from |message|.
  DCHECK_EQ(partial_size_, 0u);
  while (message.size() >= kBlockSizeInBytes) {
    DigestBlock(message.first<kBlockSizeInBytes>());
    message = message.subspan(kBlockSizeInBytes);
  }
  if (message.empty())
    return *this;

  // Phase 3:
  //    Whatever remains is stuffed into the partial buffer.
  message = SkimIntoPartial(message);
  DCHECK(message.empty());
  return *this;
}

IdentifiableTokenBuilder& IdentifiableTokenBuilder::AddAtomic(
    ByteBuffer buffer) {
  AlignPartialBuffer();
  AddValue(buffer.size_bytes());
  AddBytes(buffer);
  AlignPartialBuffer();
  return *this;
}

IdentifiableTokenBuilder& IdentifiableTokenBuilder::AddToken(
    IdentifiableToken token) {
  return AddValue(token.value_);
}

IdentifiableTokenBuilder::operator IdentifiableToken() const {
  return GetToken();
}

IdentifiableToken IdentifiableTokenBuilder::GetToken() const {
  if (partial_size_ == 0)
    return chaining_value_;

  return IdentifiableToken(
      base::legacy::CityHash64WithSeed(GetPartialBlock(), chaining_value_));
}

IdentifiableTokenBuilder::ByteBuffer IdentifiableTokenBuilder::SkimIntoPartial(
    ByteBuffer message) {
  DCHECK(!message.empty() && partial_size_ < kBlockSizeInBytes);
  const auto to_copy =
      std::min(kBlockSizeInBytes - partial_size_, message.size());
  memcpy(&partial_[partial_size_], message.data(), to_copy);
  partial_size_ += to_copy;
  if (partial_size_ == kBlockSizeInBytes)
    DigestBlock(TakeCompletedBlock());
  DCHECK_LE(partial_size_, kBlockSizeInBytes);
  return message.subspan(to_copy);
}

void IdentifiableTokenBuilder::AlignPartialBuffer() {
  const auto padding_to_add =
      kBlockAlignment - (partial_size_ % kBlockAlignment);
  if (padding_to_add == 0 || padding_to_add == kBlockAlignment)
    return;

  memset(partial_ + partial_size_, 0, padding_to_add);
  partial_size_ += padding_to_add;

  if (partial_size_ == kBlockSizeInBytes)
    DigestBlock(TakeCompletedBlock());

  DCHECK_LT(partial_size_, sizeof(partial_));
  DCHECK(IsAligned());
}

void IdentifiableTokenBuilder::DigestBlock(FullBlock block) {
  // partial_ should've been flushed before calling this.
  DCHECK_EQ(partial_size_, 0u);

  // The chaining value (initialized with the initialization vector
  // kChainingValueSeed) is only used for diffusion. There's no length padding
  // being done here since we aren't interested in second-preimage issues.
  //
  // There is a concern over hash flooding, but that's something the entire
  // study has more-or-less accepted for some metrics and is dealt with during
  // the analysis phase.
  chaining_value_ =
      base::legacy::CityHash64WithSeed(base::make_span(block), chaining_value_);
}

IdentifiableTokenBuilder::FullBlock
IdentifiableTokenBuilder::TakeCompletedBlock() {
  DCHECK_EQ(partial_size_, kBlockSizeInBytes);
  auto buffer = base::make_span(partial_);
  partial_size_ = 0;
  return buffer;
}

}  // namespace blink
