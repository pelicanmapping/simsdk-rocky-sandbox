#include <rocky_vsg/Application.h>
#include <rocky_vsg/Icon.h>
#include <rocky_vsg/Label.h>
#include <rocky_vsg/Mesh.h>

#include <rocky/TMSImageLayer.h>

#include <vsg/io/read.h>

#include <simCore.h>
#include <simData.h>

#include <unordered_map>

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
    for (int i = 1; i < image->width()-1; ++i) {
        image->write({ 1,0,0,1 }, i, i);
        image->write({ 1,0,0,1 }, i, image->height() - 1 - i);
    }
    return image;
}

class PlatformController
{
public:
    rocky::ECS::Registry& reg;

    PlatformController(rocky::ECS::Registry& reg_) : reg(reg_) { }

    void applyProps(const simData::PlatformProperties* props, entt::entity platform)
    {
        //todo
    }

    void applyPrefs(const simData::PlatformPrefs* prefs, entt::entity platform)
    {
        //todo
    }

    void applyUpdate(const simData::PlatformUpdate* update, entt::entity platform)
    {
        if (update)
        {
            auto& transform = reg.get_or_emplace<rocky::Transform>(platform);
            rocky::GeoPoint point(rocky::SRS::ECEF, update->x(), update->y(), update->z());
            transform.setPosition(point);
        }
    }
};

class BeamController
{
public:
    rocky::ECS::Registry& reg;

    BeamController(rocky::ECS::Registry& reg_) : reg(reg_) { }

    void applyProps(const simData::BeamProperties* props, entt::entity beam)
    {
        //nop
    }

    void applyPrefs(const simData::BeamPrefs* prefs, entt::entity beam)
    {
        const float s = 20000.0f;
        auto& mesh = reg.get_or_emplace<rocky::Mesh>(beam);
        vsg::vec3 verts[4] = { { -s, -s, 0 }, {  s, -s, 0 }, {  s,  s, 0 }, { -s,  s, 0 } };
        unsigned indices[6] = { 0,1,2, 0,2,3 };
        vsg::vec4 color{ 1, 1, 0, 0.55 };
        for (unsigned i = 0; i < 6; ) {
            mesh.add({
                {verts[indices[i++]], verts[indices[i++]], verts[indices[i++]]},
                {color, color, color} });
        }
        mesh.dirty();
    }

    void applyUpdate(const simData::BeamUpdate* update, entt::entity beam, entt::entity host)
    {
        auto host_transform = reg.try_get<rocky::Transform>(host);

        auto& transform = reg.get_or_emplace<rocky::Transform>(beam);
        if (host_transform)
            transform = *host_transform;

        //todo: update the mesh.
        auto& mesh = reg.get<rocky::Mesh>(beam);
        mesh.dirty();
    }
};

class SimObjectController : public simData::DataStore::DefaultListener
{
public:
    rocky::Application& app;
    std::unordered_map<simData::ObjectId, entt::entity> lut;
    std::unordered_map<simData::ObjectId, simData::ObjectId> host;

    SimObjectController(rocky::Application& app_)
        : app(app_)
    {
        //nop
    }

    void onAddEntity(simData::DataStore* ds, simData::ObjectId id, simData::ObjectType type) override
    {
        if (type & simData::PLATFORM)
        {
            rocky::Log::info() << "Add platform with id=" << id << std::endl;

            auto platform = app.entities.create();
            lut[id] = platform;
            auto& icon = app.entities.emplace<rocky::Icon>(platform);
            icon.image = createMissingImage();
            icon.style.size_pixels = 64;
        }

        else if (type & simData::BEAM)
        {
            rocky::Log::info() << "Add beam with id=" << id << std::endl;
            simData::DataStore::Transaction xaction;
            auto beam_props = ds->beamProperties(id, &xaction);
            if (beam_props) {
                rocky::Log::info() << "  host id=" << beam_props->hostid() << std::endl;
                host[id] = beam_props->hostid();
            }
            
            auto beam = lut[id] = app.entities.create();
            BeamController(app.entities).applyProps(beam_props, beam);

            xaction.complete(&beam_props);
        }

        else if (type & simData::GATE)
        {
            rocky::Log::info() << "Add gate with id=" << id << std::endl;
            simData::DataStore::Transaction xaction;
            auto gate_props = ds->gateProperties(id, &xaction);
            if (gate_props) {
                rocky::Log::info() << "  host id=" << gate_props->hostid() << std::endl;
                host[id] = gate_props->hostid();
            }
            xaction.complete(&gate_props);
        }
    }

