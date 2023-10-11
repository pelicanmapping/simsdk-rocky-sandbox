#include <rocky_vsg/Application.h>
#include <rocky_vsg/Icon.h>
#include <rocky_vsg/Label.h>
#include <rocky_vsg/Mesh.h>
#include <rocky_vsg/Line.h>

#include <rocky/TMSImageLayer.h>

#include <vsg/io/read.h>

#include <simCore.h>
#include <simData.h>

#include <unordered_map>
#include <vsgXchange/all.h>

#define EXAMPLE_AIRPLANE_ICON "https://github.com/gwaldron/osgearth/blob/master/data/airport.png?raw=true"

int
error_out(const rocky::Status& s)
{
    std::cout << s.message << std::endl;
    return -1;
}

// Creates an image to use when the image we want is missing.
std::shared_ptr<rocky::Image> createMissingImage()
{
    auto image = rocky::Image::create(rocky::Image::R8G8B8A8_UNORM, 15, 15);
    image->fill({ 1,1,1,1 });
    for (unsigned i = 1; i < image->width()-1; ++i) {
        image->write({ 1,0,0,1 }, i, i);
        image->write({ 1,0,0,1 }, i, image->height() - 1 - i);
    }
    return image;
}

#define VALUE_CHANGED(P0, P1, NAME) \
    ((P0).has_##NAME() && (!(P1).has_##NAME() || ((P0).##NAME() != (P1).##NAME())))


using ObjectToEntityLUT = std::unordered_map<simData::ObjectId, entt::entity>;

struct SimulationContext
{
    rocky::InstanceVSG& instance;
    rocky::ECS::Registry& registry;
    ObjectToEntityLUT entities;
    std::unordered_map<std::string, vsg::ref_ptr<vsg::Font>> fonts;
    std::shared_ptr<spdlog::logger> log = rocky::Log()->clone("simvis");

    vsg::ref_ptr<vsg::Font>& get_font(const std::string& name)
    {
        auto& font = fonts[name];
        if (!font.valid())
        {
            font = vsg::read_cast<vsg::Font>(name, instance.runtime().readerWriterOptions);
        }
        return font;
    }
};

struct Platform
{
    simData::PlatformProperties props;
    simData::PlatformPrefs prefs;
    simData::PlatformUpdate update;
};

struct Beam
{
    simData::BeamProperties props;
    simData::BeamPrefs prefs;
    simData::BeamUpdate update;
};

class CommonController
{
public:
    void applyCommonPrefs(const simData::CommonPrefs* new_prefs, const simData::CommonPrefs& old_prefs, entt::entity entity, SimulationContext& sim)
    {
        if (VALUE_CHANGED(*new_prefs, old_prefs, name))
        {
            auto& label = sim.registry.get_or_emplace<rocky::Label>(entity);
            label.text = new_prefs->name();
            if (!label.style.font) label.style.font = sim.get_font("arial.ttf");
            label.dirty();
        }

        if (new_prefs->has_labelprefs())
        {
            applyLabelPrefs(&new_prefs->labelprefs(), old_prefs.labelprefs(), entity, sim);
        }
    }

    void applyLabelPrefs(const simData::LabelPrefs* new_prefs, const simData::LabelPrefs& old_prefs, entt::entity entity, SimulationContext& sim)
    {
        if (VALUE_CHANGED(*new_prefs, old_prefs, overlayfontname))
        {
            auto& font = sim.get_font(new_prefs->overlayfontname());
            if (font)
            {
                auto& label = sim.registry.get_or_emplace<rocky::Label>(entity);
                label.style.font = font;
                label.dirty();
            }
        }

        if (VALUE_CHANGED(*new_prefs, old_prefs, overlayfontpointsize))
        {
            auto& label = sim.registry.get_or_emplace<rocky::Label>(entity);
            label.style.pointSize = new_prefs->overlayfontpointsize();
            label.dirty();
        }

        if (VALUE_CHANGED(*new_prefs, old_prefs, alignment))
        {
            auto& label = sim.registry.get_or_emplace<rocky::Label>(entity);
            auto align = new_prefs->alignment();
            switch (align) {
            case simData::TextAlignment::ALIGN_RIGHT_CENTER:
                label.style.horizontalAlignment = vsg::StandardLayout::RIGHT_ALIGNMENT;
                label.style.verticalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
                break;
            }
        }
    }
};

class PlatformController : public CommonController
{
public:

    const Platform* create(const simData::PlatformProperties* props, SimulationContext& sim)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(props, nullptr);
        ROCKY_SOFT_ASSERT_AND_RETURN(props->has_id(), nullptr);

        sim.log->info("Add platform with id=" + std::to_string(props->id()));
        auto platform_entity = sim.entities[props->id()] = sim.registry.create();
        auto& platform = sim.registry.emplace<Platform>(platform_entity);
        sim.registry.emplace<rocky::Transform>(platform_entity);
        applyProps(props, sim);
        return &platform;
    }

    void applyProps(const simData::PlatformProperties* new_props, SimulationContext& sim)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(new_props, void());
        ROCKY_SOFT_ASSERT_AND_RETURN(new_props->has_id(), void());

        auto entity = sim.entities[new_props->id()];
        auto& platform = sim.registry.emplace<Platform>(entity);

        platform.props = *new_props;        
    }

    void applyPrefs(const simData::PlatformPrefs* new_prefs, const simData::ObjectId id, SimulationContext& sim)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(new_prefs != nullptr, void());

        auto platform_entity = sim.entities[id];
        auto& platform = sim.registry.get<Platform>(platform_entity);

        if (VALUE_CHANGED(*new_prefs, platform.prefs, icon))
        {
            auto& icon = sim.registry.get_or_emplace<rocky::Icon>(platform_entity);

            auto io = sim.instance.ioOptions();
            auto image = io.services.readImageFromURI(new_prefs->icon(), io);
            if (image.status.ok())
            {
                icon.image = image.value;
            }
            else
            {
                sim.log->warn("Icon \"" + new_prefs->icon() + "\" cannot load: " + image.status.toString());
                icon.image = createMissingImage();
            }
            icon.dirtyImage();
        }

        if (VALUE_CHANGED(*new_prefs, platform.prefs, scale))
        {
            auto& icon = sim.registry.get_or_emplace<rocky::Icon>(platform_entity);
            if (icon.image && icon.image->valid())
            {
                icon.style.size_pixels = icon.image->width() * new_prefs->scale();
            }
            platform.prefs.set_scale(new_prefs->scale());
        }

        if (new_prefs->has_commonprefs())
        {
            applyCommonPrefs(&new_prefs->commonprefs(), platform.prefs.commonprefs(), platform_entity, sim);
        }

        platform.prefs = *new_prefs;
    }

    void applyUpdate(const simData::PlatformUpdate* new_update, const simData::ObjectId id, SimulationContext& sim)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(new_update, void());

        auto platform_entity = sim.entities[id];
        auto& platform = sim.registry.get<Platform>(platform_entity);

        if (new_update->has_x() || new_update->has_y() || new_update->has_z())
        {
            auto entity = sim.entities[id];
            auto& transform = sim.registry.get_or_emplace<rocky::Transform>(platform_entity);
            rocky::GeoPoint point(rocky::SRS::ECEF, new_update->x(), new_update->y(), new_update->z());
            transform.setPosition(point);
        }

        platform.update = *new_update;
    }
};

