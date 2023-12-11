//
// Created by HWZ on 2023/12/4.
//

#include "CFAHBenchLib.h"
#include "Simulation.h"
#include "GPUInfo.h"

#include <functional>
#include <thread>

using namespace std::chrono_literals;

void *CreateSimulation(size_t timeMs) noexcept(false) {

    auto gpu_devices = GPUInfo::getOpenCLDevices();

    std::vector<Simulation> res;
    for (auto& gpu_device : gpu_devices) {
        if (gpu_device.device().find("Intel") != std::string::npos) {
            continue;
        }
        Simulation simulation;
        simulation.run_length = std::chrono::seconds(timeMs / 1000);
        simulation.work_unit = WorkUnit(std::string{"dhfr"});
        simulation.deviceId = gpu_device.device_id();

        res.emplace_back(std::move(simulation));
    }

    return new std::vector<Simulation>(std::move(res));
}

void DestroySimulation(void *simulation) noexcept(false) {
    delete (std::vector<Simulation> *) simulation;
}



class CUpdater : public Updater {
    progressFunc progressF;
    messageFunc messageF;
    cancelledFunc cancelledF;
    void *data;

public:

    CUpdater(progressFunc progressF, messageFunc messageF, cancelledFunc cancelledF, void *data) :
            progressF(progressF), messageF(messageF), cancelledF(cancelledF), data(data)
    {

    }

    void progress(int step, int total_step, float score) const override
    {
        if (progressF) {
            progressF(data, step, total_step, score);
        }
    }

    void message(std::string string1) const override
    {
        if (messageF) {
            messageF(data, string1.c_str());
        }
    }

    void message(boost::format format) const override
    {

    }

    bool cancelled() const override
    {
        if (cancelledF) {
            return cancelledF(data);
        }
        return false;
    }
};

float RunSimulation(void *pVoid, progressFunc pf, messageFunc mf, cancelledFunc cf, void *data) noexcept(false) {
    auto simulations = (std::vector<Simulation> *) pVoid;

    float res{0};
    for (auto &simulation : *simulations) {
        CUpdater updater(pf, mf, cf, data);
        res = std::max(res, simulation.run(updater).scaled_score());
    }
    return res;
}

void PrepareSimulation(void *pVoid, progressFunc pf, messageFunc mf, cancelledFunc cf, void *data) noexcept(false){
    auto simulations = (std::vector<Simulation> *) pVoid;

    for (auto &simulation : *simulations) {
        CUpdater updater(pf, mf, cf, data);
        simulation.prepare(updater);
    }
}

const char* GetSimulationSummary(void* pVoid) noexcept(false)
{
    static std::string res;
    res.clear();
    auto simulations = (std::vector<Simulation> *) pVoid;
    for (auto& simulation : *simulations) {
        res += simulation.summary();
    }
    return res.c_str();
}
