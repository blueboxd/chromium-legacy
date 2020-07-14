#ifndef __ZXCVBN__FEEDBACK_HPP
#define __ZXCVBN__FEEDBACK_HPP

#include <zxcvbn/common.hpp>

#include <string>
#include <vector>

namespace zxcvbn {

struct Feedback {
  std::string warning;
  std::vector<std::string> suggestions;
};

Feedback get_feedback(score_t score, const std::vector<Match> & sequence);

}

#endif
