public:
  static std::shared_ptr<Grammar> parse(const char *s, size_t n,
                                        const Rules &rules, std::string &start,
                                        Log log) {
    return get_instance().perform_core(s, n, rules, start, log);
  }

  static std::shared_ptr<Grammar> parse(const char *s, size_t n,
                                        std::string &start, Log log) {
    Rules dummy;
    return parse(s, n, dummy, start, log);
  }

  // For debuging purpose
  static Grammar &grammar() { return get_instance().g; }

private:
  static ParserGenerator &get_instance() {
    static ParserGenerator instance;
    return instance;
  }

  ParserGenerator() {
    make_grammar();
    setup_actions();
  }

  struct Instruction {
    std::string type;
    std::any data;
  };

  struct Data {
    std::shared_ptr<Grammar> grammar;
    std::string start;
    const char *start_pos = nullptr;
    std::vector<std::pair<std::string, const char *>> duplicates;
    std::map<std::string, Instruction> instructions;

    Data() : grammar(std::make_shared<Grammar>()) {}
  };
