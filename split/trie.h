
/*-----------------------------------------------------------------------------
 *  Trie
 *---------------------------------------------------------------------------*/

class Trie {
public:
  Trie() = default;
  Trie(const Trie &) = default;

  Trie(const std::vector<std::string> &items) {
    for (const auto &item : items) {
      for (size_t len = 1; len <= item.size(); len++) {
        auto last = len == item.size();
        std::string_view sv(item.data(), len);
        auto it = dic_.find(sv);
        if (it == dic_.end()) {
          dic_.emplace(sv, Info{last, last});
        } else if (last) {
          it->second.match = true;
        } else {
          it->second.done = false;
        }
      }
    }
  }

  size_t match(const char *text, size_t text_len) const {
    size_t match_len = 0;
    auto done = false;
    size_t len = 1;
    while (!done && len <= text_len) {
      std::string_view sv(text, len);
      auto it = dic_.find(sv);
      if (it == dic_.end()) {
        done = true;
      } else {
        if (it->second.match) { match_len = len; }
        if (it->second.done) { done = true; }
      }
      len += 1;
    }
    return match_len;
  }

private:
  struct Info {
    bool done;
    bool match;
  };

  // TODO: Use unordered_map when heterogeneous lookup is supported in C++20
  // std::unordered_map<std::string, Info> dic_;
  std::map<std::string, Info, std::less<>> dic_;
};
