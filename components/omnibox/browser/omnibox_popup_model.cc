// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_popup_model.h"

#include <algorithm>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_popup_view.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "third_party/icu/source/common/unicode/ubidi.h"
#include "ui/gfx/geometry/rect.h"

#if !defined(OS_ANDROID) && !defined(OS_IOS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#endif

///////////////////////////////////////////////////////////////////////////////
// OmniboxPopupModel::Selection

bool OmniboxPopupModel::Selection::operator==(const Selection& b) const {
  return line == b.line && state == b.state;
}

bool OmniboxPopupModel::Selection::operator!=(const Selection& b) const {
  return !operator==(b);
}

bool OmniboxPopupModel::Selection::IsChangeToKeyword(Selection from) const {
  return state == KEYWORD && from.state != KEYWORD;
}

OmniboxPopupModel::Selection OmniboxPopupModel::Selection::With(
    LineState new_state) const {
  return Selection(line, new_state);
}

///////////////////////////////////////////////////////////////////////////////
// OmniboxPopupModel

const size_t OmniboxPopupModel::kNoMatch = static_cast<size_t>(-1);

OmniboxPopupModel::OmniboxPopupModel(OmniboxPopupView* popup_view,
                                     OmniboxEditModel* edit_model,
                                     PrefService* pref_service)
    : view_(popup_view),
      edit_model_(edit_model),
      pref_service_(pref_service),
      selection_(kNoMatch, NORMAL),
      has_selected_match_(false) {
  edit_model->set_popup_model(this);
}

OmniboxPopupModel::~OmniboxPopupModel() = default;

// static
void OmniboxPopupModel::ComputeMatchMaxWidths(int contents_width,
                                              int separator_width,
                                              int description_width,
                                              int available_width,
                                              bool description_on_separate_line,
                                              bool allow_shrinking_contents,
                                              int* contents_max_width,
                                              int* description_max_width) {
  available_width = std::max(available_width, 0);
  *contents_max_width = std::min(contents_width, available_width);
  *description_max_width = std::min(description_width, available_width);

  // If the description is empty, or the contents and description are on
  // separate lines, each can get the full available width.
  if (!description_width || description_on_separate_line)
    return;

  // If we want to display the description, we need to reserve enough space for
  // the separator.
  available_width -= separator_width;
  if (available_width < 0) {
    *description_max_width = 0;
    return;
  }

  if (contents_width + description_width > available_width) {
    if (allow_shrinking_contents) {
      // Try to split the available space fairly between contents and
      // description (if one wants less than half, give it all it wants and
      // give the other the remaining space; otherwise, give each half).
      // However, if this makes the contents too narrow to show a significant
      // amount of information, give the contents more space.
      *contents_max_width = std::max(
          (available_width + 1) / 2, available_width - description_width);

      const int kMinimumContentsWidth = 300;
      *contents_max_width = std::min(
          std::min(std::max(*contents_max_width, kMinimumContentsWidth),
                   contents_width),
          available_width);
    }

    // Give the description the remaining space, unless this makes it too small
    // to display anything meaningful, in which case just hide the description
    // and let the contents take up the whole width.
    *description_max_width =
        std::min(description_width, available_width - *contents_max_width);
    const int kMinimumDescriptionWidth = 75;
    if (*description_max_width <
        std::min(description_width, kMinimumDescriptionWidth)) {
      *description_max_width = 0;
      // Since we're not going to display the description, the contents can have
      // the space we reserved for the separator.
      available_width += separator_width;
      *contents_max_width = std::min(contents_width, available_width);
    }
  }
}

