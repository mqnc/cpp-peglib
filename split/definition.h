
/*
 * Definition
 */
class Definition {
public:
  struct Result {
    bool ret;
    bool recovered;
    size_t len;
    ErrorInfo error_info;
  };

  Definition() : holder_(std::make_shared<Holder>(this)) {}

  Definition(const Definition &rhs) : name(rhs.name), holder_(rhs.holder_) {
    holder_->outer_ = this;
  }

  Definition(const std::shared_ptr<Ope> &ope)
      : holder_(std::make_shared<Holder>(this)) {
    *this <= ope;
  }

  operator std::shared_ptr<Ope>() {
    return std::make_shared<WeakHolder>(holder_);
  }

  Definition &operator<=(const std::shared_ptr<Ope> &ope) {
    holder_->ope_ = ope;
    return *this;
  }

  Result parse(const char *s, size_t n, const char *path = nullptr,
               Log log = nullptr) const {
    SemanticValues vs;
    std::any dt;
    return parse_core(s, n, vs, dt, path, log);
  }

  Result parse(const char *s, const char *path = nullptr,
               Log log = nullptr) const {
    auto n = strlen(s);
    return parse(s, n, path, log);
  }

  Result parse(const char *s, size_t n, std::any &dt,
               const char *path = nullptr, Log log = nullptr) const {
    SemanticValues vs;
    return parse_core(s, n, vs, dt, path, log);
  }

  Result parse(const char *s, std::any &dt, const char *path = nullptr,
               Log log = nullptr) const {
    auto n = strlen(s);
    return parse(s, n, dt, path, log);
  }

  template <typename T>
  Result parse_and_get_value(const char *s, size_t n, T &val,
                             const char *path = nullptr,
                             Log log = nullptr) const {
    SemanticValues vs;
    std::any dt;
    auto r = parse_core(s, n, vs, dt, path, log);
    if (r.ret && !vs.empty() && vs.front().has_value()) {
      val = std::any_cast<T>(vs[0]);
    }
    return r;
  }

  template <typename T>
  Result parse_and_get_value(const char *s, T &val, const char *path = nullptr,
                             Log log = nullptr) const {
    auto n = strlen(s);
    return parse_and_get_value(s, n, val, path, log);
  }

  template <typename T>
  Result parse_and_get_value(const char *s, size_t n, std::any &dt, T &val,
                             const char *path = nullptr,
                             Log log = nullptr) const {
    SemanticValues vs;
    auto r = parse_core(s, n, vs, dt, path, log);
    if (r.ret && !vs.empty() && vs.front().has_value()) {
      val = std::any_cast<T>(vs[0]);
    }
    return r;
  }

  template <typename T>
  Result parse_and_get_value(const char *s, std::any &dt, T &val,
                             const char *path = nullptr,
                             Log log = nullptr) const {
    auto n = strlen(s);
    return parse_and_get_value(s, n, dt, val, path, log);
  }

  void operator=(Action a) { action = a; }

  template <typename T> Definition &operator,(T fn) {
    operator=(fn);
    return *this;
  }

  Definition &operator~() {
    ignoreSemanticValue = true;
    return *this;
  }

  void accept(Ope::Visitor &v) { holder_->accept(v); }

  std::shared_ptr<Ope> get_core_operator() const { return holder_->ope_; }

  bool is_token() const {
    std::call_once(is_token_init_, [this]() {
      is_token_ = TokenChecker::is_token(*get_core_operator());
    });
    return is_token_;
  }

  std::string name;
  const char *s_ = nullptr;

  size_t id = 0;
  Action action;
  std::function<void(const char *s, size_t n, std::any &dt)> enter;
  std::function<void(const char *s, size_t n, size_t matchlen, std::any &value,
                     std::any &dt)>
      leave;
  bool ignoreSemanticValue = false;
  std::shared_ptr<Ope> whitespaceOpe;
  std::shared_ptr<Ope> wordOpe;
  bool enablePackratParsing = false;
  bool is_macro = false;
  std::vector<std::string> params;
  TracerEnter tracer_enter;
  TracerLeave tracer_leave;
  bool disable_action = false;

  std::string error_message;
  bool no_ast_opt = false;

private:
  friend class Reference;
  friend class ParserGenerator;

  Definition &operator=(const Definition &rhs);
  Definition &operator=(Definition &&rhs);

  void initialize_definition_ids() const {
    std::call_once(definition_ids_init_, [&]() {
      AssignIDToDefinition vis;
      holder_->accept(vis);
      if (whitespaceOpe) { whitespaceOpe->accept(vis); }
      if (wordOpe) { wordOpe->accept(vis); }
      definition_ids_.swap(vis.ids);
    });
  }

  Result parse_core(const char *s, size_t n, SemanticValues &vs, std::any &dt,
                    const char *path, Log log) const {
    initialize_definition_ids();

    std::shared_ptr<Ope> ope = holder_;
    if (whitespaceOpe) { ope = std::make_shared<Sequence>(whitespaceOpe, ope); }

    Context cxt(path, s, n, definition_ids_.size(), whitespaceOpe, wordOpe,
                enablePackratParsing, tracer_enter, tracer_leave, log);

    auto len = ope->parse(s, n, vs, cxt, dt);
    return Result{success(len), cxt.recovered, len, cxt.error_info};
  }

  std::shared_ptr<Holder> holder_;
  mutable std::once_flag is_token_init_;
  mutable bool is_token_ = false;
  mutable std::once_flag assign_id_to_definition_init_;
  mutable std::once_flag definition_ids_init_;
  mutable std::unordered_map<void *, size_t> definition_ids_;
};
