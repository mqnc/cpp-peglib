
class PrioritizedChoice : public Ope {
public:
  template <typename... Args>
  PrioritizedChoice(bool for_label, const Args &... args)
      : opes_{static_cast<std::shared_ptr<Ope>>(args)...},
        for_label_(for_label) {}
  PrioritizedChoice(const std::vector<std::shared_ptr<Ope>> &opes)
      : opes_(opes) {}
  PrioritizedChoice(std::vector<std::shared_ptr<Ope>> &&opes) : opes_(opes) {}

  size_t parse_core(const char *s, size_t n, SemanticValues &vs, Context &c,
                    std::any &dt) const override {
    size_t len = static_cast<size_t>(-1);

    if (!for_label_) { c.cut_stack.push_back(false); }

    size_t id = 0;
    for (const auto &ope : opes_) {
      if (!c.cut_stack.empty()) { c.cut_stack.back() = false; }

      auto &chldsv = c.push();
      c.push_capture_scope();

      auto se = scope_exit([&]() {
        c.pop();
        c.pop_capture_scope();
      });

      len = ope->parse(s, n, chldsv, c, dt);

      if (success(len)) {
        if (!chldsv.empty()) {
          for (size_t i = 0; i < chldsv.size(); i++) {
            vs.emplace_back(std::move(chldsv[i]));
          }
        }
        if (!chldsv.tags.empty()) {
          for (size_t i = 0; i < chldsv.tags.size(); i++) {
            vs.tags.emplace_back(std::move(chldsv.tags[i]));
          }
        }
        vs.sv_ = chldsv.sv_;
        vs.choice_count_ = opes_.size();
        vs.choice_ = id;
        if (!chldsv.tokens.empty()) {
          for (size_t i = 0; i < chldsv.tokens.size(); i++) {
            vs.tokens.emplace_back(std::move(chldsv.tokens[i]));
          }
        }
        c.shift_capture_values();
        break;
      } else if (!c.cut_stack.empty() && c.cut_stack.back()) {
        break;
      }

      id++;
    }

    if (!for_label_) { c.cut_stack.pop_back(); }

    return len;
  }

  void accept(Visitor &v) override;

  size_t size() const { return opes_.size(); }

  std::vector<std::shared_ptr<Ope>> opes_;
  bool for_label_ = false;
};