class BeamController : public CommonController
{
public:
    const Beam* create(const simData::BeamProperties* props, SimulationContext& sim)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(props, nullptr);
        ROCKY_SOFT_ASSERT_AND_RETURN(props->has_id(), nullptr);

        auto beam_entity = sim.entities[props->id()] = sim.registry.create();
        Beam& beam = sim.registry.emplace<Beam>(beam_entity);
        // give it an empty transform so hosted objects can find it:
        sim.registry.emplace<rocky::Transform>(beam_entity);

        applyProps(props, sim);
        return &beam;
    }

    void applyProps(const simData::BeamProperties* new_props, SimulationContext& sim)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(new_props, void());

        auto beam_entity = sim.entities[new_props->id()];
        auto& beam = sim.registry.get<Beam>(beam_entity);

        if (VALUE_CHANGED(*new_props, beam.props, hostid))
        {
            ROCKY_SOFT_ASSERT_AND_RETURN(sim.entities.count(new_props->hostid()) > 0, void());

            auto host_entity = sim.entities[new_props->hostid()];
            auto& transform = sim.registry.get<rocky::Transform>(beam_entity);
            transform.parent = sim.registry.try_get<rocky::Transform>(host_entity);
            ROCKY_SOFT_ASSERT(transform.parent != nullptr);
        }

        beam.props = *new_props;
    }

    void applyPrefs(const simData::BeamPrefs* new_prefs, const simData::ObjectId beam_id, SimulationContext& sim)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(new_prefs, void());

        auto beam_entity = sim.entities[beam_id];
        auto& beam = sim.registry.get<Beam>(beam_entity);

        // detect changes and apply new prefs.
        auto& line = sim.registry.emplace_or_replace<rocky::Line>(beam_entity);
        makeBeamGeometry(line, *new_prefs, beam.update);

        if (new_prefs->has_commonprefs())
        {
            applyCommonPrefs(&new_prefs->commonprefs(), beam.prefs.commonprefs(), beam_entity, sim);
        }

        beam.prefs = *new_prefs;
    }

    void applyUpdate(const simData::BeamUpdate* new_update, const simData::ObjectId beam_id, SimulationContext& sim)
    {
        auto beam_entity = sim.entities[beam_id];
        auto& beam = sim.registry.get<Beam>(beam_entity);

        if (VALUE_CHANGED(*new_update, beam.update, azimuth))
        {
            auto& transform = sim.registry.get<rocky::Transform>(beam_entity);
            vsg::dquat rot;
            rocky::euler_radians_to_quaternion(beam.update.elevation(), 0.0, new_update->azimuth(), rot);
            transform.local_matrix = vsg::rotate(rot);
        }

        if (VALUE_CHANGED(*new_update, beam.update, elevation))
        {
            auto& transform = sim.registry.get<rocky::Transform>(beam_entity);
            vsg::dquat rot;
            rocky::euler_radians_to_quaternion(new_update->elevation(), 0.0, beam.update.azimuth(), rot);
            transform.local_matrix = vsg::rotate(rot);
        }

        if (VALUE_CHANGED(*new_update, beam.update, range))
        {
            auto& line = sim.registry.emplace_or_replace<rocky::Line>(beam_entity);
            makeBeamGeometry(line, beam.prefs, *new_update);
        }

        beam.update = *new_update;
    }

    void makeBeamGeometry(rocky::Line& line, const simData::BeamPrefs& prefs, const simData::BeamUpdate& update) const
    {
        auto range = update.range();
        auto width_rad = prefs.horizontalwidth();
        auto height_rad = prefs.verticalwidth();
        auto beam_offset = prefs.beampositionoffset();

        // x-forward
        double x = range * acos(width_rad * 0.5);
        double y = range * asin(width_rad * 0.5);
        double z = range * asin(height_rad * 0.5);
        
        vsg::dvec3 origin(beam_offset.x(), beam_offset.y(), beam_offset.z());
        auto UL = vsg::dvec3(x, y, z) + origin;
        auto UR = vsg::dvec3(x, -y, z) + origin;
        auto LL = vsg::dvec3(x, y, -z) + origin;
        auto LR = vsg::dvec3(x, -y, -z) + origin;

        std::vector<std::vector<vsg::dvec3>> strips =
        {
            { UR, UL, LL, LR, UR },
            { UR, origin, UL },
            { LR, origin, LL }
        };

        for (auto& strip : strips)
        {
            line.push(strip.begin(), strip.end());
        }

        line.style = rocky::LineStyle();
        line.style->color = { 1, 1, 0, 1 };
        line.style->width = 2.0f;

        line.dirty();
    }
};

