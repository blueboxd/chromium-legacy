// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_TEST_CALLBACK_RECEIVER_H_
#define COMPONENTS_FEED_CORE_V2_TEST_CALLBACK_RECEIVER_H_

#include <utility>

#include "base/callback.h"
#include "base/optional.h"

namespace feed {
template <typename T>
class CallbackReceiver {
 public:
  void Done(T result) { result_ = std::move(result); }
  base::OnceCallback<void(T)> Bind() {
    return base::BindOnce(&CallbackReceiver::Done, base::Unretained(this));
  }

  void Clear() { result_ = base::nullopt; }

  base::Optional<T>& GetResult() { return result_; }

 private:
  base::Optional<T> result_;
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_TEST_CALLBACK_RECEIVER_H_