    void onPrefsChange(simData::DataStore* ds, simData::ObjectId id) override
    {
        rocky::Log::info() << "onPrefsChange id=" << id << std::endl;

        auto type = ds->objectType(id);

        if (type & simData::PLATFORM)
        {
            if (lut.find(id) == lut.end())
                lut[id] = app.entities.create();

            simData::DataStore::Transaction x;
            auto prefs = ds->platformPrefs(id, &x);
            auto platform = lut[id];

            if (prefs->has_icon())
            {
                auto io = app.instance.ioOptions();
                auto image = io.services.readImageFromURI(prefs->icon(), io);
                if (image.status.ok())
                {
                    auto& icon = app.entities.get_or_emplace<rocky::Icon>(platform);
                    icon.image = image.value;
                    icon.dirtyImage();
                }
                else
                {
                    rocky::Log::warn() << "Icon \"" << prefs->icon() << "\" cannot load: " << image.status.toString() << std::endl;
                }
            }

            if (prefs->has_scale())
            {
                auto& icon = app.entities.get_or_emplace<rocky::Icon>(platform);
                if (icon.image && icon.image->valid())
                {
                    icon.style.size_pixels = icon.image->width() * prefs->scale();
                }
            }

            if (prefs->commonprefs().has_name())
            {
                static vsg::ref_ptr<vsg::Font> font;
                if (!font)
                    font = vsg::read_cast<vsg::Font>("C:/windows/fonts/arialbd.ttf");

                if (font)
                {
                    auto& label = app.entities.get_or_emplace<rocky::Label>(platform);
                    label.font = font;
                    label.text = prefs->commonprefs().name();
                    label.dirty();
                }
                else
                {
                    rocky::Log::warn() << "Failed to load font file, no label for you" << std::endl;
                }
            }

            x.complete(&prefs);
        }

        else if (type & simData::BEAM)
        {
            if (lut.find(id) == lut.end())
                lut[id] = app.entities.create();

            simData::DataStore::Transaction x;
            auto prefs = ds->beamPrefs(id, &x);

            auto beam = lut[id];
            BeamController(app.entities).applyPrefs(prefs, beam);
            x.complete(&prefs);
        }

        else if (type & simData::GATE)
        {
            //todo
        }        
    }

    void onPropertiesChange(simData::DataStore* ds, simData::ObjectId id) override
    {
        rocky::Log::info() << "onPropertiesChange id=" << id << std::endl;
    }

