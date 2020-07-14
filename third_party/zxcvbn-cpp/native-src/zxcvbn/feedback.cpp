#include <zxcvbn/feedback.hpp>

#include <zxcvbn/frequency_lists.hpp>
#include <zxcvbn/optional.hpp>
#include <zxcvbn/scoring.hpp>
#include <zxcvbn/util.hpp>

#include <regex>

namespace zxcvbn {

static
optional::optional<Feedback> get_match_feedback(const Match & match, bool is_sole_match);

static
Feedback get_dictionary_match_feedback(const Match & match, bool is_sole_match);

Feedback get_feedback(score_t score,
                      const std::vector<Match> & sequence) {
  // starting feedback
  if (!sequence.size()) {
    return {
        "",
        {
            "Use a few words, avoid common phrases",
            "No need for symbols, digits, or uppercase letters",
        },
    };
  }

  // no feedback if score is good or great.
  if (score > 2) return {"", {}};

  // tie feedback to the longest match for longer sequences
  auto longest_match = sequence.begin();
  for (auto match = longest_match + 1; match != sequence.end(); ++match) {
    if (match->token.length() > longest_match->token.length()) {
      longest_match = match;
    }
  }

  auto maybe_feedback = get_match_feedback(*longest_match, sequence.size() == 1);
  auto extra_feedback = "Add another word or two. Uncommon words are better.";
  if (maybe_feedback) {
    auto & feedback = *maybe_feedback;

    feedback.suggestions.insert(maybe_feedback->suggestions.begin(),
                                extra_feedback);

    return feedback;
  }
  else {
    return {"", {extra_feedback}};
  }
}

optional::optional<Feedback> get_match_feedback(const Match & match_, bool is_sole_match) {
  switch (match_.get_pattern()) {
  case MatchPattern::DICTIONARY: {
    return get_dictionary_match_feedback(match_, is_sole_match);
  }

  case MatchPattern::SPATIAL: {
    auto & match = match_.get_spatial();
    auto warning = (match.turns == 1)
      ? "Straight rows of keys are easy to guess"
      : "Short keyboard patterns are easy to guess";

    return Feedback{warning, {
        "Use a longer keyboard pattern with more turns",
          }};
  }

  case MatchPattern::REPEAT: {
    auto warning = (match_.get_repeat().base_token.length() == 1)
      ? "Repeats like \"aaa\" are easy to guess"
      : "Repeats like \"abcabcabc\" are only slightly harder to guess than \"abc\"";

    return Feedback{warning, {
        "Avoid repeated words and characters",
          }};
  }

  case MatchPattern::SEQUENCE: {
    return Feedback{"Sequences like abc or 6543 are easy to guess",
        {"Avoid sequences"},
        };
  }

  case MatchPattern::REGEX: {
    auto & match = match_.get_regex();
    if (match.regex_tag == RegexTag::RECENT_YEAR) {
      return Feedback{"Recent years are easy to guess", {
          "Avoid recent years",
          "Avoid years that are associated with you",
        }};
    }
    break;
  }

  case MatchPattern::DATE: {
    return Feedback{"Dates are often easy to guess", {
        "Avoid dates and years that are associated with you",
      }};
  }
  default:
    break;
  }
  return optional::nullopt;
}

static
Feedback get_dictionary_match_feedback(const Match & match_, bool is_sole_match) {
  assert(match_.get_pattern() == MatchPattern::DICTIONARY);
  auto & match = match_.get_dictionary();
  auto warning = [&] {
    if (match.dictionary_tag == DictionaryTag::PASSWORDS) {
      if (is_sole_match and !match.l33t and !match.reversed) {
        if (match.rank <= 10) {
          return "This is a top-10 common password";
        }
        else if (match.rank <= 100) {
          return "This is a top-100 common password";
        }
        else {
          return "This is a very common password";
        }
      }
      else if (match_.guesses_log10 <= 4) {
        return "This is similar to a commonly used password";
      }
    }
    else if (match.dictionary_tag == DictionaryTag::ENGLISH_WIKIPEDIA) {
      if (is_sole_match) {
        return "A word by itself is easy to guess";
      }
    }
    else if (match.dictionary_tag == DictionaryTag::SURNAMES ||
             match.dictionary_tag == DictionaryTag::MALE_NAMES ||
             match.dictionary_tag == DictionaryTag::FEMALE_NAMES) {
      if (is_sole_match) {
        return "Names and surnames by themselves are easy to guess";
      }
      else {
        return "Common names and surnames are easy to guess";
      }
    }

    return "";
  }();

  std::vector<std::string> suggestions;
  auto & word = match_.token;
  if (std::regex_search(word, START_UPPER())) {
    suggestions.push_back("Capitalization doesn't help very much");
  } else if (std::regex_search(word, ALL_UPPER()) and
             // XXX: UTF-8
             util::ascii_lower(word) == word) {
    suggestions.push_back("All-uppercase is almost as easy to guess as all-lowercase");
  }

  if (match.reversed and match_.token.length() >= 4) {
    suggestions.push_back("Reversed words aren't much harder to guess");
  }
  if (match.l33t) {
    suggestions.push_back("Predictable substitutions like '@' instead of 'a' don't help very much");
  }

  return {warning, suggestions};
}

}
