
struct IsReference : public Ope::Visitor {
  void visit(Reference &) override { is_reference_ = true; }

  static bool check(Ope &ope) {
    IsReference vis;
    ope.accept(vis);
    return vis.is_reference_;
  }

private:
  bool is_reference_ = false;
};

struct TraceOpeName : public Ope::Visitor {
  void visit(Sequence &) override { name_ = "Sequence"; }
  void visit(PrioritizedChoice &) override { name_ = "PrioritizedChoice"; }
  void visit(Repetition &) override { name_ = "Repetition"; }
  void visit(AndPredicate &) override { name_ = "AndPredicate"; }
  void visit(NotPredicate &) override { name_ = "NotPredicate"; }
  void visit(Dictionary &) override { name_ = "Dictionary"; }
  void visit(LiteralString &) override { name_ = "LiteralString"; }
  void visit(CharacterClass &) override { name_ = "CharacterClass"; }
  void visit(Character &) override { name_ = "Character"; }
  void visit(AnyCharacter &) override { name_ = "AnyCharacter"; }
  void visit(CaptureScope &) override { name_ = "CaptureScope"; }
  void visit(Capture &) override { name_ = "Capture"; }
  void visit(TokenBoundary &) override { name_ = "TokenBoundary"; }
  void visit(Ignore &) override { name_ = "Ignore"; }
  void visit(User &) override { name_ = "User"; }
  void visit(WeakHolder &) override { name_ = "WeakHolder"; }
  void visit(Holder &ope) override { name_ = ope.trace_name(); }
  void visit(Reference &) override { name_ = "Reference"; }
  void visit(Whitespace &) override { name_ = "Whitespace"; }
  void visit(BackReference &) override { name_ = "BackReference"; }
  void visit(PrecedenceClimbing &) override { name_ = "PrecedenceClimbing"; }
  void visit(Recovery &) override { name_ = "Recovery"; }
  void visit(Cut &) override { name_ = "Cut"; }

  static std::string get(Ope &ope) {
    TraceOpeName vis;
    ope.accept(vis);
    return vis.name_;
  }

private:
  const char *name_ = nullptr;
};

struct AssignIDToDefinition : public Ope::Visitor {
  void visit(Sequence &ope) override {
    for (auto op : ope.opes_) {
      op->accept(*this);
    }
  }
  void visit(PrioritizedChoice &ope) override {
    for (auto op : ope.opes_) {
      op->accept(*this);
    }
  }
  void visit(Repetition &ope) override { ope.ope_->accept(*this); }
  void visit(AndPredicate &ope) override { ope.ope_->accept(*this); }
  void visit(NotPredicate &ope) override { ope.ope_->accept(*this); }
  void visit(CaptureScope &ope) override { ope.ope_->accept(*this); }
  void visit(Capture &ope) override { ope.ope_->accept(*this); }
  void visit(TokenBoundary &ope) override { ope.ope_->accept(*this); }
  void visit(Ignore &ope) override { ope.ope_->accept(*this); }
  void visit(WeakHolder &ope) override { ope.weak_.lock()->accept(*this); }
  void visit(Holder &ope) override;
  void visit(Reference &ope) override;
  void visit(Whitespace &ope) override { ope.ope_->accept(*this); }
  void visit(PrecedenceClimbing &ope) override;
  void visit(Recovery &ope) override { ope.ope_->accept(*this); }

  std::unordered_map<void *, size_t> ids;
};