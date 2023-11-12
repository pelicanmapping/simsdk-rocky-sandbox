#pragma once
#include "SimulationContext.h"
#include <rocky/vsg/Line.h>


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
    const Beam* create(const simData::BeamProperties* props, SimulationContext& sim)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(props, nullptr);
        ROCKY_SOFT_ASSERT_AND_RETURN(props->has_id(), nullptr);

        auto entt_id = sim.entities[props->id()] = sim.registry.create();
        auto& beam = sim.registry.emplace<Beam>(entt_id);
        // give it an empty transform so hosted objects can find it:
        sim.registry.emplace<rocky::Transform>(entt_id);

        applyProps(props, sim);
        return &beam;
    }

    void applyProps(const simData::BeamProperties* new_props, SimulationContext& sim)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(new_props, void());

        auto entt_id = sim.entities[new_props->id()];
        auto& beam = sim.registry.get<Beam>(entt_id);

        if (VALUE_CHANGED(hostid, *new_props, beam.props))
        {
            ROCKY_SOFT_ASSERT_AND_RETURN(sim.entities.count(new_props->hostid()) > 0, void());

            auto host_entt_id = sim.entities[new_props->hostid()];
            auto& transform = sim.registry.get<rocky::Transform>(entt_id);
            transform.parent = sim.registry.try_get<rocky::Transform>(host_entt_id);
            ROCKY_SOFT_ASSERT(transform.parent != nullptr);
        }

        beam.props = *new_props;
    }

    void applyPrefs(const simData::BeamPrefs* new_prefs, const simData::ObjectId beam_id, SimulationContext& sim)
    {
        ROCKY_SOFT_ASSERT_AND_RETURN(new_prefs, void());

        auto entt_id = sim.entities[beam_id];
        auto& beam = sim.registry.get<Beam>(entt_id);

        // detect changes and apply new prefs.
        auto& line = sim.registry.emplace_or_replace<rocky::Line>(entt_id);
        makeBeamGeometry(line, *new_prefs, beam.update);

        if (new_prefs->has_commonprefs())
        {
            applyCommonPrefs(&new_prefs->commonprefs(), beam.prefs.commonprefs(), entt_id, sim);
        }

        beam.prefs = *new_prefs;
    }

    void applyUpdate(const simData::BeamUpdate* new_update, const simData::ObjectId beam_id, SimulationContext& sim)
    {
        auto entt_id = sim.entities[beam_id];
        auto& beam = sim.registry.get<Beam>(entt_id);

        if (VALUE_CHANGED(azimuth, *new_update, beam.update))
        {
            auto& transform = sim.registry.get<rocky::Transform>(entt_id);
            vsg::dquat rot;
            rocky::euler_radians_to_quaternion(beam.update.elevation(), 0.0, new_update->azimuth(), rot);
            transform.local_matrix = vsg::rotate(rot);
        }

        if (VALUE_CHANGED(elevation , *new_update, beam.update))
        {
            auto& transform = sim.registry.get<rocky::Transform>(entt_id);
            vsg::dquat rot;
            rocky::euler_radians_to_quaternion(new_update->elevation(), 0.0, beam.update.azimuth(), rot);
            transform.local_matrix = vsg::rotate(rot);
        }

        if (VALUE_CHANGED(range, *new_update, beam.update))
        {
            auto& line = sim.registry.emplace_or_replace<rocky::Line>(entt_id);
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
