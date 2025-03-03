#pragma once
#include "SimulationContext.h"
#include <rocky/vsg/ecs.h>


struct Beam
{
    simData::BeamProperties props;
    simData::BeamPrefs prefs;
    simData::BeamUpdate update;
};


//! Applies DataStore beam messages to the corresponding visualization components.
class BeamAdapter : public SimEntityAdapter
{
public:
    const Beam* create(const simData::BeamProperties* props, SimulationContext& sim, entt::registry& registry)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(props, nullptr);
        ROCKY_SOFT_ASSERT_AND_RETURN(props->has_id(), nullptr);

        auto entt_id = sim.entities[props->id()] = registry.create();
        auto& beam = registry.emplace<Beam>(entt_id);

        // give it an empty transform so hosted objects can find it:
        registry.emplace<rocky::Transform>(entt_id);

        // emplace a Line primitive for drawing the beam's frustum.
        // we will fill in the geometry later.
        auto& line = registry.emplace<rocky::Line>(entt_id);

        applyProps(props, sim, registry);
        return &beam;
    }

    void applyProps(const simData::BeamProperties* new_props, SimulationContext& sim, entt::registry& registry)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(new_props, void());

        auto entt_id = sim.entities[new_props->id()];
        auto& beam = registry.get<Beam>(entt_id);

        if (VALUE_CHANGED(hostid, *new_props, beam.props))
        {
            ROCKY_SOFT_ASSERT_AND_RETURN(sim.entities.count(new_props->hostid()) > 0, void());

            auto host_entt_id = sim.entities[new_props->hostid()];
            auto& transform = registry.get<rocky::Transform>(entt_id);
        }

        beam.props = *new_props;
    }

    void applyPrefs(const simData::BeamPrefs* new_prefs, const simData::ObjectId beam_id, SimulationContext& sim, entt::registry& registry)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(new_prefs, void());

        auto entt_id = sim.entities[beam_id];
        auto& beam = registry.get<Beam>(entt_id);

        // detect changes and apply new prefs.
        auto& line = registry.emplace_or_replace<rocky::Line>(entt_id);
        makeBeamGeometry(line, *new_prefs, beam.update);

        if (new_prefs->has_commonprefs())
        {
            applyCommonPrefs(&new_prefs->commonprefs(), beam.prefs.commonprefs(), entt_id, sim, registry);
        }

        beam.prefs = *new_prefs;
    }

    void applyUpdate(const simData::BeamUpdate* new_update, const simData::ObjectId beam_id, SimulationContext& sim, entt::registry& registry)
    {
        auto entt_id = sim.entities[beam_id];
        auto& beam = registry.get<Beam>(entt_id);

        auto& host = registry.get<Platform>(sim.entities[beam.props.hostid()]);
        auto& host_transform = registry.get<rocky::Transform>(sim.entities[host.props.id()]);

        if (VALUE_CHANGED(azimuth, *new_update, beam.update))
        {
            auto& transform = registry.get<rocky::Transform>(entt_id);
            transform = host_transform;
            auto rot = rocky::quaternion_from_euler_radians(beam.update.elevation(), 0.0, new_update->azimuth());
            transform.localMatrix *= glm::mat4_cast(rot);
        }

        if (VALUE_CHANGED(elevation , *new_update, beam.update))
        {
            auto& transform = registry.get<rocky::Transform>(entt_id);
            transform = host_transform;
            auto rot = rocky::quaternion_from_euler_radians(beam.update.elevation(), 0.0, new_update->azimuth());
            transform.localMatrix *= glm::mat4_cast(rot);
        }

        if (VALUE_CHANGED(range, *new_update, beam.update))
        {
            auto& line = registry.get<rocky::Line>(entt_id);
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

        line.topology = rocky::Line::Topology::Segments;

        line.points = {
            UR, UL, UL, LL, LL, LR, LR, UR,
            UR, origin, UL, origin, LR, origin, LL, origin
        };

        line.style.color = vsg::vec4{ 1, 1, 0, 1 };
        line.style.width = 2.0f;

        line.dirty();
    }
};
