#pragma once
#include "SimulationContext.h"
#include <rocky/vsg/ecs.h>

struct Platform
{
    simData::PlatformProperties props;
    simData::PlatformPrefs prefs;
    simData::PlatformUpdate update;
};


//! Applies DataStore platform messages to the corresponding visualization components.
class PlatformAdapter : public SimEntityAdapter
{
public:
    const Platform* create(const simData::PlatformProperties* props, SimulationContext& sim, entt::registry& registry)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(props, nullptr);
        ROCKY_SOFT_ASSERT_AND_RETURN(props->has_id(), nullptr);

        sim.log->info("Add platform with id=" + std::to_string(props->id()));
        auto entt_id = sim.entities[props->id()] = registry.create();
        auto& platform = registry.emplace<Platform>(entt_id);
        registry.emplace<rocky::Transform>(entt_id);
        applyProps(props, sim, registry);
        return &platform;
    }

    void applyProps(const simData::PlatformProperties* new_props, SimulationContext& sim, entt::registry& registry)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(new_props, void());
        ROCKY_SOFT_ASSERT_AND_RETURN(new_props->has_id(), void());

        auto entt_id = sim.entities[new_props->id()];
        auto& platform = registry.emplace<Platform>(entt_id);

        platform.props = *new_props;
    }

    void applyPrefs(const simData::PlatformPrefs* new_prefs, const simData::ObjectId id, SimulationContext& sim, entt::registry& registry)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(new_prefs != nullptr, void());

        auto entt_id = sim.entities[id];
        auto& platform = registry.get<Platform>(entt_id);

        if (VALUE_CHANGED(icon, *new_prefs, platform.prefs))
        {
            auto& icon = registry.get_or_emplace<rocky::Icon>(entt_id);

            auto io = sim.runtime->io;
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
            icon.dirty();
        }

        if (VALUE_CHANGED(scale, *new_prefs, platform.prefs))
        {
            auto& icon = registry.get_or_emplace<rocky::Icon>(entt_id);
            if (icon.image && icon.image->valid())
            {
                icon.style.size_pixels = icon.image->width() * new_prefs->scale();
            }
            platform.prefs.set_scale(new_prefs->scale());
        }

        if (new_prefs->has_commonprefs())
        {
            applyCommonPrefs(&new_prefs->commonprefs(), platform.prefs.commonprefs(), entt_id, sim, registry);
        }

        platform.prefs = *new_prefs;
    }

    void applyUpdate(const simData::PlatformUpdate* new_update, const simData::ObjectId id, SimulationContext& sim, entt::registry& registry)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(new_update, void());

        auto entt_id = sim.entities[id];
        auto& platform = registry.get<Platform>(entt_id);

        if (new_update->has_x() || new_update->has_y() || new_update->has_z())
        {
            auto& transform = registry.get<rocky::Transform>(entt_id);
            transform.position = rocky::GeoPoint(rocky::SRS::ECEF, new_update->x(), new_update->y(), new_update->z());
            transform.dirty();
        }

        platform.update = *new_update;
    }
};
