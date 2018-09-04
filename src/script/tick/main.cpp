// Deps: out/script_chat.so: out/script_base.so
#include <api.h>

#include <StaticHook.h>

#include <list>
#include <memory>

struct FixedFunction {
  int16_t chip;
  std::function<void(void)> fun;
  FixedFunction(int16_t chip, std::function<void(void)> fun)
      : chip(chip)
      , fun(fun) {}
  operator int16_t() { return chip; }
  void operator()() { fun(); }
};

std::unordered_multimap<int16_t, FixedFunction> tickHandlers;
std::list<FixedFunction> timeoutHandlers;
int16_t count = 0;

TInstanceHook(void, _ZN5Level4tickEv, Level) {
  for (auto it : tickHandlers)
    if (count % it.first == it.second) it.second();
  if (!timeoutHandlers.empty()) {
    auto &to = timeoutHandlers.front();
    if (to.chip-- <= 0) {
      to();
      timeoutHandlers.pop_front();
    }
    while (!timeoutHandlers.empty() && timeoutHandlers.front().chip <= 0) {
      timeoutHandlers.front()();
      timeoutHandlers.pop_front();
    }
  }
  count++;
  original(this);
}

SCM_DEFINE(c_set_interval, "interval-run", 2, 0, 0, (scm::val<uint> cycle, scm::callback<> fn), "setInterval") {
  auto it = tickHandlers.emplace(std::make_pair(cycle, FixedFunction{ (int16_t)(count % cycle), fn }));
  return SCM_UNSPECIFIED;
}

SCM_DEFINE(c_set_timeout, "delay-run", 2, 0, 0, (scm::val<uint> len, scm::callback<> fn), "setTimeout") {
  int16_t sum = 0;
  for (auto it = timeoutHandlers.begin(); it != timeoutHandlers.end(); ++it) {
    sum += it->chip;
    if (sum > len) {
      timeoutHandlers.insert(it, FixedFunction(it->chip - sum + len, fn));
      it->chip -= sum - len;
      return SCM_UNSPECIFIED;
    }
  }
  timeoutHandlers.push_back(FixedFunction(len - sum, fn));
  return SCM_UNSPECIFIED;
}

static void init_guile() {
#ifndef DIAG
#include "main.x"
#endif
}

extern "C" void mod_init() {
  script_preload(init_guile);
  // chaiscript::ModulePtr m(new chaiscript::Module());
  // m->add(chaiscript::fun([](std::function<void(void)> fn, int16_t cycle) -> std::function<void(void)> {
  //          auto it = tickHandlers.emplace(std::make_pair(cycle, FixedFunction{ (int16_t)(count % cycle), fn }));
  //          return [it]() { tickHandlers.erase(it); };
  //        }),
  //        "setInterval");
  // m->add(chaiscript::fun([](std::function<void(void)> fn, int16_t len) {
  //          int16_t sum = 0;
  //          for (auto it = timeoutHandlers.begin(); it != timeoutHandlers.end(); ++it) {
  //            sum += it->chip;
  //            if (sum > len) {
  //              timeoutHandlers.insert(it, FixedFunction(it->chip - sum + len, fn));
  //              it->chip -= sum - len;
  //              return;
  //            }
  //          }
  //          timeoutHandlers.push_back(FixedFunction(len - sum, fn));
  //        }),
  //        "setTimeout");
  // loadModule(m);
}