// static
// Defines forward and backward ordering for possible line states.
OmniboxPopupModel::LineState OmniboxPopupModel::GetNextLineState(
    LineState state,
    Direction direction) {
  const bool button_row = OmniboxFieldTrial::IsSuggestionButtonRowEnabled();
  switch (direction) {
    case kForward:
      switch (state) {
        case NO_STATE:
          return HEADER_BUTTON_FOCUSED;
        case HEADER_BUTTON_FOCUSED:
          return NORMAL;
        case NORMAL:
          return button_row ? FOCUSED_BUTTON_KEYWORD : KEYWORD;
        case KEYWORD:
          return button_row ? FOCUSED_BUTTON_TAB_SWITCH : BUTTON_FOCUSED;
        case BUTTON_FOCUSED:
          return NO_STATE;
        case FOCUSED_BUTTON_KEYWORD:
          return FOCUSED_BUTTON_TAB_SWITCH;
        case FOCUSED_BUTTON_TAB_SWITCH:
          return FOCUSED_BUTTON_PEDAL;
        case FOCUSED_BUTTON_PEDAL:
          return NO_STATE;
        default:
          break;
      }
      break;
    case kBackward:
      switch (state) {
        case NO_STATE:
          return button_row ? FOCUSED_BUTTON_PEDAL : BUTTON_FOCUSED;
        case HEADER_BUTTON_FOCUSED:
          return NO_STATE;
        case NORMAL:
          return HEADER_BUTTON_FOCUSED;
        case KEYWORD:
          return NORMAL;
        case BUTTON_FOCUSED:
          return KEYWORD;
        case FOCUSED_BUTTON_KEYWORD:
          return NORMAL;
        case FOCUSED_BUTTON_TAB_SWITCH:
          return FOCUSED_BUTTON_KEYWORD;
        case FOCUSED_BUTTON_PEDAL:
          return FOCUSED_BUTTON_TAB_SWITCH;
        default:
          break;
      }
      break;
    default:
      break;
  }
  NOTREACHED();
  return OmniboxPopupModel::NO_STATE;
}

bool OmniboxPopupModel::IsOpen() const {
  return view_->IsOpen();
}

void OmniboxPopupModel::SetSelectedLine(size_t line,
                                        bool reset_to_default,
                                        bool force) {
  const AutocompleteResult& result = this->result();
  if (result.empty())
    return;

  // Cancel the query so the matches don't change on the user.
  autocomplete_controller()->Stop(false);

  if (line != kNoMatch)
    line = std::min(line, result.size() - 1);
  has_selected_match_ = !reset_to_default;

  if (line == selected_line() && !force)
    return;  // Nothing else to do.

  // We need to update selection before calling InvalidateLine(), since it will
  // use selection to determine how to draw.  We also need to update
  // |selection_.line| before calling OnPopupDataChanged(), so that when the
  // edit notifies its controller that something has changed, the controller
  // can get the correct updated data.
  const Selection old_selection = selection_;
  selection_ = Selection(line, NORMAL);
  view_->OnSelectionChanged(old_selection, selection_);

  if (line == kNoMatch)
    return;

  // Update the edit with the new data for this match.
  // TODO(pkasting): If |selection_.line| moves to the controller, this can be
  // eliminated and just become a call to the observer on the edit.
  base::string16 keyword;
  bool is_keyword_hint;
  TemplateURLService* service = edit_model_->client()->GetTemplateURLService();
  const AutocompleteMatch& match = result.match_at(line);
  match.GetKeywordUIState(service, &keyword, &is_keyword_hint);

  if (reset_to_default) {
    edit_model_->OnPopupDataChanged(match.inline_autocompletion,
                                    /*is_temporary_text=*/false, keyword,
                                    is_keyword_hint);
  } else {
    edit_model_->OnPopupDataChanged(match.fill_into_edit,
                                    /*is_temporary_text=*/true, keyword,
                                    is_keyword_hint);
  }
}

void OmniboxPopupModel::ResetToInitialState() {
  size_t new_line = result().default_match() ? 0 : kNoMatch;
  SetSelectedLine(new_line, true, false);
  view_->OnDragCanceled();
}

