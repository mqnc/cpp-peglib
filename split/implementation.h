
/*
 * Implementations
 */

inline size_t parse_literal(const char *s, size_t n, SemanticValues &vs,
                            Context &c, std::any &dt, const std::string &lit,
                            std::once_flag &init_is_word, bool &is_word,
                            bool ignore_case) {
  size_t i = 0;
  for (; i < lit.size(); i++) {
    if (i >= n || (ignore_case ? (std::tolower(s[i]) != std::tolower(lit[i]))
                               : (s[i] != lit[i]))) {
      c.set_error_pos(s, lit.c_str());
      return static_cast<size_t>(-1);
    }
  }

  // Word check
  SemanticValues dummy_vs;
  Context dummy_c(nullptr, c.s, c.l, 0, nullptr, nullptr, false, nullptr,
                  nullptr, nullptr);
  std::any dummy_dt;

  std::call_once(init_is_word, [&]() {
    if (c.wordOpe) {
      auto len =
          c.wordOpe->parse(lit.data(), lit.size(), dummy_vs, dummy_c, dummy_dt);
      is_word = success(len);
    }
  });

  if (is_word) {
    NotPredicate ope(c.wordOpe);
    auto len = ope.parse(s + i, n - i, dummy_vs, dummy_c, dummy_dt);
    if (fail(len)) { return len; }
    i += len;
  }

  // Skip whiltespace
  if (!c.in_token_boundary_count) {
    if (c.whitespaceOpe) {
      auto len = c.whitespaceOpe->parse(s + i, n - i, vs, c, dt);
      if (fail(len)) { return len; }
      i += len;
    }
  }

  return i;
}

inline void Context::set_error_pos(const char *a_s, const char *literal) {
  if (log) {
    if (error_info.error_pos <= a_s) {
      if (error_info.error_pos < a_s) {
        error_info.error_pos = a_s;
        error_info.expected_tokens.clear();
      }
      if (literal) {
        error_info.add(literal, true);
      } else if (!rule_stack.empty()) {
        auto rule = rule_stack.back();
        auto ope = rule->get_core_operator();
        if (auto token = FindLiteralToken::token(*ope);
            token && token[0] != '\0') {
          error_info.add(token, true);
        } else {
          error_info.add(rule->name.c_str(), false);
        }
      }
    }
  }
}

inline void Context::trace_enter(const Ope &ope, const char *a_s, size_t n,
                                 SemanticValues &vs, std::any &dt) const {
  trace_ids.push_back(next_trace_id++);
  tracer_enter(ope, a_s, n, vs, *this, dt);
}

inline void Context::trace_leave(const Ope &ope, const char *a_s, size_t n,
                                 SemanticValues &vs, std::any &dt,
                                 size_t len) const {
  tracer_leave(ope, a_s, n, vs, *this, dt, len);
  trace_ids.pop_back();
}

inline bool Context::is_traceable(const Ope &ope) const {
  if (tracer_enter && tracer_leave) {
    return !IsReference::check(const_cast<Ope &>(ope));
  }
  return false;
}

inline size_t Ope::parse(const char *s, size_t n, SemanticValues &vs,
                         Context &c, std::any &dt) const {
  if (c.is_traceable(*this)) {
    c.trace_enter(*this, s, n, vs, dt);
    auto len = parse_core(s, n, vs, c, dt);
    c.trace_leave(*this, s, n, vs, dt, len);
    return len;
  }
  return parse_core(s, n, vs, c, dt);
}

inline size_t Dictionary::parse_core(const char *s, size_t n,
                                     SemanticValues & /*vs*/, Context &c,
                                     std::any & /*dt*/) const {
  auto len = trie_.match(s, n);
  if (len > 0) { return len; }
  c.set_error_pos(s);
  return static_cast<size_t>(-1);
}

inline size_t LiteralString::parse_core(const char *s, size_t n,
                                        SemanticValues &vs, Context &c,
                                        std::any &dt) const {
  return parse_literal(s, n, vs, c, dt, lit_, init_is_word_, is_word_,
                       ignore_case_);
}

