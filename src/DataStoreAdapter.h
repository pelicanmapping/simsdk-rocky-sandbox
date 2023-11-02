#pragma once
#include "SimulationContext.h"
#include "Platform.h"
#include "Beam.h"
#include "Gate.h"
#include <rocky_vsg/Icon.h>
#include <rocky_vsg/Line.h>


//! Listener that relays DataStore messages to the appropriate Adapter.
class DataStoreAdapter : public simData::DataStore::DefaultListener
{
public:
    rocky::Application& app;

    SimulationContext sim;
    PlatformAdapter platforms;
    BeamAdapter beams;
    GateAdapter gates;

    DataStoreAdapter(rocky::Application& app_) :
        app(app_),
        sim({ app.instance, app.entities })
    {
        //nop        
    }

    void onAddEntity(simData::DataStore* ds, simData::ObjectId id, simData::ObjectType type) override
    {
        if (type & simData::PLATFORM)
        {
            simData::DataStore::Transaction x;
            auto props = ds->platformProperties(id, &x);
            platforms.create(props, sim);
            x.complete(&props);
        }

        else if (type & simData::BEAM)
        {
            simData::DataStore::Transaction x;
            auto props = ds->beamProperties(id, &x);
            beams.create(props, sim);
            x.complete(&props);
        }

        else if (type & simData::GATE)
        {
            simData::DataStore::Transaction x;
            auto props = ds->gateProperties(id, &x);
            //gates.create(props, sim);
            x.complete(&props);
        }
    }

    void onPropertiesChange(simData::DataStore* ds, simData::ObjectId id) override
    {
        auto type = ds->objectType(id);

        if (type & simData::PLATFORM)
        {
            simData::DataStore::Transaction x;
            auto props = ds->platformProperties(id, &x);
            platforms.applyProps(props, sim);
            x.complete(&props);
        }

        else if (type & simData::BEAM)
        {
            simData::DataStore::Transaction x;
            auto prefs = ds->beamPrefs(id, &x);
            beams.applyPrefs(prefs, id, sim);
            x.complete(&prefs);
        }

        else if (type & simData::GATE)
        {
            simData::DataStore::Transaction x;
            auto prefs = ds->gatePrefs(id, &x);
            //gates.applyPrefs(prefs, id);
            x.complete(&prefs);
        }
    }

    void onPrefsChange(simData::DataStore* ds, simData::ObjectId id) override
    {
        auto type = ds->objectType(id);

        if (type & simData::PLATFORM)
        {
            simData::DataStore::Transaction x;
            auto prefs = ds->platformPrefs(id, &x);
            platforms.applyPrefs(prefs, id, sim);
            x.complete(&prefs);
        }

        else if (type & simData::BEAM)
        {
            simData::DataStore::Transaction x;
            auto prefs = ds->beamPrefs(id, &x);
            beams.applyPrefs(prefs, id, sim);
            x.complete(&prefs);
        }

        else if (type & simData::GATE)
        {
            simData::DataStore::Transaction x;
            auto prefs = ds->gatePrefs(id, &x);
            gates.applyPrefs(prefs, id, sim);
            x.complete(&prefs);
        }
    }

    void update(simData::DataStore* ds, const std::vector<simData::ObjectId>& ids)
    {
        for (auto& id : ids)
        {
            auto type = ds->objectType(id);

            if (type & simData::PLATFORM)
            {
                auto plat_slice = ds->platformUpdateSlice(id);
                if (plat_slice && plat_slice->current())
                {
                    platforms.applyUpdate(plat_slice->current(), id, sim);
                }
            }
            else if (type & simData::BEAM)
            {
                auto beam_slice = ds->beamUpdateSlice(id);
                if (beam_slice && beam_slice->current())
                {
                    beams.applyUpdate(beam_slice->current(), id, sim);
                }
            }
            else if (type & simData::GATE)
            {
                auto gate_slice = ds->gateUpdateSlice(id);
                if (gate_slice && gate_slice->current())
                {
                    gates.applyUpdate(gate_slice->current(), id, sim);
                }
            }
        }
    }
};
