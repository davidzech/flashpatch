#pragma once

template <class T> class Maybe {
  private:
    T t;
    bool hasValue;

  public:
    Maybe(decltype(nullptr)) : hasValue(false) {}
    explicit Maybe(T &&v) : t(v), hasValue(true) {}
    // Maybe(T &v) : t(v), hasValue{true} {}
    Maybe(T v) : t(v), hasValue(true) {}

    bool HasValue() const { return hasValue; }

    const T &get() const & { return t; }
    const T &&get() const && { return t; }

    T *operator->() { return &t; }
    const T *operator->() const { return &t; }

    T &operator*() & { return t; }
    const T &operator*() const & { return t; }

    T &&operator*() && { return t; }
    const T &&operator*() const && { return t; }

    operator bool() const { return hasValue; }
};

template <int N, class T> __attribute__((always_inline)) constexpr T Align(T x) { return x & -N; }