inline size_t TokenBoundary::parse_core(const char *s, size_t n,
                                        SemanticValues &vs, Context &c,
                                        std::any &dt) const {
  size_t len;
  {
    c.in_token_boundary_count++;
    auto se = scope_exit([&]() { c.in_token_boundary_count--; });
    len = ope_->parse(s, n, vs, c, dt);
  }

  if (success(len)) {
    vs.tokens.emplace_back(std::string_view(s, len));

    if (!c.in_token_boundary_count) {
      if (c.whitespaceOpe) {
        auto l = c.whitespaceOpe->parse(s + len, n - len, vs, c, dt);
        if (fail(l)) { return l; }
        len += l;
      }
    }
  }
  return len;
}

inline size_t Holder::parse_core(const char *s, size_t n, SemanticValues &vs,
                                 Context &c, std::any &dt) const {
  if (!ope_) {
    throw std::logic_error("Uninitialized definition ope was used...");
  }

  // Macro reference
  if (outer_->is_macro) {
    c.rule_stack.push_back(outer_);
    auto len = ope_->parse(s, n, vs, c, dt);
    c.rule_stack.pop_back();
    return len;
  }

  size_t len;
  std::any val;

  c.packrat(s, outer_->id, len, val, [&](std::any &a_val) {
    if (outer_->enter) { outer_->enter(s, n, dt); }

    auto se2 = scope_exit([&]() {
      c.pop();
      if (outer_->leave) { outer_->leave(s, n, len, a_val, dt); }
    });

    auto &chldsv = c.push();

    c.rule_stack.push_back(outer_);
    len = ope_->parse(s, n, chldsv, c, dt);
    c.rule_stack.pop_back();

    // Invoke action
    if (success(len)) {
      chldsv.sv_ = std::string_view(s, len);
      chldsv.name_ = outer_->name;

      if (!IsPrioritizedChoice::check(*ope_)) {
        chldsv.choice_count_ = 0;
        chldsv.choice_ = 0;
      }

      try {
        a_val = reduce(chldsv, dt);
      } catch (const parse_error &e) {
        if (c.log) {
          if (e.what()) {
            if (c.error_info.message_pos < s) {
              c.error_info.message_pos = s;
              c.error_info.message = e.what();
            }
          }
        }
        len = static_cast<size_t>(-1);
      }
    }
  });

  if (success(len)) {
    if (!outer_->ignoreSemanticValue) {
      vs.emplace_back(std::move(val));
      vs.tags.emplace_back(str2tag(outer_->name));
    }
  }

  return len;
}

inline std::any Holder::reduce(SemanticValues &vs, std::any &dt) const {
  if (outer_->action && !outer_->disable_action) {
    return outer_->action(vs, dt);
  } else if (vs.empty()) {
    return std::any();
  } else {
    return std::move(vs.front());
  }
}

inline const char *Holder::trace_name() const {
  if (trace_name_.empty()) { trace_name_ = "[" + outer_->name + "]"; }
  return trace_name_.data();
}

inline size_t Reference::parse_core(const char *s, size_t n, SemanticValues &vs,
                                    Context &c, std::any &dt) const {
  if (rule_) {
    // Reference rule
    if (rule_->is_macro) {
      // Macro
      FindReference vis(c.top_args(), c.rule_stack.back()->params);

      // Collect arguments
      std::vector<std::shared_ptr<Ope>> args;
      for (auto arg : args_) {
        arg->accept(vis);
        args.emplace_back(std::move(vis.found_ope));
      }

      c.push_args(std::move(args));
      auto se = scope_exit([&]() { c.pop_args(); });
      auto ope = get_core_operator();
      return ope->parse(s, n, vs, c, dt);
    } else {
      // Definition
      c.push_args(std::vector<std::shared_ptr<Ope>>());
      auto se = scope_exit([&]() { c.pop_args(); });
      auto ope = get_core_operator();
      return ope->parse(s, n, vs, c, dt);
    }
  } else {
    // Reference parameter in macro
    const auto &args = c.top_args();
    return args[iarg_]->parse(s, n, vs, c, dt);
  }
}

inline std::shared_ptr<Ope> Reference::get_core_operator() const {
  return rule_->holder_;
}

