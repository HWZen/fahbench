//
// Created by HWZ on 2023/12/4.
//

#ifndef FAHBENCH_CFAHBENCHLIB_H
#define FAHBENCH_CFAHBENCHLIB_H

#ifdef CFAHBENCH_EXPORTS
#define CFAHBENCH_EXPORT extern "C" __declspec(dllexport)
#else
#define CFAHBENCH_EXPORT extern "C" __declspec(dllimport)
#endif

CFAHBENCH_EXPORT void *CreateSimulation(size_t timeMs) noexcept(false);

CFAHBENCH_EXPORT void DestroySimulation(void *simulation) noexcept(false);

using progressFunc = void(*)(void*, int, int, float);

using messageFunc = void(*)(void*, const char *);

using cancelledFunc = bool(*)(void*);

CFAHBENCH_EXPORT float RunSimulation(void *pVoid,
        progressFunc pf,
        messageFunc mf,
        cancelledFunc cf,
        void *data) noexcept(false);

CFAHBENCH_EXPORT void PrepareSimulation(void *simulation,
        progressFunc pf,
        messageFunc mf,
        cancelledFunc cf,
        void *data) noexcept(false);

CFAHBENCH_EXPORT const char* GetSimulationSummary(void *simulation) noexcept(false);

#endif //FAHBENCH_CFAHBENCHLIB_H