    void update(simData::DataStore* dataStore, const std::vector<simData::ObjectId>& ids)
    {
        for (auto& id : ids)
        {
            auto type = dataStore->objectType(id);

            if (type == simData::PLATFORM)
            {
                auto platformSlice = dataStore->platformUpdateSlice(id);
                if (platformSlice)
                    PlatformController(app.entities).applyUpdate(platformSlice->current(), lut[id]);
            }
            else if (type == simData::BEAM)
            {
                auto beamSlice = dataStore->beamUpdateSlice(id);
                if (beamSlice)
                    BeamController(app.entities).applyUpdate(beamSlice->current(), lut[id], lut[host[id]]);
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
    simData::DataStore::Transaction transaction;
    simData::PlatformProperties* newProps = dataStore.addPlatform(&transaction);
    simData::ObjectId result = newProps->id();
    transaction.complete(&newProps);
    return result;
}

/// create a beam and add it to 'dataStore'
///@return id for new beam
simData::ObjectId
addBeam(simData::ObjectId hostId, simData::DataStore& dataStore)
{
    simData::DataStore::Transaction transaction;

    simData::BeamProperties* beamProps = dataStore.addBeam(&transaction);
    simData::ObjectId result = beamProps->id();
    beamProps->set_hostid(hostId);
    transaction.complete(&beamProps);

    simData::BeamPrefs* beamPrefs = dataStore.mutable_beamPrefs(result, &transaction);
    beamPrefs->set_azimuthoffset(simCore::DEG2RAD * 0.0);
    beamPrefs->set_verticalwidth(simCore::DEG2RAD * 30.0);
    beamPrefs->set_horizontalwidth(simCore::DEG2RAD * 60.0);
    beamPrefs->mutable_commonprefs()->set_color(0x7FFF007F);
    transaction.complete(&beamPrefs);

    return result;
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

/// Sets up default prefs for a platform
void
configurePlatformPrefs(simData::ObjectId platformId, simData::DataStore* dataStore, const std::string& name)
{
    simData::DataStore::Transaction xaction;
    simData::PlatformPrefs* prefs = dataStore->mutable_platformPrefs(platformId, &xaction);

    prefs->mutable_commonprefs()->set_name(name);
    prefs->set_icon(EXAMPLE_AIRPLANE_ICON);
    prefs->set_scale(3.0f);
    prefs->set_dynamicscale(true);
    prefs->set_circlehilightcolor(0xffffffff);

    prefs->mutable_commonprefs()->set_draw(true);
    prefs->mutable_commonprefs()->mutable_labelprefs()->set_draw(true);
    prefs->mutable_commonprefs()->mutable_labelprefs()->set_overlayfontpointsize(14);

    prefs->mutable_commonprefs()->mutable_localgrid()->mutable_speedring()->set_timeformat(simData::ELAPSED_SECONDS);
    prefs->mutable_commonprefs()->mutable_localgrid()->mutable_speedring()->set_radius(2);

    xaction.complete(&prefs);
}


/// connect beam to platform, set some properties
void
configurePrefs(
    simData::ObjectId platformId,
    simData::ObjectId beamId,
    simData::ObjectId gateId,
    simData::DataStore* dataStore)
{
    /// configure the platform
    configurePlatformPrefs(platformId, dataStore, "Simulated Platform");

    /// set up the beam
    {
        simData::DataStore::Transaction xaction;
        simData::BeamPrefs* prefs = dataStore->mutable_beamPrefs(beamId, &xaction);
        prefs->set_beamdrawmode(simData::BeamPrefs::WIRE_ON_SOLID);
        xaction.complete(&prefs);
    }

    /// set up the gate
    {
        simData::DataStore::Transaction xaction;
        simData::GatePrefs* prefs = dataStore->mutable_gatePrefs(gateId, &xaction);
        //TODO: set some gate prefs here
        xaction.complete(&prefs);
    }
}

int
main(int argc, char** argv)
{
    simCore::checkVersionThrow();

    // Application object for the 3D map display.
    rocky::Application app(argc, argv);
    rocky::Log::level = rocky::LogLevel::INFO;
    
    //rocky::Log::info() << app.about() << std::endl;

    // Add some example data to the base map
    rocky::Status s = createDefaultExampleMap(app);
    if (s.failed())
        return error_out(s);

    // The SIM SDK entity stream.
    simData::MemoryDataStore dataStore;

    // Factory object for creating 3D objects from the data store stream.
    auto controller = std::make_shared<SimObjectController>(app);

    // Connect the factory to the data store.
    dataStore.addListener(controller);

    /// create a clock so clock-based features will work
    simCore::Clock* clock = new simCore::ClockImpl;
    clock->setMode(simCore::Clock::MODE_FREEWHEEL, simCore::TimeStamp(1970, simCore::getSystemTime()));

    /// add in the platform and beam
    simData::ObjectId platformId = addPlatform(dataStore);
    simData::ObjectId beamId = addBeam(platformId, dataStore);
    simData::ObjectId gateId = addGate(beamId, dataStore);
    //simData::ObjectId laserId = addLaser(platformId, dataStore);

    /// connect them and add some additional settings
    configurePrefs(platformId, beamId, gateId, &dataStore);

    /// Position the platform:
    {
        auto geo_to_ecef = rocky::SRS::WGS84.to(rocky::SRS::ECEF);
        simCore::Vec3 ecef;
        geo_to_ecef.transform(glm::dvec3{ 2.0, 60.0, 10000.0 }, ecef);

        simData::DataStore::Transaction x;
        auto update = dataStore.addPlatformUpdate(platformId, &x);
        update->set_time(0.0);
        update->setPosition(ecef);
        x.complete(&update);
    }

    /// Update the beam so it shows up:
    {
        simData::DataStore::Transaction x;
        auto update = dataStore.addBeamUpdate(beamId, &x);
        x.complete(&update);
    }
    
    /// update the data store
    dataStore.update(0);

    controller->update(&dataStore, { platformId, beamId });

    return app.run();
}
