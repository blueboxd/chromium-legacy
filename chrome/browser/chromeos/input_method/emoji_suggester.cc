// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/emoji_suggester.h"

#include "base/files/file_util.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chromeos/services/ime/constants.h"

using input_method::InputMethodEngineBase;

namespace chromeos {

namespace {

const int MAX_CANDIDATE_SIZE = 5;
const base::FilePath::CharType kEmojiMapFilePath[] =
    FILE_PATH_LITERAL("/emoji/emoji-map.csv");

std::string ReadEmojiDataFromFile() {
  if (!base::DirectoryExists(base::FilePath(ime::kBundledInputMethodsDirPath)))
    return base::EmptyString();

  std::string emoji_data;
  base::FilePath::StringType path(ime::kBundledInputMethodsDirPath);
  path.append(kEmojiMapFilePath);
  if (!base::ReadFileToString(base::FilePath(path), &emoji_data))
    LOG(WARNING) << "Emoji map file missing.";
  return emoji_data;
}

std::vector<std::string> SplitString(const std::string& str,
                                     const std::string& delimiter) {
  return base::SplitString(str, delimiter, base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

std::string GetLastWord(const std::string& str) {
  size_t last_pos_to_search = str.length() - 1;
  // Search from second last char if last char is a white space.
  if (base::IsAsciiWhitespace(str.back()))
    last_pos_to_search = str.length() - 2;

  const auto space_before_last_word = str.find_last_of(" ", last_pos_to_search);

  // If not found, return the entire string up to the last position to search
  // else return the last word.
  return space_before_last_word == std::string::npos
             ? str.substr(0, last_pos_to_search + 1)
             : str.substr(space_before_last_word + 1,
                          last_pos_to_search - space_before_last_word);
}

// Create emoji suggestion's candidate window property.
InputMethodEngine::CandidateWindowProperty CreateProperty(int candidates_size) {
  InputMethodEngine::CandidateWindowProperty properties_out;
  properties_out.is_cursor_visible = true;
  properties_out.page_size = std::min(candidates_size, MAX_CANDIDATE_SIZE);
  properties_out.show_window_at_composition = false;
  properties_out.is_vertical = true;
  properties_out.is_auxiliary_text_visible = false;
  return properties_out;
}

}  // namespace

EmojiSuggester::EmojiSuggester(InputMethodEngine* engine) : engine_(engine) {
  LoadEmojiMap();
}

EmojiSuggester::~EmojiSuggester() = default;

void EmojiSuggester::LoadEmojiMap() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&ReadEmojiDataFromFile),
      base::BindOnce(&EmojiSuggester::OnEmojiDataLoaded,
                     weak_factory_.GetWeakPtr()));
}

void EmojiSuggester::OnEmojiDataLoaded(const std::string& emoji_data) {
  // Split data into lines.
  for (const auto& line : SplitString(emoji_data, "\n")) {
    // Get a word and a string of emojis from the line.
    const auto comma_pos = line.find_first_of(",");
    DCHECK(comma_pos != std::string::npos);
    std::string word = line.substr(0, comma_pos);
    std::string emojis = line.substr(comma_pos + 1);
    // Build emoji_map_ from splitting the string of emojis.
    emoji_map_[word] = SplitString(emojis, ";");
  }
}

void EmojiSuggester::OnFocus(int context_id) {
  context_id_ = context_id;
}

void EmojiSuggester::OnBlur() {
  context_id_ = -1;
}

SuggestionStatus EmojiSuggester::HandleKeyEvent(
    const InputMethodEngineBase::KeyboardEvent& event) {
  if (!suggestion_shown_)
    return SuggestionStatus::kNotHandled;
  SuggestionStatus status = SuggestionStatus::kNotHandled;
  std::string error;
  if (event.key == "Tab" || event.key == "Right" || event.key == "Enter") {
    suggestion_shown_ = false;
    engine_->CommitText(context_id_, candidates_[candidate_id_].value.c_str(),
                        &error);
    engine_->SetCandidateWindowVisible(false, &error);
    status = SuggestionStatus::kAccept;
  } else if (event.key == "Down") {
    candidate_id_ < static_cast<int>(candidates_.size()) - 1
        ? candidate_id_++
        : candidate_id_ = 0;
    engine_->SetCursorPosition(context_id_, candidate_id_, &error);
    status = SuggestionStatus::kBrowsing;
  } else if (event.key == "Up") {
    candidate_id_ > 0
        ? candidate_id_--
        : candidate_id_ = static_cast<int>(candidates_.size()) - 1;
    engine_->SetCursorPosition(context_id_, candidate_id_, &error);
    status = SuggestionStatus::kBrowsing;
  } else if (event.key == "Esc") {
    DismissSuggestion();
    suggestion_shown_ = false;
    status = SuggestionStatus::kDismiss;
  }
  if (!error.empty()) {
    LOG(ERROR) << "Fail to handle event. " << error;
  }
  return status;
}

bool EmojiSuggester::Suggest(const base::string16& text) {
  if (emoji_map_.empty() || !base::IsAsciiWhitespace(text[text.length() - 1]))
    return false;
  std::string last_word = GetLastWord(base::UTF16ToUTF8(text));
  if (!last_word.empty() && emoji_map_.count(last_word)) {
    ShowSuggestion(last_word);
    return true;
  }
  return false;
}

void EmojiSuggester::ShowSuggestion(const std::string& text) {
  std::string error;
  suggestion_shown_ = true;
  candidates_.clear();
  const std::vector<std::string>& candidates = emoji_map_.at(text);
  for (size_t i = 0; i < candidates.size(); i++) {
    candidates_.emplace_back();
    candidates_.back().value = candidates[i];
    candidates_.back().id = i;
    candidates_.back().label = base::UTF16ToUTF8(base::FormatNumber(i + 1));
  }
  engine_->SetCandidates(context_id_, candidates_, &error);

  candidate_id_ = 0;
  engine_->SetCandidateWindowProperty(
      CreateProperty(static_cast<int>(candidates_.size())));
  engine_->SetCandidateWindowVisible(true, &error);
  engine_->SetCursorPosition(context_id_, candidate_id_, &error);
  if (!error.empty()) {
    LOG(ERROR) << "Fail to show suggestion. " << error;
  }
}

void EmojiSuggester::DismissSuggestion() {
  std::string error;
  suggestion_shown_ = false;
  engine_->SetCandidateWindowVisible(false, &error);
  if (!error.empty()) {
    LOG(ERROR) << "Failed to dismiss suggestion. " << error;
  }
}

AssistiveType EmojiSuggester::GetProposeActionType() {
  return AssistiveType::kEmoji;
}

}  // namespace chromeos
