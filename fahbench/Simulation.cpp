#include <sstream>
#include <fstream>
#include <string>
#include <memory>

#include <stdexcept>
#include <boost/format.hpp>

#include <OpenMM.h>
#include <openmm/serialization/XmlSerializer.h>

#include "StateTests.h"
#include "Simulation.h"
#include "Utils.h"

#ifdef WIN32
#include <float.h>
#define isnan(x) _isnan(x)
#endif

using namespace OpenMM;

using std::string;
using std::map;

class Simulation::Impl {
public:
    Context * pContext{nullptr};
    std::unique_ptr<System> sys{nullptr};
    std::unique_ptr<State> state{nullptr};
    std::unique_ptr<Integrator> intg{nullptr};
};

Simulation::Simulation()
    : work_unit(std::string("dhfr"))
    , platform("OpenCL")
    , precision("single")
    , deviceId(0)
    , platformId(0)
    , verifyAccuracy(true)
    , nan_check_freq(0)
    , run_length(60)
#ifdef _WIN32
    , openmm_plugin_dir(fs::canonical(getExecutableDir() / fs::path("openmm")))
#else
    , openmm_plugin_dir(fs::canonical(getExecutableDir() / fs::path("../lib/openmm")))
#endif
    , impl(new Impl)
{
}


map< string, string > Simulation::getPropertiesMap() const {
    map<string, string> properties;
    if (platform == "CUDA") {
        properties["CudaPrecision"] = precision;
        properties["CudaDeviceIndex"] = std::to_string(deviceId);
    } else if (platform == "OpenCL") {
        properties["OpenCLPrecision"] = precision;
        properties["OpenCLDeviceIndex"] = std::to_string(deviceId);
        properties["OpenCLPlatformIndex"] = std::to_string(platformId);
    }
    return properties;
}

string Simulation::summary() const {
    std::stringstream ss;
    ss << "FAHBench Simulation" << std::endl;
    ss << "-------------------" << std::endl;

    ss << "Plugin directory: " << openmm_plugin_dir << std::endl;
    ss << "Work unit: " << work_unit.codename() << std::endl;
    ss << "WU Name: " << work_unit.fullname() << std::endl;
    ss << "WU Description: " << work_unit.description() << std::endl;
    ss << "System XML: " << work_unit.system_fn() << std::endl;
    ss << "Integrator XML: " << work_unit.integrator_fn() << std::endl;
    ss << "State XML: " << work_unit.state_fn() << std::endl;
    ss << "Step chunk: " << work_unit.step_chunk() << std::endl;
    ss << "Device ID " << deviceId << "; Platform " << platform;
    if (platform == "OpenCL") {
        ss << "; Platform ID " << platformId << std::endl;
    } else {
        ss << std::endl;
    }
    ss << "Run length: " << run_length.count() << "s" << std::endl;
    return ss.str();
}


SimulationResult Simulation::run(const Updater & update) {

    if (!impl->pContext){
        prepare(update);
    }

    update.message("Starting Benchmark");
    float score = benchmark(*impl->pContext, update);
    delete impl->pContext;

    // This still uses try_lock which returns "false" if it can't get the lock
    // So it is theoretically possible for it to return "Finished" even if it should have been cancelled.
    if (update.cancelled()) {
        return SimulationResult(ResultStatus::CANCELLED);
    } else {
        update.message("Benchmarking finished");
        SimulationResult result(score, impl->sys->getNumParticles());
        return result;
    }
}

