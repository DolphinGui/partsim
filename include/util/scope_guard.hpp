#pragma once

#include <utility>

template <typename Function> struct ScopeGuard {
  Function f;
  ScopeGuard(Function&& f): f(std::move(f)){}
  ~ScopeGuard() { f(); }
};