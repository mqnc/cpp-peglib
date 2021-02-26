
  std::shared_ptr<Grammar> perform_core(const char *s, size_t n,
                                        const Rules &rules, std::string &start,
                                        Log log) {
    Data data;
    auto &grammar = *data.grammar;

    // Built-in macros
    {
      // `%recover`
      {
        auto &rule = grammar[RECOVER_DEFINITION_NAME];
        rule <= ref(grammar, "x", "", false, {});
        rule.name = RECOVER_DEFINITION_NAME;
        rule.s_ = "[native]";
        rule.ignoreSemanticValue = true;
        rule.is_macro = true;
        rule.params = {"x"};
      }
    }

    std::any dt = &data;
    auto r = g["Grammar"].parse(s, n, dt, nullptr, log);

    if (!r.ret) {
      if (log) {
        if (r.error_info.message_pos) {
          auto line = line_info(s, r.error_info.message_pos);
          log(line.first, line.second, r.error_info.message);
        } else {
          auto line = line_info(s, r.error_info.error_pos);
          log(line.first, line.second, "syntax error");
        }
      }
      return nullptr;
    }

    // User provided rules
    for (const auto &x : rules) {
      auto name = x.first;
      auto ignore = false;
      if (!name.empty() && name[0] == '~') {
        ignore = true;
        name.erase(0, 1);
      }
      if (!name.empty()) {
        auto &rule = grammar[name];
        rule <= x.second;
        rule.name = name;
        rule.ignoreSemanticValue = ignore;
      }
    }

    // Check duplicated definitions
    auto ret = data.duplicates.empty();

    for (const auto &x : data.duplicates) {
      if (log) {
        const auto &name = x.first;
        auto ptr = x.second;
        auto line = line_info(s, ptr);
        log(line.first, line.second, "'" + name + "' is already defined.");
      }
    }

    // Check if the start rule has ignore operator
    {
      auto &rule = grammar[data.start];
      if (rule.ignoreSemanticValue) {
        if (log) {
          auto line = line_info(s, rule.s_);
          log(line.first, line.second,
              "Ignore operator cannot be applied to '" + rule.name + "'.");
        }
        ret = false;
      }
    }

    if (!ret) { return nullptr; }

    // Check missing definitions
    for (auto &x : grammar) {
      auto &rule = x.second;

      ReferenceChecker vis(grammar, rule.params);
      rule.accept(vis);
      for (const auto &y : vis.error_s) {
        const auto &name = y.first;
        const auto ptr = y.second;
        if (log) {
          auto line = line_info(s, ptr);
          log(line.first, line.second, vis.error_message[name]);
        }
        ret = false;
      }
    }

    if (!ret) { return nullptr; }

    // Link references
    for (auto &x : grammar) {
      auto &rule = x.second;
      LinkReferences vis(grammar, rule.params);
      rule.accept(vis);
    }

    // Check left recursion
    ret = true;

    for (auto &x : grammar) {
      const auto &name = x.first;
      auto &rule = x.second;

      DetectLeftRecursion vis(name);
      rule.accept(vis);
      if (vis.error_s) {
        if (log) {
          auto line = line_info(s, vis.error_s);
          log(line.first, line.second, "'" + name + "' is left recursive.");
        }
        ret = false;
      }
    }

    if (!ret) { return nullptr; }

    // Set root definition
    auto &start_rule = grammar[data.start];

    // Check infinite loop
    for (auto &[name, rule] : grammar) {
      DetectInfiniteLoop vis(rule.s_, name);
      rule.accept(vis);
      if (vis.has_error) {
        if (log) {
          auto line = line_info(s, vis.error_s);
          log(line.first, line.second,
              "infinite loop is detected in '" + vis.error_name + "'.");
        }
        return nullptr;
      }
    }

    // Automatic whitespace skipping
    if (grammar.count(WHITESPACE_DEFINITION_NAME)) {
      for (auto &x : grammar) {
        auto &rule = x.second;
        auto ope = rule.get_core_operator();
        if (IsLiteralToken::check(*ope)) { rule <= tok(ope); }
      }

      start_rule.whitespaceOpe =
          wsp(grammar[WHITESPACE_DEFINITION_NAME].get_core_operator());
    }

    // Word expression
    if (grammar.count(WORD_DEFINITION_NAME)) {
      start_rule.wordOpe = grammar[WORD_DEFINITION_NAME].get_core_operator();
    }

    // Apply instructions
    for (const auto &item : data.instructions) {
      const auto &name = item.first;
      const auto &instruction = item.second;
      auto &rule = grammar[name];

      if (instruction.type == "precedence") {
        const auto &info =
            std::any_cast<PrecedenceClimbing::BinOpeInfo>(instruction.data);

        if (!apply_precedence_instruction(rule, info, s, log)) {
          return nullptr;
        }
      } else if (instruction.type == "message") {
        rule.error_message = std::any_cast<std::string>(instruction.data);
      } else if (instruction.type == "no_ast_opt") {
        rule.no_ast_opt = true;
      }
    }

    // Set root definition
    start = data.start;

    return data.grammar;
  }
