#pragma once
#include "utils/ArdaUtils.h"
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
  Gender gender;

  Arda::Utils::Ideology ideology;
  Type type;
  std::vector<std::string> traits;
};
} // namespace Arda