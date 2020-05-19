// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEDERATED_LEARNING_SIM_HASH_H_
#define COMPONENTS_FEDERATED_LEARNING_SIM_HASH_H_

#include <set>
#include <unordered_set>

namespace federated_learning {

// A 2^64 bit vector
class LargeBitVector {
 public:
  LargeBitVector();
  LargeBitVector(const LargeBitVector&);
  ~LargeBitVector();

  void SetBit(uint64_t pos);
  const std::set<uint64_t>& PositionsOfSetBits() const;

 private:
  // Sparse representation of a 2^64 bit vector. Each number in
  // |positions_of_set_bits_| represents the position of a bit that is being
  // set.
  std::set<uint64_t> positions_of_set_bits_;
};

// Set the two seeds used for generating the random gaussian.
void SetSeedsForTesting(uint64_t seed1, uint64_t seed2);

// SimHash a 2^64 bit vector to an |output_dimensions| bit number.
// |output_dimensions| must be greater than 0 and no greater than 64.
uint64_t SimHashBits(const LargeBitVector& input, size_t output_dimensions);

// SimHash a set of strings to an |output_dimensions| bit number.
// |output_dimensions| must be greater than 0 and no greater than 64.
uint64_t SimHashStrings(const std::unordered_set<std::string>& input,
                        size_t output_dimensions);

}  // namespace federated_learning

#endif  // COMPONENTS_FEDERATED_LEARNING_SIM_HASH_H_
