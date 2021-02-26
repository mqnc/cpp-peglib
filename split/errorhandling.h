/*
 * Semantic predicate
 */
// Note: 'parse_error' exception class should be be used in sematic action
// handlers to reject the rule.
struct parse_error {
  parse_error() = default;
  parse_error(const char *s) : s_(s) {}
  const char *what() const { return s_.empty() ? nullptr : s_.data(); }

private:
  std::string s_;
};

/*
 * Parse result helper
 */
inline bool success(size_t len) { return len != static_cast<size_t>(-1); }

inline bool fail(size_t len) { return len == static_cast<size_t>(-1); }

/*
 * Log
 */
using Log = std::function<void(size_t, size_t, const std::string &)>;

/*
 * ErrorInfo
 */
struct ErrorInfo {
  const char *error_pos = nullptr;
  std::vector<std::pair<const char *, bool>> expected_tokens;
  const char *message_pos = nullptr;
  std::string message;
  mutable const char *last_output_pos = nullptr;

  void clear() {
    error_pos = nullptr;
    expected_tokens.clear();
    message_pos = nullptr;
    message.clear();
  }

  void add(const char *token, bool is_literal) {
    for (const auto &x : expected_tokens) {
      if (x.first == token && x.second == is_literal) { return; }
    }
    expected_tokens.push_back(std::make_pair(token, is_literal));
  }

  void output_log(const Log &log, const char *s, size_t n) const {
    if (message_pos) {
      if (message_pos > last_output_pos) {
        last_output_pos = message_pos;
        auto line = line_info(s, message_pos);
        std::string msg;
        if (auto unexpected_token = heuristic_error_token(s, n, message_pos);
            !unexpected_token.empty()) {
          msg = replace_all(message, "%t", unexpected_token);

          auto unexpected_char = unexpected_token.substr(
              0, codepoint_length(unexpected_token.data(),
                                  unexpected_token.size()));

          msg = replace_all(msg, "%c", unexpected_char);
        } else {
          msg = message;
        }
        log(line.first, line.second, msg);
      }
    } else if (error_pos) {
      if (error_pos > last_output_pos) {
        last_output_pos = error_pos;
        auto line = line_info(s, error_pos);

        std::string msg;
        if (expected_tokens.empty()) {
          msg = "syntax error.";
        } else {
          msg = "syntax error";

          // unexpected token
          if (auto unexpected_token = heuristic_error_token(s, n, error_pos);
              !unexpected_token.empty()) {
            msg += ", unexpected '";
            msg += unexpected_token;
            msg += "'";
          }

          auto first_item = true;
          size_t i = 0;
          while (i < expected_tokens.size()) {
            auto [token, is_literal] =
                expected_tokens[expected_tokens.size() - i - 1];

            // Skip rules start with '_'
            if (!is_literal && token[0] != '_') {
              msg += (first_item ? ", expecting " : ", ");
              if (is_literal) {
                msg += "'";
                msg += token;
                msg += "'";
              } else {
                msg += "<";
                msg += token;
                msg += ">";
              }
              first_item = false;
            }

            i++;
          }
          msg += ".";
        }

        log(line.first, line.second, msg);
      }
    }
  }

private:
  int cast_char(char c) const { return static_cast<unsigned char>(c); }

  std::string heuristic_error_token(const char *s, size_t n,
                                    const char *error_pos) const {
    auto len = n - std::distance(s, error_pos);
    if (len) {
      size_t i = 0;
      auto c = cast_char(error_pos[i++]);
      if (!std::ispunct(c) && !std::isspace(c)) {
        while (i < len && !std::ispunct(cast_char(error_pos[i])) &&
               !std::isspace(cast_char(error_pos[i]))) {
          i++;
        }
      }

      size_t count = 8;
      size_t j = 0;
      while (count > 0 && j < i) {
        j += codepoint_length(&error_pos[j], i - j);
        count--;
      }

      return escape_characters(error_pos, j);
    }
    return std::string();
  }

  std::string replace_all(std::string str, const std::string &from,
                          const std::string &to) const {
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos) {
      str.replace(pos, from.length(), to);
      pos += to.length();
    }
    return str;
  }
};
