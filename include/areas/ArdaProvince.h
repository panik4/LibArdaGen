#pragma once
#include "areas/Province.h"
#include "generic/VictoryPoint.h"
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
class ArdaProvince {
public:
  int ID;
  std::string name;
  std::string owner;
  std::string terrainType;
  double popFactor;
  double devFactor;
  double cityShare;
  std::shared_ptr<Fwg::Areas::Province> baseProvince;
  std::shared_ptr<VictoryPoint> victoryPoint;
  // containers
  std::vector<Arda::ArdaProvince> neighbours;
  // these positions are used for victory points, units, etc
  std::vector<ScenarioPosition> positions;

  // constructors/destructor
  ArdaProvince(std::shared_ptr<Fwg::Areas::Province> province);
  ArdaProvince();
  ~ArdaProvince();
  // operators
  bool operator==(const Arda::ArdaProvince &right) const { return ID == right.ID; };
  bool operator<(const Arda::ArdaProvince &right) const { return ID < right.ID; };
  std::string toHexString();
};
} // namespace Arda
