#include <rocky_vsg/Application.h>
#include <rocky/TMSImageLayer.h>
#include <simCore.h>
#include <simData.h>
#include "DataStoreAdapter.h"

#define EXAMPLE_AIRPLANE_ICON "https://github.com/gwaldron/osgearth/blob/master/data/airport.png?raw=true"

int
error_out(const rocky::Status& s)
{
    std::cout << s.message << std::endl;
    return -1;
}

/// add default data to the map.
rocky::Status
createDefaultExampleMap(rocky::Application& app)
{
    auto imagery = rocky::TMSImageLayer::create();
    imagery->uri = rocky::URI("https://readymap.org/readymap/tiles/1.0.0/7/");
    app.mapNode->map->layers().add(imagery);
    return imagery->status();
}

/// create a platform and add it to 'dataStore'
///@return id for the new platform
simData::ObjectId
addPlatform(simData::DataStore& dataStore)
{
    simData::DataStore::Transaction xaction;
    simData::PlatformProperties* newProps = dataStore.addPlatform(&xaction);
    simData::ObjectId id = newProps->id();
    xaction.complete(&newProps);

    simData::PlatformPrefs* prefs = dataStore.mutable_platformPrefs(id, &xaction);
    prefs->mutable_commonprefs()->set_name("Airplane");
    prefs->set_icon(EXAMPLE_AIRPLANE_ICON);
    prefs->set_scale(2.0f);
    prefs->set_dynamicscale(true);
    prefs->set_circlehilightcolor(0xffffffff);
    prefs->mutable_commonprefs()->set_draw(true);
    prefs->mutable_commonprefs()->mutable_labelprefs()->set_draw(true);
    prefs->mutable_commonprefs()->mutable_labelprefs()->set_overlayfontpointsize(36.0f);
    prefs->mutable_commonprefs()->mutable_labelprefs()->set_alignment(simData::ALIGN_RIGHT_CENTER);
    prefs->mutable_commonprefs()->mutable_localgrid()->mutable_speedring()->set_timeformat(simData::ELAPSED_SECONDS);
    prefs->mutable_commonprefs()->mutable_localgrid()->mutable_speedring()->set_radius(2);
    xaction.complete(&prefs);

    return id;
}

/// create a beam and add it to 'dataStore'
///@return id for new beam
simData::ObjectId
addBeam(simData::ObjectId hostId, simData::DataStore& dataStore)
{
    simData::DataStore::Transaction xaction;
    simData::BeamProperties* beamProps = dataStore.addBeam(&xaction);
    simData::ObjectId id = beamProps->id();
    beamProps->set_hostid(hostId);
    xaction.complete(&beamProps);

    simData::BeamPrefs* prefs = dataStore.mutable_beamPrefs(id, &xaction);
    prefs->set_azimuthoffset(simCore::DEG2RAD * 0.0);
    prefs->set_verticalwidth(simCore::DEG2RAD * 25.0);
    prefs->set_horizontalwidth(simCore::DEG2RAD * 30.0);
    prefs->mutable_commonprefs()->set_color(0x7FFF007F);
    prefs->set_beamdrawmode(simData::BeamPrefs::WIRE_ON_SOLID);
    xaction.complete(&prefs);

    return id;
}

/// create a gate and add it to 'dataStore'
///@return id for new gate
simData::ObjectId
addGate(simData::ObjectId hostId, simData::DataStore& dataStore)
{
    simData::DataStore::Transaction transaction;

    simData::GateProperties* gateProps = dataStore.addGate(&transaction);
    simData::ObjectId result = gateProps->id();
    gateProps->set_hostid(hostId);
    transaction.complete(&gateProps);

    simData::GatePrefs* gatePrefs = dataStore.mutable_gatePrefs(result, &transaction);
    gatePrefs->set_gateazimuthoffset(simCore::DEG2RAD * 0.0);
    transaction.complete(&gatePrefs);

    return result;
}

int
main(int argc, char** argv)
{
    if (argc > 1 && std::string(argv[argc - 1]) == "--pause")
    {
        std::cout << "Press enter to continue." << std::endl;
        getchar();
    }

    simCore::checkVersionThrow();

    // Application object for the 3D map display.
    rocky::Application app(argc, argv);
    rocky::Log()->set_level(rocky::log::level::info);

    // Add some example data to the base map
    rocky::Status s = createDefaultExampleMap(app);
    if (s.failed())
        return error_out(s);

    // The SIM SDK data stream.
    simData::MemoryDataStore data_store;

    // The Controller creates and updates visualization objects from the data store stream
    auto adapter = std::make_shared<DataStoreAdapter>(app);

    // Connect the controller to the data store.
    data_store.addListener(adapter);

    // Create and configure the sim objects:
    simData::ObjectId plat_id = addPlatform(data_store);
    simData::ObjectId beam_id = addBeam(plat_id, data_store);
    simData::ObjectId gate_id = addGate(beam_id, data_store);

    // Build a simulation
    auto LLA = vsg::dvec3{ 2.0, 35.0, 10000.0 };
    auto geo_to_ecef = rocky::SRS::WGS84.to(rocky::SRS::ECEF);
    simCore::Vec3 ecef;
    for (double time = 0.0; time <= 60.0; time += 0.01)
    {
        LLA.x += 0.001;
        geo_to_ecef.transform(LLA, ecef);

        simData::DataStore::Transaction x;
        auto platform_update = data_store.addPlatformUpdate(plat_id, &x);
        platform_update->set_time(time);
        platform_update->setPosition(ecef);
        x.complete(&platform_update);

        auto beam_update = data_store.addBeamUpdate(beam_id, &x);
        beam_update->set_time(time);
        beam_update->set_azimuth(0.5*sin(time));
        beam_update->set_range(35000.0);
        x.complete(&beam_update);
    }
    
    // Run the frame loop
    auto start = std::chrono::steady_clock::now();
    while (app.frame())
    {
        auto now = std::chrono::steady_clock::now();
        double time = 1e-6 * (double)std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();
        data_store.update(time);
        adapter->update(&data_store, { plat_id, beam_id });
    }

    return 0;
}
