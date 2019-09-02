// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test library for compression_script.
//
// The array will be put in the .data section which will be used as an argument
// for the script. We expect library to not crash and return the 55 as a
// result.

#include <numeric>
#include <vector>

// The first and last 4 values are randomly chosen bytes to be used by test as
// "magic" values indicating the beginning and the end of the array. This array
// needs to be an exported symbol to avoid compiler optimization which is why it
// is not declared as "const".
unsigned char array[18] = {151, 155, 125, 68, 1,  2,   3,  4,   5,
                           6,   7,   8,   9,  10, 236, 55, 136, 224};

extern "C" {
int GetSum();
}

int GetSum() {
  // We are using some c++ features here to better simulate a c++ library and
  // cause more code reach to catch potential memory errors.
  std::vector<int> sum_array(&array[4], &array[4] + 10);
  int sum = std::accumulate(sum_array.begin(), sum_array.end(), 0);
  // sum should be equal to 55.
  return sum;
}
