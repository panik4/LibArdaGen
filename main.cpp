#define D_SCL_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#pragma warning(disable : 4996)
#include "FastWorldGenerator.h"
#include "Utils/Logging.h"
#include "Utils/Utils.h"
#include "generic/ScenarioGenerator.h"
#include <filesystem>

int main() {
  Arda::ArdaGen scenarioGen;
  scenarioGen.genHeight();
}