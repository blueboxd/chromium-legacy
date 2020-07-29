// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMANDER_COMMANDER_VIEW_MODEL_H_
#define CHROME_BROWSER_UI_COMMANDER_COMMANDER_VIEW_MODEL_H_

#include <vector>

#include "base/strings/string16.h"
#include "ui/gfx/range/range.h"

namespace commander {

// A view model for a single command to be presented by the commander UI.
struct CommandItemViewModel {
 public:
  CommandItemViewModel(base::string16& title,
                       std::vector<gfx::Range>& matched_ranges);
  ~CommandItemViewModel();
  CommandItemViewModel(const CommandItemViewModel& other);
  // The displayed title of the command.
  base::string16 title;
  // The locations of spans in |title| that should be emphasised to
  // indicate to the user why the command was surfaced for their input.
  std::vector<gfx::Range> matched_ranges;
};

// A view model for a set of results to be presented by the commander UI.
struct CommanderViewModel {
  enum Action {
    // Display the items in |items|.
    kDisplayResults,
    // Close the UI. Typically sent after a command has been executed.
    kClose,
    // Clear the input and requery. Sent when the user has selected a command
    // that needs further user input.
    kPrompt,
  };

 public:
  CommanderViewModel();
  ~CommanderViewModel();
  CommanderViewModel(const CommanderViewModel& other);
  // An identifier for this result set. See discussion in
  // commander_backend.h for more details.
  int result_set_id;
  // A pre-ranked list of items to display. Can be empty if there are
  // no results, or |action| is not kDisplayResults.
  std::vector<CommandItemViewModel> items;
  // The action the view should take in response to receiving this view model.
  Action action;
};

}  // namespace commander

#endif  // CHROME_BROWSER_UI_COMMANDER_COMMANDER_VIEW_MODEL_H_
