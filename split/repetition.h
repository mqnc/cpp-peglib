
class Repetition : public Ope {
public:
  Repetition(const std::shared_ptr<Ope> &ope, size_t min, size_t max)
      : ope_(ope), min_(min), max_(max) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &vs, Context &c,
                    std::any &dt) const override {
    size_t count = 0;
    size_t i = 0;
    while (count < min_) {
      c.push_capture_scope();
      auto se = scope_exit([&]() { c.pop_capture_scope(); });
      const auto &rule = *ope_;
      auto len = rule.parse(s + i, n - i, vs, c, dt);
      if (success(len)) {
        c.shift_capture_values();
      } else {
        return len;
      }
      i += len;
      count++;
    }

    while (n - i > 0 && count < max_) {
      c.push_capture_scope();
      auto se = scope_exit([&]() { c.pop_capture_scope(); });
      auto save_sv_size = vs.size();
      auto save_tok_size = vs.tokens.size();
      const auto &rule = *ope_;
      auto len = rule.parse(s + i, n - i, vs, c, dt);
      if (success(len)) {
        c.shift_capture_values();
      } else {
        if (vs.size() != save_sv_size) {
          vs.erase(vs.begin() + static_cast<std::ptrdiff_t>(save_sv_size));
          vs.tags.erase(vs.tags.begin() +
                        static_cast<std::ptrdiff_t>(save_sv_size));
        }
        if (vs.tokens.size() != save_tok_size) {
          vs.tokens.erase(vs.tokens.begin() +
                          static_cast<std::ptrdiff_t>(save_tok_size));
        }
        break;
      }
      i += len;
      count++;
    }
    return i;
  }

  void accept(Visitor &v) override;

  bool is_zom() const {
    return min_ == 0 && max_ == std::numeric_limits<size_t>::max();
  }

  static std::shared_ptr<Repetition> zom(const std::shared_ptr<Ope> &ope) {
    return std::make_shared<Repetition>(ope, 0,
                                        std::numeric_limits<size_t>::max());
  }

  static std::shared_ptr<Repetition> oom(const std::shared_ptr<Ope> &ope) {
    return std::make_shared<Repetition>(ope, 1,
                                        std::numeric_limits<size_t>::max());
  }

  static std::shared_ptr<Repetition> opt(const std::shared_ptr<Ope> &ope) {
    return std::make_shared<Repetition>(ope, 0, 1);
  }

  std::shared_ptr<Ope> ope_;
  size_t min_;
  size_t max_;
};
