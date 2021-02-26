
class Sequence : public Ope {
public:
  template <typename... Args>
  Sequence(const Args &... args)
      : opes_{static_cast<std::shared_ptr<Ope>>(args)...} {}
  Sequence(const std::vector<std::shared_ptr<Ope>> &opes) : opes_(opes) {}
  Sequence(std::vector<std::shared_ptr<Ope>> &&opes) : opes_(opes) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &vs, Context &c,
                    std::any &dt) const override {
    auto &chldsv = c.push();
    auto pop_se = scope_exit([&]() { c.pop(); });
    size_t i = 0;
    for (const auto &ope : opes_) {
      const auto &rule = *ope;
      auto len = rule.parse(s + i, n - i, chldsv, c, dt);
      if (fail(len)) { return len; }
      i += len;
    }
    if (!chldsv.empty()) {
      for (size_t j = 0; j < chldsv.size(); j++) {
        vs.emplace_back(std::move(chldsv[j]));
      }
    }
    if (!chldsv.tags.empty()) {
      for (size_t j = 0; j < chldsv.tags.size(); j++) {
        vs.tags.emplace_back(std::move(chldsv.tags[j]));
      }
    }
    vs.sv_ = chldsv.sv_;
    if (!chldsv.tokens.empty()) {
      for (size_t j = 0; j < chldsv.tokens.size(); j++) {
        vs.tokens.emplace_back(std::move(chldsv.tokens[j]));
      }
    }
    return i;
  }

  void accept(Visitor &v) override;

  std::vector<std::shared_ptr<Ope>> opes_;
};
