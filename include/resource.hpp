#pragma once
template <typename T, typename Destructor> struct resource {
  T handle;
  Destructor d;
  ~resource() { d(handle); }
};