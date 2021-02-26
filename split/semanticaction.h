/*
 * Semantic action
 */
template <typename F, typename... Args> std::any call(F fn, Args &&... args) {
  using R = decltype(fn(std::forward<Args>(args)...));
  if constexpr (std::is_void<R>::value) {
    fn(std::forward<Args>(args)...);
    return std::any();
  } else if constexpr (std::is_same<typename std::remove_cv<R>::type,
                                    std::any>::value) {
    return fn(std::forward<Args>(args)...);
  } else {
    return std::any(fn(std::forward<Args>(args)...));
  }
}

template <typename T>
struct argument_count : argument_count<decltype(&T::operator())> {};
template <typename R, typename... Args>
struct argument_count<R (*)(Args...)>
    : std::integral_constant<unsigned, sizeof...(Args)> {};
template <typename R, typename C, typename... Args>
struct argument_count<R (C::*)(Args...)>
    : std::integral_constant<unsigned, sizeof...(Args)> {};
template <typename R, typename C, typename... Args>
struct argument_count<R (C::*)(Args...) const>
    : std::integral_constant<unsigned, sizeof...(Args)> {};

class Action {
public:
  Action() = default;
  Action(Action &&rhs) = default;
  template <typename F> Action(F fn) : fn_(make_adaptor(fn)) {}
  template <typename F> void operator=(F fn) { fn_ = make_adaptor(fn); }
  Action &operator=(const Action &rhs) = default;

  operator bool() const { return bool(fn_); }

  std::any operator()(SemanticValues &vs, std::any &dt) const {
    return fn_(vs, dt);
  }

private:
  using Fty = std::function<std::any(SemanticValues &vs, std::any &dt)>;

  template <typename F> Fty make_adaptor(F fn) {
    if constexpr (argument_count<F>::value == 1) {
      return [fn](auto &vs, auto & /*dt*/) { return call(fn, vs); };
    } else {
      return [fn](auto &vs, auto &dt) { return call(fn, vs, dt); };
    }
  }

  Fty fn_;
};
