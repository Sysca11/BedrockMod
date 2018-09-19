#include <StaticHook.h>
#include <api.h>
#include <base.h>

MAKE_FOREIGN_TYPE(mce::UUID, "uuid", { "t0", "t1" });
MAKE_FOREIGN_TYPE(Actor *, "actor");
MAKE_FOREIGN_TYPE(ServerPlayer *, "player");

#include "main.h"

#ifndef DIAG
static_assert(sizeof(void *) == 8, "Only works in 64bit");
#endif

SCM_DEFINE_PUBLIC(c_make_uuid, "uuid", 1, 0, 0, (scm::val<std::string> name), "Create UUID") {
  std::string temp = name;
  return scm::to_scm(mce::UUID::fromStringFix(temp));
}

SCM_DEFINE_PUBLIC(c_uuid_to_string, "uuid->string", 1, 0, 0, (scm::val<mce::UUID> uuid), "UUID to string") {
  return scm::to_scm(uuid.get().asString());
}

SCM_DEFINE_PUBLIC(c_actor_name, "actor-name", 1, 0, 0, (scm::val<Actor *> act), "Return Actor's name") { return scm::to_scm(act->getNameTag()); }

SCM_DEFINE_PUBLIC(c_for_each_player, "for-each-player", 1, 0, 0, (scm::callback<bool, ServerPlayer *> callback), "Invoke function for each player") {
  support_get_minecraft()->getLevel().forEachPlayer([=](Player &p) { return callback((ServerPlayer *)&p); });
  return SCM_UNSPECIFIED;
}

SCM_DEFINE_PUBLIC(c_player_kick, "player-kick", 1, 0, 0, (scm::val<ServerPlayer *> player), "Kick player from server") {
  kickPlayer(player);
  return SCM_UNSPECIFIED;
}

SCM_DEFINE_PUBLIC(c_player_permission_level, "player-permission-level", 1, 0, 0, (scm::val<ServerPlayer *> player),
                  "Get player's permission level.") {
  return scm::to_scm(player->getCommandPermissionLevel());
}

SCM_DEFINE_PUBLIC(c_player_uuid, "player-uuid", 1, 0, 0, (scm::val<ServerPlayer *> player), "Get Player's UUID") {
  return scm::to_scm(player->getUUID());
}

SCM_DEFINE_PUBLIC(c_player_xuid, "player-xuid", 1, 0, 0, (scm::val<ServerPlayer *> player), "Get Player's XUID") {
  return scm::to_scm(player->getXUID());
}

SCM_DEFINE_PUBLIC(c_player_stats, "player-stats", 1, 0, 0, (scm::val<ServerPlayer *> player), "Get Player's stats info") {
  auto status = ServerCommand::mGame->getNetworkHandler().getPeerForUser(player->getClientId()).getNetworkStatus();
  return scm::list(status.ping, status.avgping, status.packetloss, status.avgpacketloss);
}

MAKE_HOOK(player_login, "player-login", mce::UUID);
MAKE_FLUID(bool, login_result, "login-result");

struct Whitelist {};

TInstanceHook(bool, _ZNK9Whitelist9isAllowedERKN3mce4UUIDERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE, Whitelist, mce::UUID &uuid,
              std::string const &msg) {
  return login_result()[true] <<= [=] { player_login(uuid); };
}

LOADFILE(preload, "src/script/base/preload.scm");

PRELOAD_MODULE("minecraft base") {
  onPlayerJoined <<= scm::make_hook<ServerPlayer &>("player-joined");
  onPlayerLeft <<= scm::make_hook<ServerPlayer &>("player-left");

#ifndef DIAG
#include "main.x"
#include "preload.scm.z"
#endif

  scm_c_eval_string(&file_preload_start);
}

extern "C" void mod_set_server(void *v) { support_get_minecraft()->activateWhitelist(); }
