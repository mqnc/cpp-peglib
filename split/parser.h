
/*-----------------------------------------------------------------------------
 *  parser
 *---------------------------------------------------------------------------*/

class parser {
public:
  parser() = default;

  parser(const char *s, size_t n, const Rules &rules) {
    load_grammar(s, n, rules);
  }

  parser(std::string_view sv, const Rules &rules)
      : parser(sv.data(), sv.size(), rules) {}

  parser(const char *s, size_t n) : parser(s, n, Rules()) {}

  parser(std::string_view sv) : parser(sv.data(), sv.size(), Rules()) {}

  operator bool() { return grammar_ != nullptr; }

  bool load_grammar(const char *s, size_t n, const Rules &rules) {
    grammar_ = ParserGenerator::parse(s, n, rules, start_, log);
    return grammar_ != nullptr;
  }

  bool load_grammar(const char *s, size_t n) {
    return load_grammar(s, n, Rules());
  }

  bool load_grammar(std::string_view sv, const Rules &rules) {
    return load_grammar(sv.data(), sv.size(), rules);
  }

  bool load_grammar(std::string_view sv) {
    return load_grammar(sv.data(), sv.size());
  }

  bool parse_n(const char *s, size_t n, const char *path = nullptr) const {
    if (grammar_ != nullptr) {
      const auto &rule = (*grammar_)[start_];
      return post_process(s, n, rule.parse(s, n, path, log));
    }
    return false;
  }

  bool parse(std::string_view sv, const char *path = nullptr) const {
    return parse_n(sv.data(), sv.size(), path);
  }

  bool parse_n(const char *s, size_t n, std::any &dt,
               const char *path = nullptr) const {
    if (grammar_ != nullptr) {
      const auto &rule = (*grammar_)[start_];
      return post_process(s, n, rule.parse(s, n, dt, path, log));
    }
    return false;
  }

  bool parse(std::string_view sv, std::any &dt,
             const char *path = nullptr) const {
    return parse_n(sv.data(), sv.size(), dt, path);
  }

  template <typename T>
  bool parse_n(const char *s, size_t n, T &val,
               const char *path = nullptr) const {
    if (grammar_ != nullptr) {
      const auto &rule = (*grammar_)[start_];
      return post_process(s, n, rule.parse_and_get_value(s, n, val, path, log));
    }
    return false;
  }

  template <typename T>
  bool parse(std::string_view sv, T &val, const char *path = nullptr) const {
    return parse_n(sv.data(), sv.size(), val, path);
  }

  template <typename T>
  bool parse_n(const char *s, size_t n, std::any &dt, T &val,
               const char *path = nullptr) const {
    if (grammar_ != nullptr) {
      const auto &rule = (*grammar_)[start_];
      return post_process(s, n,
                          rule.parse_and_get_value(s, n, dt, val, path, log));
    }
    return false;
  }

  template <typename T>
  bool parse(const char *s, std::any &dt, T &val,
             const char *path = nullptr) const {
    auto n = strlen(s);
    return parse_n(s, n, dt, val, path);
  }

  Definition &operator[](const char *s) { return (*grammar_)[s]; }

  const Definition &operator[](const char *s) const { return (*grammar_)[s]; }

  std::vector<std::string> get_rule_names() const {
    std::vector<std::string> rules;
    for (auto &[name, _] : *grammar_) {
      rules.push_back(name);
    }
    return rules;
  }

  void enable_packrat_parsing() {
    if (grammar_ != nullptr) {
      auto &rule = (*grammar_)[start_];
      rule.enablePackratParsing = true;
    }
  }

  void enable_trace(TracerEnter tracer_enter, TracerLeave tracer_leave) {
    if (grammar_ != nullptr) {
      auto &rule = (*grammar_)[start_];
      rule.tracer_enter = tracer_enter;
      rule.tracer_leave = tracer_leave;
    }
  }

  template <typename T = Ast> parser &enable_ast() {
    for (auto &[_, rule] : *grammar_) {
      if (!rule.action) { add_ast_action<T>(rule); }
    }
    return *this;
  }

  template <typename T>
  std::shared_ptr<T> optimize_ast(std::shared_ptr<T> ast,
                                  bool opt_mode = true) const {
    return AstOptimizer(opt_mode, get_no_ast_opt_rules()).optimize(ast);
  }

  Log log;

private:
  bool post_process(const char *s, size_t n,
                    const Definition::Result &r) const {
    auto ret = r.ret && r.len == n;
    if (log && !ret) { r.error_info.output_log(log, s, n); }
    return ret && !r.recovered;
  }

  std::vector<std::string> get_no_ast_opt_rules() const {
    std::vector<std::string> rules;
    for (auto &[name, rule] : *grammar_) {
      if (rule.no_ast_opt) { rules.push_back(name); }
    }
    return rules;
  }

  std::shared_ptr<Grammar> grammar_;
  std::string start_;
};
