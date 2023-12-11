#ifndef SIMULATION_H_
#define SIMULATION_H_

#include <string>
#include <sstream>
#include <map>
#include <chrono>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>

#include <OpenMM.h>

#include "DLLDefines.h"
#include "Updater.h"
#include "WorkUnit.h"
#include "SimulationResult.h"



using std::string;
using std::map;
namespace fs = boost::filesystem;

class FAHBENCH_EXPORT Simulation {

public:
    Simulation();

    Simulation(Simulation&&) noexcept;

    WorkUnit work_unit;

    std::string platform;
    std::string precision;

    int deviceId;
    int platformId;

    bool verifyAccuracy;
    int nan_check_freq;
    std::chrono::seconds run_length;

    map<string, string> getPropertiesMap() const;

    std::string summary() const;

    SimulationResult prepare(const Updater& update);

    SimulationResult run(const Updater & update);

    ~Simulation();

private:
    fs::path openmm_plugin_dir;

    class Impl;
    Impl * impl{ nullptr};

    template<class T>
    T * loadObject(const string & fname) const;

    float benchmark(OpenMM::Context & context, const Updater & update) const;

};


#endif // SIMULATION_H_
