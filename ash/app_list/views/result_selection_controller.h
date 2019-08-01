// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_RESULT_SELECTION_CONTROLLER_H_
#define ASH_APP_LIST_VIEWS_RESULT_SELECTION_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/app_list/app_list_export.h"
#include "ash/app_list/views/search_result_base_view.h"
#include "ash/app_list/views/search_result_container_view.h"
#include "base/macros.h"

namespace app_list {

class SearchResultContainerView;

// This alias is intended to clarify the intended use of this class within the
// context of this controller.
using ResultSelectionModel = std::vector<SearchResultContainerView*>;

// Stores and organizes the details for the 'coordinates' of the selected
// result. This includes all information to determine exactly where a result is,
// including both inter- and intra-container details, along with the traversal
// direction for the container.
struct APP_LIST_EXPORT ResultLocationDetails {
  ResultLocationDetails(int container_index,
                        int container_count,
                        int result_index,
                        int result_count,
                        bool container_is_horizontal);

  bool operator==(const ResultLocationDetails& other) const;
  bool operator!=(const ResultLocationDetails& other) const;

  // True if the result is the first(0th) in its container
  bool is_first_result() const { return result_index == 0; }

  // True if the result is the last in its container
  bool is_last_result() const { return result_index == (result_count - 1); }

  // Index of the container within the overall nest of containers.
  int container_index = 0;

  // Number of containers to traverse among.
  int container_count = 0;

  // Index of the result within the list of results inside a container.
  int result_index = 0;

  // Number of results within the current container.
  int result_count = 0;

  // Whether the container is horizontally traversable.
  bool container_is_horizontal = false;
};

// A controller class to manage result selection across containers.
class APP_LIST_EXPORT ResultSelectionController {
 public:
  explicit ResultSelectionController(
      const ResultSelectionModel* result_container_views);
  ~ResultSelectionController();

  // Returns the currently selected result.
  SearchResultBaseView* selected_result() { return selected_result_; }

  // Returns the |ResultLocationDetails| object for the |selected_result_|.
  ResultLocationDetails* selected_location_details() {
    return selected_location_details_.get();
  }

  // Calls |SetSelection| using the result of |GetNextResultLocation|. Returns
  // true if selection was changed.
  bool MoveSelection(const ui::KeyEvent& event);

  // Resets the selection to the first result.
  void ResetSelection();

  // Clears the |selected_result_|, |selected_location_details_|.
  void ClearSelection();

 private:
  // Calls |GetNextResultLocationForLocation| using |selected_location_details_|
  // as the location
  ResultLocationDetails GetNextResultLocation(const ui::KeyEvent& event);

  // Logic for next is separated for modular use. You can ask for the "next"
  // location to be generated using any starting location/event combination.
  ResultLocationDetails GetNextResultLocationForLocation(
      const ui::KeyEvent& event,
      const ResultLocationDetails& location);

  // Sets the current selection to the provided |location|.
  void SetSelection(const ResultLocationDetails& location,
                    bool reverse_tab_order);

  SearchResultBaseView* GetResultAtLocation(
      const ResultLocationDetails& location);

  // Updates a |ResultLocationDetails| to a new container, updating most
  // attributes based on |result_selection_model_|.
  void ChangeContainer(ResultLocationDetails* location_details,
                       int new_container_index);

  // Container views to be traversed by this controller.
  // Owned by |SearchResultPageView|.
  const ResultSelectionModel* result_selection_model_;

  // Returns true if the container at the given |index| within
  // |result_selection_model_| responds true to
  // |SearchResultContainerView|->|IsHorizontallyTraversable|.
  bool IsContainerAtIndexHorizontallyTraversable(int index) const;

  // The currently selected result view
  SearchResultBaseView* selected_result_ = nullptr;

  // The |ResultLocationDetails| for the currently selected result view
  std::unique_ptr<ResultLocationDetails> selected_location_details_;

  DISALLOW_COPY_AND_ASSIGN(ResultSelectionController);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_VIEWS_RESULT_SELECTION_CONTROLLER_H_