void OmniboxPopupModel::SetSelectedLineState(LineState state) {
  DCHECK(!result().empty());
  DCHECK_NE(kNoMatch, selected_line());

  const AutocompleteMatch& match = result().match_at(selected_line());
  GURL current_destination(match.destination_url);
  DCHECK((state != KEYWORD) || match.associated_keyword.get());

  if (state == BUTTON_FOCUSED)
    old_focused_url_ = current_destination;

  selection_ = Selection(selected_line(), state);
  view_->InvalidateLine(selected_line());

  if (state == BUTTON_FOCUSED) {
    edit_model_->SetAccessibilityLabel(match);
    view_->ProvideButtonFocusHint(selected_line());
  }
}

void OmniboxPopupModel::TryDeletingLine(size_t line) {
  // When called with line == selected_line(), we could use
  // GetInfoForCurrentText() here, but it seems better to try and delete the
  // actual selection, rather than any "in progress, not yet visible" one.
  if (line == kNoMatch)
    return;

  // Cancel the query so the matches don't change on the user.
  autocomplete_controller()->Stop(false);

  const AutocompleteMatch& match = result().match_at(line);
  if (match.SupportsDeletion()) {
    // Try to preserve the selection even after match deletion.
    const size_t old_selected_line = selected_line();
    const bool was_temporary_text = has_selected_match_;

    // This will synchronously notify both the edit and us that the results
    // have changed, causing both to revert to the default match.
    autocomplete_controller()->DeleteMatch(match);
    const AutocompleteResult& result = this->result();
    if (!result.empty() &&
        (was_temporary_text || old_selected_line != selected_line())) {
      // Move the selection to the next choice after the deleted one.
      // SetSelectedLine() will clamp to take care of the case where we deleted
      // the last item.
      // TODO(pkasting): Eventually the controller should take care of this
      // before notifying us, reducing flicker.  At that point the check for
      // deletability can move there too.
      SetSelectedLine(old_selected_line, false, true);
    }
  }
}

bool OmniboxPopupModel::IsStarredMatch(const AutocompleteMatch& match) const {
  auto* bookmark_model = edit_model_->client()->GetBookmarkModel();
  return bookmark_model && bookmark_model->IsBookmarked(match.destination_url);
}

void OmniboxPopupModel::OnResultChanged() {
  rich_suggestion_bitmaps_.clear();
  const AutocompleteResult& result = this->result();
  size_t old_selected_line = selected_line();
  has_selected_match_ = false;

  if (result.default_match()) {
    Selection selection(0, selected_line_state());

    // If selected line state was |BUTTON_FOCUSED| and nothing has changed,
    // leave it.
    const bool has_focused_match =
        selection.state == BUTTON_FOCUSED &&
        result.match_at(selection.line).has_tab_match;
    const bool has_changed =
        selection.line != old_selected_line ||
        result.match_at(selection.line).destination_url != old_focused_url_;
    if (!has_focused_match || has_changed) {
      selection.state = NORMAL;
    }
    selection_ = selection;
  } else {
    selection_ = Selection(kNoMatch, NORMAL);
  }

  bool popup_was_open = view_->IsOpen();
  view_->UpdatePopupAppearance();
  if (view_->IsOpen() != popup_was_open)
    edit_model_->controller()->OnPopupVisibilityChanged();
}

const SkBitmap* OmniboxPopupModel::RichSuggestionBitmapAt(
    int result_index) const {
  const auto iter = rich_suggestion_bitmaps_.find(result_index);
  if (iter == rich_suggestion_bitmaps_.end()) {
    return nullptr;
  }
  return &iter->second;
}

void OmniboxPopupModel::SetRichSuggestionBitmap(int result_index,
                                                const SkBitmap& bitmap) {
  rich_suggestion_bitmaps_[result_index] = bitmap;
  view_->UpdatePopupAppearance();
}

