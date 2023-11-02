#pragma once
#include "SimulationContext.h"
#include <rocky_vsg/Line.h>


struct Gate
{
    simData::GateProperties props;
    simData::GatePrefs prefs;
    simData::GateUpdate update;
};


//! Applies DataStore gate messages to the corresponding visualization components.
class GateAdapter : public SimEntityAdapter
{
public:
    const Gate* create(const simData::GateProperties* props, SimulationContext& sim)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(props, nullptr);
        ROCKY_SOFT_ASSERT_AND_RETURN(props->has_id(), nullptr);

        auto entt_id = sim.entities[props->id()] = sim.registry.create();
        auto& gate = sim.registry.emplace<Gate>(entt_id);
        // give it an empty transform so hosted objects can find it:
        sim.registry.emplace<rocky::Transform>(entt_id);

        applyProps(props, sim);
        return &gate;
    }

    void applyProps(const simData::GateProperties* new_props, SimulationContext& sim)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(new_props, void());

        auto entt_id = sim.entities[new_props->id()];
        auto& gate = sim.registry.get<Gate>(entt_id);

        if (VALUE_CHANGED(hostid, *new_props, gate.props))
        {
            ROCKY_SOFT_ASSERT_AND_RETURN(sim.entities.count(new_props->hostid()) > 0, void());

            auto host_entt_id = sim.entities[new_props->hostid()];
            auto& transform = sim.registry.get<rocky::Transform>(entt_id);
            transform.parent = sim.registry.try_get<rocky::Transform>(host_entt_id);
            ROCKY_SOFT_ASSERT(transform.parent != nullptr);
        }

        gate.props = *new_props;
    }

    void applyPrefs(const simData::GatePrefs* new_prefs, const simData::ObjectId gate_id, SimulationContext& sim)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(new_prefs, void());

        auto entt_id = sim.entities[gate_id];
        auto& gate = sim.registry.get<Gate>(entt_id);

        // detect changes and apply new prefs.
        makeGeometry(*new_prefs, gate.update);

        if (new_prefs->has_commonprefs())
        {
            applyCommonPrefs(&new_prefs->commonprefs(), gate.prefs.commonprefs(), entt_id, sim);
        }

        gate.prefs = *new_prefs;
    }

    void applyUpdate(const simData::GateUpdate* new_update, const simData::ObjectId gate_id, SimulationContext& sim)
    {
        auto entt_id = sim.entities[gate_id];
        auto& gate = sim.registry.get<Gate>(entt_id);
        
        //TODO

        gate.update = *new_update;
    }


    void makeGeometry(const simData::GatePrefs& prefs, const simData::GateUpdate& update) const
    {
        //TODO
    }
};
