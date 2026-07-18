#pragma once
#include "utils/ArdaUtils.h"
#include "utils/Archive.h"
#include <string>
#include <vector>
namespace Arda {
enum class Gender { Male, Female };
enum class Type {
  Leader,
  ArmyChief,
  NavyChief,
  AirForceChief,
  HighCommand,
  ArmyGeneral,
  FleetAdmiral,
  Politician,
  Theorist
};
class Character {
public:
  Character();
  ~Character();

  std::string name;
  std::string surname;
  std::string portraitPath = "";
  Gender gender;

  Arda::Utils::Ideology ideology;
  Type type;
  std::vector<std::string> traits;

  void serialise(Fwg::Utils::Serialisation::Archive &ar) {
    ar &name &surname &portraitPath;
    ar.serialiseEnum(gender);
    ar.serialiseEnum(ideology);
    ar.serialiseEnum(type);
    ar &traits;
  }
  void deserialise(Fwg::Utils::Serialisation::Archive &ar) { serialise(ar); }
};
} // namespace Arda