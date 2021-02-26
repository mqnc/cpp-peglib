/*
 * Context
 */
class Context;
class Ope;
class Definition;

using TracerEnter = std::function<void(const Ope &name, const char *s, size_t n,
                                       const SemanticValues &vs,
                                       const Context &c, const std::any &dt)>;

using TracerLeave = std::function<void(
    const Ope &ope, const char *s, size_t n, const SemanticValues &vs,
    const Context &c, const std::any &dt, size_t)>;

class Context {
public:
  const char *path;
  const char *s;
  const size_t l;
  std::vector<size_t> source_line_index;

  ErrorInfo error_info;
  bool recovered = false;

  std::vector<std::shared_ptr<SemanticValues>> value_stack;
  size_t value_stack_size = 0;

  std::vector<Definition *> rule_stack;
  std::vector<std::vector<std::shared_ptr<Ope>>> args_stack;

  size_t in_token_boundary_count = 0;

  std::shared_ptr<Ope> whitespaceOpe;
  bool in_whitespace = false;

  std::shared_ptr<Ope> wordOpe;

  std::vector<std::map<std::string_view, std::string>> capture_scope_stack;
  size_t capture_scope_stack_size = 0;

  std::vector<bool> cut_stack;

  const size_t def_count;
  const bool enablePackratParsing;
  std::vector<bool> cache_registered;
  std::vector<bool> cache_success;

  std::map<std::pair<size_t, size_t>, std::tuple<size_t, std::any>>
      cache_values;

  TracerEnter tracer_enter;
  TracerLeave tracer_leave;

  Log log;

  Context(const char *path, const char *s, size_t l, size_t def_count,
          std::shared_ptr<Ope> whitespaceOpe, std::shared_ptr<Ope> wordOpe,
          bool enablePackratParsing, TracerEnter tracer_enter,
          TracerLeave tracer_leave, Log log)
      : path(path), s(s), l(l), whitespaceOpe(whitespaceOpe), wordOpe(wordOpe),
        def_count(def_count), enablePackratParsing(enablePackratParsing),
        cache_registered(enablePackratParsing ? def_count * (l + 1) : 0),
        cache_success(enablePackratParsing ? def_count * (l + 1) : 0),
        tracer_enter(tracer_enter), tracer_leave(tracer_leave), log(log) {

    for (size_t pos = 0; pos < l; pos++) {
      if (s[pos] == '\n') { source_line_index.push_back(pos); }
    }
    source_line_index.push_back(l);

    args_stack.resize(1);

    push_capture_scope();
  }

  ~Context() { assert(!value_stack_size); }

  Context(const Context &) = delete;
  Context(Context &&) = delete;
  Context operator=(const Context &) = delete;

  template <typename T>
  void packrat(const char *a_s, size_t def_id, size_t &len, std::any &val,
               T fn) {
    if (!enablePackratParsing) {
      fn(val);
      return;
    }

    auto col = a_s - s;
    auto idx = def_count * static_cast<size_t>(col) + def_id;

    if (cache_registered[idx]) {
      if (cache_success[idx]) {
        auto key = std::pair(col, def_id);
        std::tie(len, val) = cache_values[key];
        return;
      } else {
        len = static_cast<size_t>(-1);
        return;
      }
    } else {
      fn(val);
      cache_registered[idx] = true;
      cache_success[idx] = success(len);
      if (success(len)) {
        auto key = std::pair(col, def_id);
        cache_values[key] = std::pair(len, val);
      }
      return;
    }
  }

  SemanticValues &push() {
    assert(value_stack_size <= value_stack.size());
    if (value_stack_size == value_stack.size()) {
      value_stack.emplace_back(std::make_shared<SemanticValues>());
    } else {
      auto &vs = *value_stack[value_stack_size];
      if (!vs.empty()) {
        vs.clear();
        if (!vs.tags.empty()) { vs.tags.clear(); }
      }
      vs.sv_ = std::string_view();
      vs.choice_count_ = 0;
      vs.choice_ = 0;
      if (!vs.tokens.empty()) { vs.tokens.clear(); }
    }

    auto &vs = *value_stack[value_stack_size++];
    vs.path = path;
    vs.ss = s;
    vs.source_line_index = &source_line_index;
    return vs;
  }

  void pop() { value_stack_size--; }

  void push_args(std::vector<std::shared_ptr<Ope>> &&args) {
    args_stack.emplace_back(args);
  }

  void pop_args() { args_stack.pop_back(); }

  const std::vector<std::shared_ptr<Ope>> &top_args() const {
    return args_stack[args_stack.size() - 1];
  }

  void push_capture_scope() {
    assert(capture_scope_stack_size <= capture_scope_stack.size());
    if (capture_scope_stack_size == capture_scope_stack.size()) {
      capture_scope_stack.emplace_back(
          std::map<std::string_view, std::string>());
    } else {
      auto &cs = capture_scope_stack[capture_scope_stack_size];
      if (!cs.empty()) { cs.clear(); }
    }
    capture_scope_stack_size++;
  }

  void pop_capture_scope() { capture_scope_stack_size--; }

  void shift_capture_values() {
    assert(capture_scope_stack.size() >= 2);
    auto curr = &capture_scope_stack[capture_scope_stack_size - 1];
    auto prev = curr - 1;
    for (const auto &kv : *curr) {
      (*prev)[kv.first] = kv.second;
    }
  }

  void set_error_pos(const char *a_s, const char *literal = nullptr);

  // void trace_enter(const char *name, const char *a_s, size_t n,
  void trace_enter(const Ope &ope, const char *a_s, size_t n,
                   SemanticValues &vs, std::any &dt) const;
  // void trace_leave(const char *name, const char *a_s, size_t n,
  void trace_leave(const Ope &ope, const char *a_s, size_t n,
                   SemanticValues &vs, std::any &dt, size_t len) const;
  bool is_traceable(const Ope &ope) const;

  mutable size_t next_trace_id = 0;
  mutable std::list<size_t> trace_ids;
};
