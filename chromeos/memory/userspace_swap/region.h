// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_MEMORY_USERSPACE_SWAP_REGION_H_
#define CHROMEOS_MEMORY_USERSPACE_SWAP_REGION_H_

#include <sys/uio.h>
#include <cstdint>
#include <vector>

#include "base/containers/span.h"
#include "base/numerics/checked_math.h"
#include "base/strings/string_piece.h"

namespace chromeos {
namespace memory {
namespace userspace_swap {

// A region describes a block of memory.
class Region {
 public:
  uintptr_t address = 0;
  uintptr_t length = 0;

  Region() = default;
  Region(Region&&) = default;
  Region(const Region&) = default;
  Region& operator=(const Region&) = default;
  Region& operator=(Region&&) = default;

  // To avoid requiring callers to cast pointers or integral types to Regions,
  // we're flexible and allow any pointer type or any integral type. We convert
  // them to the types needed for a Region. This simplifies calling code
  // tremendously. Static asserts enforce that the types are valid.
  template <typename Address, typename Length>
  Region(Address* address, Length length)
      : address(reinterpret_cast<uintptr_t>(const_cast<Address*>(address))),
        length(length) {
    static_assert(std::is_integral<Length>::value,
                  "length must be an integral type");
    static_assert(sizeof(Length) <= sizeof(uintptr_t),
                  "Length cannot be longer than uint64_t");

    // Verify that the end of this region is valid and wouldn't overflow if we
    // added length to the address.
    CHECK((base::CheckedNumeric<uintptr_t>(this->address) + this->length)
              .IsValid());
  }

  template <typename Address, typename Length>
  Region(Address address, Length length)
      : Region(reinterpret_cast<void*>(address), length) {
    static_assert(sizeof(Address) <= sizeof(void*),
                  "Address cannot be longer than a pointer type");
  }

  template <typename Address>
  Region(Address address) : Region(address, 1) {
    static_assert(
        std::is_integral<Address>::value || std::is_pointer<Address>::value,
        "Adress must be integral or pointer type");
  }

  template <typename T>
  Region(const std::vector<T>& vec)
      : Region(vec.data(), vec.size() * sizeof(T)) {}

  // AsIovec will return the iovec representation of this Region.
  struct iovec AsIovec() const {
    return {.iov_base = reinterpret_cast<void*>(address), .iov_len = length};
  }

  base::StringPiece AsStringPiece() const {
    return base::StringPiece(reinterpret_cast<char*>(address), length);
  }

  template <typename T>
  base::span<T> AsSpan() const {
    return base::span<T>(reinterpret_cast<T*>(address), length);
  }

  bool operator<(const Region& other) const {
    // Because the standard library treats equality as !less(a,b) && !less(b,a)
    // our definition of less than will be that this has to be FULLY before
    // other. Overlapping regions are not allowed and are explicitly checked
    // before inserting by using find() any overlap would return equal, this
    // also has the property that you can search for a Region of length 1 to
    // find the mapping for a fault.
    return ((address + length - 1) < other.address);
  }
};

}  // namespace userspace_swap
}  // namespace memory
}  // namespace chromeos

#endif  // CHROMEOS_MEMORY_USERSPACE_SWAP_REGION_H_
