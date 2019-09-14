/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_ANIMATION_SMIL_TIME_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_ANIMATION_SMIL_TIME_H_

#include <ostream>

#include "base/time/time.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

struct SMILInterval;

class SMILTime {
  DISALLOW_NEW();

 public:
  constexpr SMILTime() : time_(0) {}
  constexpr SMILTime(double time) : time_(time) {}

  static constexpr SMILTime Unresolved() {
    return std::numeric_limits<double>::quiet_NaN();
  }
  static constexpr SMILTime Indefinite() {
    return std::numeric_limits<double>::infinity();
  }
  static constexpr SMILTime Earliest() {
    return -std::numeric_limits<double>::infinity();
  }

  // Used for computing progress. Don't use for anything else.
  double InternalValueAsDouble() const { return time_; }
  double InSecondsF() const { return time_; }
  int64_t InMicroseconds() const {
    return time_ * base::Time::kMicrosecondsPerSecond;
  }

  bool IsFinite() const { return std::isfinite(time_); }
  bool IsIndefinite() const { return std::isinf(time_); }
  bool IsUnresolved() const { return std::isnan(time_); }

  SMILTime operator+(SMILTime other) const { return time_ + other.time_; }
  SMILTime operator-(SMILTime other) const { return time_ - other.time_; }
  // So multiplying times does not make too much sense but SMIL defines it for
  // duration * repeatCount
  SMILTime operator*(SMILTime other) const;
  // Similarly for divisions/modulo. (Used primarily for computing interval
  // progress/repeats.)
  int64_t operator/(SMILTime other) const {
    return int64_t(time_ / other.time_);
  }
  SMILTime operator%(SMILTime other) const {
    DCHECK(IsFinite());
    DCHECK(other.IsFinite());
    return SMILTime(fmod(time_, other.time_));
  }

  bool operator==(SMILTime other) const {
    return (IsUnresolved() && other.IsUnresolved()) || time_ == other.time_;
  }
  explicit operator bool() const { return IsFinite() && time_; }
  bool operator!=(SMILTime other) const { return !this->operator==(other); }

  // Ordering of SMILTimes has to follow: finite < indefinite (Inf) < unresolved
  // (NaN). The first comparison is handled by IEEE754 but NaN values must be
  // checked explicitly to guarantee that unresolved is ordered correctly too.
  bool operator>(SMILTime other) const {
    return IsUnresolved() || time_ > other.time_;
  }
  bool operator<(SMILTime other) const { return other.operator>(*this); }
  bool operator>=(SMILTime other) const {
    return this->operator>(other) || this->operator==(other);
  }
  bool operator<=(SMILTime other) const {
    return this->operator<(other) || this->operator==(other);
  }

 private:
  friend bool operator!=(const SMILInterval& a, const SMILInterval& b);
  friend struct SMILTimeHash;

  double time_;
};

class SMILTimeWithOrigin {
  DISALLOW_NEW();

 public:
  enum Origin { kParserOrigin, kScriptOrigin };

  SMILTimeWithOrigin() : origin_(kParserOrigin) {}

  SMILTimeWithOrigin(const SMILTime& time, Origin origin)
      : time_(time), origin_(origin) {}

  const SMILTime& Time() const { return time_; }
  bool OriginIsScript() const { return origin_ == kScriptOrigin; }
  bool HasSameOrigin(SMILTimeWithOrigin other) const {
    return origin_ == other.origin_;
  }

 private:
  SMILTime time_;
  Origin origin_;
};

std::ostream& operator<<(std::ostream& os, SMILTime time);

inline bool operator<(const SMILTimeWithOrigin& a,
                      const SMILTimeWithOrigin& b) {
  return a.Time() < b.Time();
}

// An interval of SMILTimes.
// "...the begin time of the interval is included in the interval, but the end
// time is excluded from the interval."
// (https://www.w3.org/TR/SMIL3/smil-timing.html#q101)
struct SMILInterval {
  DISALLOW_NEW();
  SMILInterval() = default;
  SMILInterval(const SMILTime& begin, const SMILTime& end)
      : begin(begin), end(end) {}

  bool IsResolved() const { return begin.IsFinite(); }
  bool BeginsAfter(SMILTime time) const { return time < begin; }
  bool BeginsBefore(SMILTime time) const { return !BeginsAfter(time); }
  bool EndsAfter(SMILTime time) const {
    DCHECK(IsResolved());
    return time < end;
  }
  bool EndsBefore(SMILTime time) const { return !EndsAfter(time); }
  bool Contains(SMILTime time) const {
    return BeginsBefore(time) && EndsAfter(time);
  }

  SMILTime begin;
  SMILTime end;
};

inline bool operator!=(const SMILInterval& a, const SMILInterval& b) {
  // Compare the "raw" values since the operator!= for SMILTime always return
  // true for non-finite times.
  return a.begin.time_ != b.begin.time_ || a.end.time_ != b.end.time_;
}

struct SMILTimeHash {
  STATIC_ONLY(SMILTimeHash);
  static unsigned GetHash(const SMILTime& key) {
    return WTF::FloatHash<double>::GetHash(key.time_);
  }
  static bool Equal(const SMILTime& a, const SMILTime& b) {
    return WTF::FloatHash<double>::Equal(a.time_, b.time_);
  }
  static const bool safe_to_compare_to_empty_or_deleted = true;
};

}  // namespace blink

namespace WTF {

template <>
struct DefaultHash<blink::SMILTime> {
  typedef blink::SMILTimeHash Hash;
};

template <>
struct HashTraits<blink::SMILTime> : GenericHashTraits<blink::SMILTime> {
  static blink::SMILTime EmptyValue() { return blink::SMILTime::Unresolved(); }
  static void ConstructDeletedValue(blink::SMILTime& slot, bool) {
    slot = blink::SMILTime::Earliest();
  }
  static bool IsDeletedValue(blink::SMILTime value) {
    return value == blink::SMILTime::Earliest();
  }
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_ANIMATION_SMIL_TIME_H_