// Android and iOS have their own platform-specific icon logic.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
gfx::Image OmniboxPopupModel::GetMatchIcon(const AutocompleteMatch& match,
                                           SkColor vector_icon_color) {
  gfx::Image extension_icon =
      edit_model_->client()->GetIconIfExtensionMatch(match);
  // Extension icons are the correct size for non-touch UI but need to be
  // adjusted to be the correct size for touch mode.
  if (!extension_icon.IsEmpty())
    return edit_model_->client()->GetSizedIcon(extension_icon);

  // Get the favicon for navigational suggestions.
  if (!AutocompleteMatch::IsSearchType(match.type) &&
      match.type != AutocompleteMatchType::DOCUMENT_SUGGESTION) {
    // Because the Views UI code calls GetMatchIcon in both the layout and
    // painting code, we may generate multiple OnFaviconFetched callbacks,
    // all run one after another. This seems to be harmless as the callback
    // just flips a flag to schedule a repaint. However, if it turns out to be
    // costly, we can optimize away the redundant extra callbacks.
    gfx::Image favicon = edit_model_->client()->GetFaviconForPageUrl(
        match.destination_url,
        base::BindOnce(&OmniboxPopupModel::OnFaviconFetched,
                       weak_factory_.GetWeakPtr(), match.destination_url));

    // Extension icons are the correct size for non-touch UI but need to be
    // adjusted to be the correct size for touch mode.
    if (!favicon.IsEmpty())
      return edit_model_->client()->GetSizedIcon(favicon);
  }

  const auto& vector_icon_type = match.GetVectorIcon(IsStarredMatch(match));

  return edit_model_->client()->GetSizedIcon(vector_icon_type,
                                             vector_icon_color);
}
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

bool OmniboxPopupModel::SelectedLineIsTabSwitchSuggestion() {
  return selected_line() != kNoMatch &&
         result().match_at(selected_line()).IsTabSwitchSuggestion();
}

OmniboxPopupModel::Selection OmniboxPopupModel::GetNextSelection(
    Direction direction,
    Step step) const {
  if (result().empty()) {
    return selection_;
  }
  Selection next = selection_;
  const bool skip_keyword =
      !OmniboxFieldTrial::IsExperimentalKeywordModeEnabled() &&
      step == kStateOrNothing;

  // This block handles state transitions within the current line.
  if (step == kStateOrLine || step == kStateOrNothing) {
    LineState next_state =
        GetNextAvailableLineState(next, direction, skip_keyword);
    if (next_state != NO_STATE) {
      next.state = next_state;
      return next;
    }
    if (step == kStateOrNothing) {
      return next;
    }
  }

  // The rest handles stepping to other lines.
  const int size = result().size();
  const int line =
      step == kAllLines
          ? (direction == kForward ? (size - 1) : 0)
          : ((next.line + (direction == kForward ? 1 : (size - 1))) % size);
  next.line = line;

  // The new line state is always NORMAL for whole line transitions, except for
  // a kStateOrLine that transitions between different lines.
  LineState next_state = NORMAL;
  DCHECK_NE(kStateOrNothing, step);  // Already handled in previous block.
  if (step == kStateOrLine) {
    next_state = GetNextAvailableLineState(Selection(line, NO_STATE), direction,
                                           skip_keyword);
  }

  if (!OmniboxFieldTrial::IsSuggestionButtonRowEnabled() &&
      (step == kStateOrLine) && direction != kForward &&
      next_state == KEYWORD) {
    // When semi-stepping backward with no button row, skip over keyword.
    next_state =
        GetNextAvailableLineState(next.With(KEYWORD), direction, skip_keyword);
  }
  next.state = next_state;
  return next;
}

OmniboxPopupModel::Selection OmniboxPopupModel::StepSelection(
    Direction direction,
    Step step) {
  // This block steps the popup model, with special consideration
  // for existing keyword logic in the edit model, where AcceptKeyword and
  // ClearKeyword must be called before changing the selected line.
  const auto old_selection = selection();
  const auto new_selection = GetNextSelection(direction, step);
  if (new_selection.IsChangeToKeyword(old_selection)) {
    edit_model()->AcceptKeyword(metrics::OmniboxEventProto::TAB);
  } else if (old_selection.IsChangeToKeyword(new_selection)) {
    edit_model()->ClearKeyword();
  }
  SetSelection(new_selection);
  return selection_;
}

