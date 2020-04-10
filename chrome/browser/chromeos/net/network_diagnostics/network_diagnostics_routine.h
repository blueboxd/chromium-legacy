// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_ROUTINE_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_ROUTINE_H_

#include <string>
#include <utility>

#include "base/bind.h"
#include "chrome/browser/chromeos/net/network_diagnostics/network_diagnostics.mojom.h"

namespace chromeos {
namespace network_diagnostics {

// Defines the key components of a network diagnostics routine. This class is
// expected to be implemented by every network diagnostics routine.
class NetworkDiagnosticsRoutine {
 public:
  // Structure of a routine's result.
  struct RoutineResult {
    RoutineResult() : routine_verdict(mojom::RoutineVerdict::kNotRun) {}
    ~RoutineResult() {}

    mojom::RoutineVerdict routine_verdict;
    std::string title;
  };

  NetworkDiagnosticsRoutine();
  virtual ~NetworkDiagnosticsRoutine();

  // Determines whether this test is capable of being run.
  virtual bool CanRun() = 0;

  // Runs the core logic of this routine. Takes in a callback that is invoked by
  // ExecuteTestCompletedCallback() once the results are analyzed by
  // AnalyzeResults().
  virtual void RunTest(base::OnceCallback<void(mojom::RoutineVerdict)>) = 0;

  // Determines the results gathered during the routine and populates
  // |routine_result_|. Runs the callback to update the clients with a verdict
  // (of type mojom::RoutineVerdict).
  virtual void AnalyzeResultsAndExecuteCallback();

 protected:
  void set_title(const std::string& title) { routine_result_.title = title; }
  std::string title() const { return routine_result_.title; }
  void set_verdict(mojom::RoutineVerdict routine_verdict) {
    routine_result_.routine_verdict = routine_verdict;
  }
  mojom::RoutineVerdict verdict() const {
    return routine_result_.routine_verdict;
  }
  void set_routine_completed_callback(
      base::OnceCallback<void(mojom::RoutineVerdict)>&
          routine_completed_callback) {
    routine_completed_callback_ = std::move(routine_completed_callback);
  }
  const base::OnceCallback<void(mojom::RoutineVerdict)>&
  routine_completed_callback() const {
    return routine_completed_callback_;
  }

 private:
  RoutineResult routine_result_;
  base::OnceCallback<void(mojom::RoutineVerdict)> routine_completed_callback_;
  friend class NetworkDiagnosticsRoutineTest;

  DISALLOW_COPY_AND_ASSIGN(NetworkDiagnosticsRoutine);
};

}  // namespace network_diagnostics
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_NETWORK_DIAGNOSTICS_ROUTINE_H_
