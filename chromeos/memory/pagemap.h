// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_MEMORY_PAGEMAP_H_
#define CHROMEOS_MEMORY_PAGEMAP_H_

#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {
namespace memory {

// Pagemap fetches pagemap entries from procfs for a process.
class CHROMEOS_EXPORT Pagemap {
 public:
  // For more information on the Pagemap layout see the kernel documentation at
  // https://www.kernel.org/doc/Documentation/vm/pagemap.txt
  struct PagemapEntry {
    uint8_t swap_type : 5;
    uint64_t swap_offset : 50;
    bool pte_soft_dirty : 1;
    bool page_exclusively_mapped : 1;
    uint8_t : 4;  // these bits are unused
    bool page_file_or_shared_anon : 1;
    bool page_swapped : 1;
    bool page_present : 1;
  } __attribute__((packed));

  static_assert(sizeof(PagemapEntry) == sizeof(uint64_t),
                "PagemapEntry is expected to be 8 bytes");

  explicit Pagemap(pid_t pid);
  ~Pagemap();

  bool IsValid() const;

  // GetEntries will populate |entries| for the memory region specified.
  // It is required that |address| be page aligned and |length| must always be a
  // page length multiple. Additionally, |entries| is expected to be sized to
  // the number of pages in the range specified. If it's not properly sized it
  // will be resized to the appropriate length for the caller.
  bool GetEntries(uint64_t address,
                  uint64_t length,
                  std::vector<PagemapEntry>* entries);

 private:
  friend class PagemapTest;

  base::ScopedFD fd_;

  DISALLOW_COPY_AND_ASSIGN(Pagemap);
};

}  // namespace memory
}  // namespace chromeos

#endif  // CHROMEOS_MEMORY_PAGEMAP_H_
