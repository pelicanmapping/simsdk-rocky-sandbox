#pragma once
#include "SimulationContext.h"
#include "Platform.h"
#include "Beam.h"
#include "Gate.h"
#include <rocky/vsg/Icon.h>
#include <rocky/vsg/Line.h>


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
            gates.create(props, sim);
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
            auto props = ds->beamProperties(id, &x);
            beams.applyProps(props, sim);
            x.complete(&props);
        }

        else if (type & simData::GATE)
        {
            simData::DataStore::Transaction x;
            auto props = ds->gateProperties(id, &x);
            gates.applyProps(props, sim);
            x.complete(&props);
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
                auto slice = ds->platformUpdateSlice(id);
                if (slice && slice->current())
                {
                    platforms.applyUpdate(slice->current(), id, sim);
                }
            }
            else if (type & simData::BEAM)
            {
                auto slice = ds->beamUpdateSlice(id);
                if (slice && slice->current())
                {
                    beams.applyUpdate(slice->current(), id, sim);
                }
            }
            else if (type & simData::GATE)
            {
                auto slice = ds->gateUpdateSlice(id);
                if (slice && slice->current())
                {
                    gates.applyUpdate(slice->current(), id, sim);
                }
            }
        }
    }
};
