#pragma once
#include "areas/Province.h"
#include "civilisation/CivilisationLayer.h"
#include "culture/Culture.h"
#include "culture/Religion.h"
#include "generic/VictoryPoint.h"
#include <unordered_set>
namespace Arda {
enum class PositionType {
  Standstill,
  StandstillRG,
  Attacking,
  Defending,
  UnitMoving,
  UnitMovingRG,
  UnitDisembarking,
  UnitDisembarkingRG,
  ShipInPort,
  ShipInPortMoving,
  VictoryPoint
};
struct ScenarioPosition {
  Fwg::Position position;
  PositionType type;
  int typeIndex;
};
class ArdaProvince : public Fwg::Areas::Province {
public:
  std::string owner;
  std::string terrainType;
  std::unordered_set<Civilization::TopographyType> topographyTypes;
  double populationDensity = 0.0;
  double worldPopulationShare = 0.0;
  double population = 0;
  double worldGdpShare = 0.0;
  double gdp = 0;

  std::shared_ptr<VictoryPoint> victoryPoint;
  // containers
  std::vector<Arda::ArdaProvince> neighbours;
  // these positions are used for victory points, units, etc
  std::vector<ScenarioPosition> positions;
  // the sum here should ALWAYS be 1
  std::map<std::shared_ptr<Arda::Religion>, double> religions;
  // the sum here should ALWAYS be 1
  std::map<std::shared_ptr<Arda::Culture>, double> cultures;

  // constructors/destructor
  ArdaProvince(std::shared_ptr<Fwg::Areas::Province> province);
  ArdaProvince();
  ~ArdaProvince();
  // operators
  bool operator==(const Arda::ArdaProvince &right) const {
    return ID == right.ID;
  };
  bool operator<(const Arda::ArdaProvince &right) const {
    return ID < right.ID;
  };
  std::string toHexString(bool prefix, bool uppercase);
};
} // namespace Arda
