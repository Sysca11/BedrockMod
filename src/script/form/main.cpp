// Deps: out/script_form.so: out/script_base.so
#include "../base/main.h"

#include <StaticHook.h>
#include <api.h>

struct ModalFormRequestPacket : Packet {
  int id;
  std::string data;
  ModalFormRequestPacket(unsigned char playerSubIndex, int id, std::string data)
      : Packet(playerSubIndex)
      , id(id)
      , data(data) {}
  ~ModalFormRequestPacket() {}

  virtual void *getId() const;
  virtual void *getName() const;
  virtual void *write(BinaryStream &) const;
  virtual void *read(BinaryStream &);
  virtual void *handle(NetworkIdentifier const &, NetEventCallback &) const;
  virtual bool disallowBatching() const;
};

struct ModalFormResponsePacket : Packet {
  int id;
  std::string data;
  ModalFormResponsePacket(unsigned char playerSubIndex, int id, std::string data)
      : Packet(playerSubIndex)
      , id(id)
      , data(data) {}

  virtual void *getId() const;
  virtual void *getName() const;
  virtual void *write(BinaryStream &) const;
  virtual void *read(BinaryStream &);
  virtual void *handle(NetworkIdentifier const &, NetEventCallback &) const;
  virtual bool disallowBatching() const;
};

struct FixedFunction {
  int32_t rid;
  scm::callback<void, std::string> fun;
  FixedFunction(int32_t rid, scm::callback<void, std::string> fun)
      : rid(rid)
      , fun(fun) {
    scm_gc_protect_object(fun);
  }
  FixedFunction(FixedFunction &&rhs)
      : rid(rhs.rid)
      , fun(rhs.fun) {
    rhs.fun.setInvalid();
  }
  operator int32_t() { return rid; }
  void operator()(std::string val) { fun(val); }
  ~FixedFunction() {
    if (scm_is_true(fun)) scm_gc_unprotect_object(fun);
  }
};

std::unordered_map<NetworkIdentifier, FixedFunction> callbacks;

TInstanceHook(void, _ZN20ServerNetworkHandler6handleERK17NetworkIdentifierRK23ModalFormResponsePacket, ServerNetworkHandler,
              NetworkIdentifier const &nid, ModalFormResponsePacket &packet) {
  auto it = callbacks.find(nid);
  if (it != callbacks.end()) {
    if (it->second.rid == packet.id) {
      it->second(packet.data);
      callbacks.erase(it);
    }
  }
}

SCM_DEFINE_PUBLIC(c_send_form, "send-form", 3, 0, 0,
                  (scm::val<ServerPlayer *> player, scm::val<std::string> request, scm::callback<void, std::string> callback),
                  "Send form to player") {
  int id = rand();
  ModalFormRequestPacket packet{ player->getClientSubId(), id, request.get() };
  callbacks.emplace(player->getClientId(), FixedFunction{ id, callback });
  player->sendNetworkPacket(packet);
  return SCM_UNSPECIFIED;
}

PRELOAD_MODULE("minecraft form") {
#ifndef DIAG
#include "main.x"
#endif
}