class DataStoreAdapter : public simData::DataStore::DefaultListener
{
public:
    rocky::Application& app;

    SimulationContext sim;
    PlatformController platforms;
    BeamController beams;

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
            //gates.applyPrefs(prefs, id);
            x.complete(&prefs);
        }        
    }

    void update(simData::DataStore* dataStore, const std::vector<simData::ObjectId>& ids)
    {
        for (auto& id : ids)
        {
            auto type = dataStore->objectType(id);

            if (type & simData::PLATFORM)
            {
                auto platformSlice = dataStore->platformUpdateSlice(id);
                if (platformSlice && platformSlice->current())
                {
                    platforms.applyUpdate(platformSlice->current(), id, sim);
                }
            }
            else if (type & simData::BEAM)
            {
                auto beamSlice = dataStore->beamUpdateSlice(id);
                if (beamSlice && beamSlice->current())
                {
                    beams.applyUpdate(beamSlice->current(), id, sim);
                }
            }
        }
    }
};

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
    simData::MemoryDataStore dataStore;

    // The Controller creates and updates visualization objects from the data store stream
    auto adapter = std::make_shared<DataStoreAdapter>(app);

    // Connect the controller to the data store.
    dataStore.addListener(adapter);

    // Create and configure the sim objects:
    simData::ObjectId platformId = addPlatform(dataStore);
    simData::ObjectId beamId = addBeam(platformId, dataStore);
    simData::ObjectId gateId = addGate(beamId, dataStore);

    // Build a simulation
    auto LLA = vsg::dvec3{ 2.0, 35.0, 10000.0 };
    auto geo_to_ecef = rocky::SRS::WGS84.to(rocky::SRS::ECEF);
    simCore::Vec3 ecef;
    for (double time = 0.0; time <= 60.0; time += 0.01)
    {
        LLA.x += 0.001;
        geo_to_ecef.transform(LLA, ecef);

        simData::DataStore::Transaction x;
        auto platform_update = dataStore.addPlatformUpdate(platformId, &x);
        platform_update->set_time(time);
        platform_update->setPosition(ecef);
        x.complete(&platform_update);

        auto beam_update = dataStore.addBeamUpdate(beamId, &x);
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
        dataStore.update(time);
        adapter->update(&dataStore, { platformId, beamId });
    }

    return 0;
}