float Simulation::benchmark(Context & context, const Updater & update) const {
    float step_size_ns = context.getIntegrator().getStepSize() / 1000.0;
    int steps = 0;
    int step_chunk = work_unit.step_chunk();

    using day = std::chrono::duration<float, std::ratio<86400>>;
    using ms = std::chrono::milliseconds;
    auto run_length_ms = std::chrono::duration_cast<ms>(run_length);

    // This step call ensures everything has been JIT compiled
    context.getIntegrator().step(1);

    int last_nan_check = 0;
    std::chrono::steady_clock clock;
    auto clock_start = clock.now();

    while (!update.cancelled()) {
        context.getIntegrator().step(step_chunk);
        steps += step_chunk;

        auto clock_now = clock.now();
        auto duration = clock_now - clock_start;

        if (duration > run_length) {
            break;
        }

        float per_day = 1.0 / std::chrono::duration_cast<day>(duration).count();
        float ns_day = steps * step_size_ns * per_day;
        update.progress(std::chrono::duration_cast<ms>(duration).count(),
                        run_length_ms.count(), ns_day);

        if (nan_check_freq > 0 && steps - last_nan_check > nan_check_freq) {
            StateTests::checkForNans(context.getState(State::Positions | State::Velocities | State::Forces));
            last_nan_check = steps;
        }
    }
    // last getState makes sure everything in the queue has been flushed.
    State finalState = context.getState(State::Positions | State::Velocities | State::Forces | State::Energy);

    auto duration = clock.now() - clock_start;
    float per_day = 1.0 / std::chrono::duration_cast<day>(duration).count();
    float ns_day = steps * step_size_ns * per_day;

    if (!update.cancelled()) {
        StateTests::checkForNans(finalState);
        StateTests::checkForDiscrepancies(finalState);
    }
    update.progress(run_length_ms.count(), run_length_ms.count(), ns_day);
    return ns_day;
}

SimulationResult Simulation::prepare(const Updater& update)
{
    if (impl->pContext)
        return {ResultStatus::FAILED};
    update.message(boost::format("Loading plugins from plugin directory"));
    Platform::loadPluginsFromDirectory(openmm_plugin_dir.string());
    update.message(boost::format("Number of registered plugins: %1%") % Platform::getNumPlatforms());
    Platform & platform = Platform::getPlatformByName(this->platform);

    update.message("Deserializing input files: system");
    impl->sys.reset(loadObject<System>(work_unit.system_fn()));
    update.message("Deserializing input files: state");
    impl->state.reset(loadObject<State>(work_unit.state_fn()));
    update.message("Deserializing input files: integrator");
    impl->intg.reset(loadObject<Integrator>(work_unit.integrator_fn()));
    if (update.cancelled()) return {ResultStatus::CANCELLED};


    update.message("Creating context (may take several minutes)");
    impl->pContext = new Context(*impl->sys, *impl->intg, platform, getPropertiesMap());
    auto &context = *impl->pContext;
    context.setState(*impl->state);
    if (update.cancelled()) {
        delete impl->pContext;
        return {ResultStatus::CANCELLED};
    }


    if (verifyAccuracy) {
        update.message("Checking accuracy against reference code");
        std::unique_ptr<Integrator> refIntg(loadObject<Integrator>(work_unit.integrator_fn()));
        update.message("Creating reference context (may take several minutes)");
        Context refContext(*impl->sys, *refIntg, Platform::getPlatformByName("Reference"));
        refContext.setState(*impl->state);
        if (update.cancelled()) {
            delete impl->pContext;
            return {ResultStatus::CANCELLED};
        }
        update.message("Comparing forces and energy");
        StateTests::compareForcesAndEnergies(refContext.getState(State::Forces | State::Energy),
                context.getState(State::Forces | State::Energy));
        if (update.cancelled()){
            delete impl->pContext;
            return {ResultStatus::CANCELLED};
        }
    }

    return {ResultStatus::QUEUED};
}

Simulation::~Simulation()
{
    delete impl;
}

Simulation::Simulation(Simulation&& other) noexcept:
work_unit(std::move(other.work_unit)),
platform(std::move(other.platform)),
precision(std::move(other.precision)),
deviceId(other.deviceId),
platformId(other.platformId),
verifyAccuracy(other.verifyAccuracy),
nan_check_freq(other.nan_check_freq),
run_length(other.run_length),
openmm_plugin_dir(std::move(other.openmm_plugin_dir)),
impl(other.impl)
{
    other.impl = nullptr;
}


template<class T>
T * Simulation::loadObject(const string & fname) const {
    std::ifstream f(fname);
    if (!f) {
        throw std::runtime_error(boost::str(boost::format("cannot open %1%") % fname));
    }
    std::istream & s = f;
    return XmlSerializer::deserialize<T>(s);
}



