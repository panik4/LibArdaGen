#pragma once
#include "utils/ArdaUtils.h"
#include "utils/SerialisationFwd.h"
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

  template<class Archive>
  void serialize(Archive &ar, const unsigned int /*version*/) {
    ar &name &surname &portraitPath &gender &ideology &type &traits;
  }
};
} // namespace Arda