OmniboxPopupModel::Selection OmniboxPopupModel::ClearSelectionState() {
  // This is subtle. DCHECK in SetSelectedLineState will fail if there are no
  // results, which can happen when the popup gets closed. In that case, though,
  // the state is left as NORMAL.
  if (selection_.state != NORMAL) {
    SetSelectedLineState(NORMAL);
  }
  return selection_;
}

bool OmniboxPopupModel::IsSelectionAvailable(Selection selection) const {
  if (selection.line >= result().size()) {
    return false;
  }
  const auto& match = result().match_at(selection.line);
  // Skip rows that are hidden because their header is collapsed, unless the
  // user is trying to focus the header itself (which is still shown).
  if (selection.state != HEADER_BUTTON_FOCUSED &&
      match.suggestion_group_id.has_value() && pref_service_ &&
      omnibox::IsSuggestionGroupIdHidden(pref_service_,
                                         match.suggestion_group_id.value())) {
    return false;
  }

  switch (selection.state) {
    case NO_STATE:
      return false;
    case HEADER_BUTTON_FOCUSED: {
      // For the first match, if it a suggestion_group_id, then it has a header.
      if (selection.line == 0)
        return match.suggestion_group_id.has_value();

      // Otherwise, we only show headers that are distinct from the previous
      // match's header.
      const auto& previous_match = result().match_at(selection.line - 1);
      return match.suggestion_group_id.has_value() &&
             match.suggestion_group_id != previous_match.suggestion_group_id;
    }
    case NORMAL:
      return true;
    case KEYWORD:
      return match.associated_keyword != nullptr;
    case BUTTON_FOCUSED:
      // TODO(orinj): Here is an opportunity to clean up the presentational
      //  logic that pkasting wanted to take out of AutocompleteMatch. The view
      //  should be driven by the model, so this is really the place to decide.
      //  In other words, this duplicates logic within OmniboxResultView.
      //  This is the proper place. OmniboxResultView should refer to here.
      if (match.ShouldShowTabMatchButton())
        return true;
      if (base::FeatureList::IsEnabled(
              omnibox::kOmniboxSuggestionTransparencyOptions) &&
          match.SupportsDeletion()) {
        return true;
      }
      return false;
    case FOCUSED_BUTTON_KEYWORD:
      return match.associated_keyword != nullptr;
    case FOCUSED_BUTTON_TAB_SWITCH:
      return match.has_tab_match;
    case FOCUSED_BUTTON_PEDAL:
      return match.pedal != nullptr;
    default:
      break;
  }
  NOTREACHED();
  return false;
}

void OmniboxPopupModel::SetSelection(Selection selection) {
  if (selection.line != selection_.line) {
    SetSelectedLine(selection.line, false, false);
  }
  if (selection.state != selection_.state) {
    SetSelectedLineState(selection.state);
  }
}

OmniboxPopupModel::LineState OmniboxPopupModel::GetNextAvailableLineState(
    Selection from,
    Direction direction,
    bool skip_keyword) const {
  Selection to = from;
  do {
    to.state = GetNextLineState(to.state, direction);
    if (skip_keyword && to.state == KEYWORD) {
      to.state = GetNextLineState(to.state, direction);
    }
  } while (to.state != OmniboxPopupModel::NO_STATE &&
           !IsSelectionAvailable(to));
  return to.state;
}

void OmniboxPopupModel::OnFaviconFetched(const GURL& page_url,
                                         const gfx::Image& icon) {
  if (icon.IsEmpty() || !view_->IsOpen())
    return;

  // Notify all affected matches.
  for (size_t i = 0; i < result().size(); ++i) {
    auto& match = result().match_at(i);
    if (!AutocompleteMatch::IsSearchType(match.type) &&
        match.destination_url == page_url) {
      view_->OnMatchIconUpdated(i);
    }
  }
}
