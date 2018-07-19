#include <api.h>

#include <StaticHook.h>
#include <log.h>

#include <functional>
#include <list>
#include <memory>

#include "base.h"

struct FixedFunction {
  uint16_t chip;
  std::function<void(void)> fun;
  FixedFunction(uint16_t chip, std::function<void(void)> fun)
      : chip(chip)
      , fun(fun) {}
  operator uint16_t() { return chip; }
  void operator()() { fun(); }
};

std::unordered_multimap<uint16_t, FixedFunction> tickHandlers;
std::list<FixedFunction> timeoutHandlers;
uint16_t count = 0;

TInstanceHook(void, _ZN5Level4tickEv, Level) {
  for (auto it : tickHandlers) {
    if (count % it.first == it.second) try {
        it.second();
      } catch (std::exception const &e) { Log::error("BASE", "TICK ERROR: %s", e.what()); }
  }
  auto &to = timeoutHandlers.front();
  if (--to.chip <= 0) {
    to();
    timeoutHandlers.pop_front();
  }
  while (timeoutHandlers.front().chip == 0) {
    timeoutHandlers.front()();
    timeoutHandlers.pop_front();
  }
  count++;
  original(this);
}

extern "C" void mod_init() {
  chaiscript::ModulePtr m(new chaiscript::Module());
  m->add(chaiscript::fun([](std::function<void(void)> fn, uint16_t cycle) {
           tickHandlers.emplace(std::make_pair(cycle, FixedFunction{ (uint16_t)(count % cycle), fn }));
         }),
         "setInterval");
  m->add(chaiscript::fun([](std::function<void(void)> fn, uint16_t len) {
           uint16_t sum = 0;
           Log::trace("setTO", "reg: %d", len);
           for (auto it = timeoutHandlers.begin(); it != timeoutHandlers.end(); ++it) {
             sum += it->chip;
             Log::trace("setTO", "sum: %d", sum);
             if (sum > len) {
               Log::trace("setTO", "INS: %d", it->chip - sum + len);
               timeoutHandlers.insert(it, FixedFunction(it->chip - sum + len, fn));
               it->chip -= sum - len;
               return;
             }
           }
           Log::trace("setTO", "BAK: %d", len - sum);
           timeoutHandlers.push_back(FixedFunction(len - sum, fn));
         }),
         "setTimeout");
  loadModule(m);
}