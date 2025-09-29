#pragma once
#include "areas/ArdaRegion.h"
#include "culture/Culture.h"
#include "entities/Colour.h"
#include "language/LanguageGroup.h"
#include <string>
namespace Arda {

class CultureGroup {
  std::vector<std::shared_ptr<Culture>> cultures;
  Fwg::Gfx::Colour colour;
  std::shared_ptr<Arda::LanguageGroup> languageGroup;
  std::shared_ptr<ArdaRegion> center;
  std::vector<std::shared_ptr<ArdaRegion>> regions;
  VisualType visualType;

public:
  std::string name;
  // Constructor
  CultureGroup(const std::string &name, const Fwg::Gfx::Colour &colour)
      : name(name), colour(colour) {}

  // Method to add a culture
  void addCulture(const std::shared_ptr<Culture> &culture) {
    cultures.push_back(culture);
  }

  // Method to remove a culture
  void removeCulture(const std::shared_ptr<Culture> &culture) {
    culture->cultureGroup = nullptr;
    cultures.erase(std::remove(cultures.begin(), cultures.end(), culture),
                   cultures.end());
  }

  void addRegion(const std::shared_ptr<ArdaRegion> &region) {
    regions.push_back(region);
  }

  void removeRegion(const std::shared_ptr<ArdaRegion> &region) {
    regions.erase(std::remove(regions.begin(), regions.end(), region),
                  regions.end());
  }

  void setCenter(const std::shared_ptr<ArdaRegion> &region) { center = region; }
  void setLanguageGroup(const std::shared_ptr<Arda::LanguageGroup> &group) {
    languageGroup = group;

    this->name = languageGroup->name;
    for (auto i = 0; i < cultures.size(); i++) {
      cultures[i]->language = languageGroup->languages[i];
      cultures[i]->name =
          cultures[i]->language->generateGenericCapitalizedWord();
    }
  }
  std::shared_ptr<ArdaRegion> getCenter() { return center; }
  std::vector<std::shared_ptr<ArdaRegion>> getRegions() { return regions; }
  std::vector<std::shared_ptr<Culture>> getCultures() { return cultures; }
  std::string getName() { return name; }
  Fwg::Gfx::Colour getColour() { return colour; }
  std::shared_ptr<Arda::LanguageGroup> getLanguageGroup() {
    return languageGroup;
  }

  // determines the visual looks of the culture group according to their
  // geographic location
  void determineVisualType();

  VisualType getVisualType() { return visualType; }
};

} // namespace Arda