inline size_t BackReference::parse_core(const char *s, size_t n,
                                        SemanticValues &vs, Context &c,
                                        std::any &dt) const {
  auto size = static_cast<int>(c.capture_scope_stack_size);
  for (auto i = size - 1; i >= 0; i--) {
    auto index = static_cast<size_t>(i);
    const auto &cs = c.capture_scope_stack[index];
    if (cs.find(name_) != cs.end()) {
      const auto &lit = cs.at(name_);
      std::once_flag init_is_word;
      auto is_word = false;
      return parse_literal(s, n, vs, c, dt, lit, init_is_word, is_word, false);
    }
  }
  throw std::runtime_error("Invalid back reference...");
}

inline Definition &
PrecedenceClimbing::get_reference_for_binop(Context &c) const {
  if (rule_.is_macro) {
    // Reference parameter in macro
    const auto &args = c.top_args();
    auto iarg = dynamic_cast<Reference &>(*binop_).iarg_;
    auto arg = args[iarg];
    return *dynamic_cast<Reference &>(*arg).rule_;
  }

  return *dynamic_cast<Reference &>(*binop_).rule_;
}

inline size_t PrecedenceClimbing::parse_expression(const char *s, size_t n,
                                                   SemanticValues &vs,
                                                   Context &c, std::any &dt,
                                                   size_t min_prec) const {
  auto len = atom_->parse(s, n, vs, c, dt);
  if (fail(len)) { return len; }

  std::string tok;
  auto &rule = get_reference_for_binop(c);
  auto action = std::move(rule.action);

  rule.action = [&](SemanticValues &vs2, std::any &dt2) {
    tok = vs2.token();
    if (action) {
      return action(vs2, dt2);
    } else if (!vs2.empty()) {
      return vs2[0];
    }
    return std::any();
  };
  auto action_se = scope_exit([&]() { rule.action = std::move(action); });

  auto i = len;
  while (i < n) {
    std::vector<std::any> save_values(vs.begin(), vs.end());
    auto save_tokens = vs.tokens;

    auto chv = c.push();
    auto chl = binop_->parse(s + i, n - i, chv, c, dt);
    c.pop();

    if (fail(chl)) { break; }

    auto it = info_.find(tok);
    if (it == info_.end()) { break; }

    auto level = std::get<0>(it->second);
    auto assoc = std::get<1>(it->second);

    if (level < min_prec) { break; }

    vs.emplace_back(std::move(chv[0]));
    i += chl;

    auto next_min_prec = level;
    if (assoc == 'L') { next_min_prec = level + 1; }

    chv = c.push();
    chl = parse_expression(s + i, n - i, chv, c, dt, next_min_prec);
    c.pop();

    if (fail(chl)) {
      vs.assign(save_values.begin(), save_values.end());
      vs.tokens = save_tokens;
      break;
    }

    vs.emplace_back(std::move(chv[0]));
    i += chl;

    std::any val;
    if (rule_.action) {
      vs.sv_ = std::string_view(s, i);
      val = rule_.action(vs, dt);
    } else if (!vs.empty()) {
      val = vs[0];
    }
    vs.clear();
    vs.emplace_back(std::move(val));
  }

  return i;
}

inline size_t Recovery::parse_core(const char *s, size_t n,
                                   SemanticValues & /*vs*/, Context &c,
                                   std::any & /*dt*/) const {
  const auto &rule = dynamic_cast<Reference &>(*ope_);

  // Custom error message
  if (c.log) {
    auto label = dynamic_cast<Reference *>(rule.args_[0].get());
    if (label) {
      if (!label->rule_->error_message.empty()) {
        c.error_info.message_pos = s;
        c.error_info.message = label->rule_->error_message;
      }
    }
  }

  // Recovery
  size_t len = static_cast<size_t>(-1);
  {
    auto save_log = c.log;
    c.log = nullptr;
    auto se = scope_exit([&]() { c.log = save_log; });

    SemanticValues dummy_vs;
    std::any dummy_dt;

    len = rule.parse(s, n, dummy_vs, c, dummy_dt);
  }

  if (success(len)) {
    c.recovered = true;

    if (c.log) {
      c.error_info.output_log(c.log, c.s, c.l);
      c.error_info.clear();
    }
  }

  // Cut
  if (!c.cut_stack.empty()) {
    c.cut_stack.back() = true;

    if (c.cut_stack.size() == 1) {
      // TODO: Remove unneeded entries in packrat memoise table
    }
  }

  return len;
}
