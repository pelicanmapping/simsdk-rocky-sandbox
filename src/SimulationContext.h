#pragma once
#include <simData/ObjectId.h>
#include <simData/DataStore.h>

#include <rocky/Image.h>
#include <rocky/vsg/ecs.h>
#include <rocky/vsg/InstanceVSG.h>
#include <rocky/Log.h>
#include <entt/entt.hpp>

#include <vsg/io/read.h>
#include <vsg/text/Font.h>
#include <vsgXchange/all.h>

#include <unordered_map>

namespace
{
    // Creates an image to use when the image we want is missing.
    std::shared_ptr<rocky::Image> createMissingImage()
    {
        auto image = rocky::Image::create(rocky::Image::R8G8B8A8_UNORM, 15, 15);
        image->fill({ 1,1,1,1 });
        for (unsigned i = 1; i < image->width() - 1; ++i) {
            image->write({ 1,0,0,1 }, i, i);
            image->write({ 1,0,0,1 }, i, image->height() - 1 - i);
        }
        return image;
    }
}

#define VALUE_CHANGED(NAME, P0, P1) \
    ((P0).has_##NAME() && (!(P1).has_##NAME() || ((P0).##NAME() != (P1).##NAME())))


using ObjectToEntityLUT = std::unordered_map<simData::ObjectId, entt::entity>;


struct SimulationContext
{
    //! global rocky instance
    rocky::InstanceVSG& instance;

    //! mapping table from simData::ObjectId to entt::entity
    ObjectToEntityLUT entities;

    //! cache of fonts by name
    std::unordered_map<std::string, vsg::ref_ptr<vsg::Font>> fonts;

    //! simvis-specific logger
    std::shared_ptr<spdlog::logger> log = rocky::Log()->clone("simvis");

    //! gets or loads a font by name
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


//! Base class for simulation entity Adapters - an Adapter applies data model changes
//! to entities in the visualization system.
class SimEntityAdapter
{
public:
    void applyCommonPrefs(const simData::CommonPrefs* new_prefs, const simData::CommonPrefs& old_prefs, entt::entity entity, SimulationContext& sim, entt::registry& registry)
    {
        if (VALUE_CHANGED(name, *new_prefs, old_prefs))
        {
            auto& label = registry.get_or_emplace<rocky::Label>(entity);
            label.text = new_prefs->name();
            if (!label.style.font) label.style.font = sim.instance.runtime().defaultFont;
            label.dirty();
        }

        if (new_prefs->has_labelprefs())
        {
            applyLabelPrefs(&new_prefs->labelprefs(), old_prefs.labelprefs(), entity, sim, registry);
        }
    }

    void applyLabelPrefs(const simData::LabelPrefs* new_prefs, const simData::LabelPrefs& old_prefs, entt::entity entity, SimulationContext& sim, entt::registry& registry)
    {
        if (VALUE_CHANGED(overlayfontname, *new_prefs, old_prefs))
        {
            auto& font = sim.get_font(new_prefs->overlayfontname());
            if (font)
            {
                auto& label = registry.get_or_emplace<rocky::Label>(entity);
                label.style.font = font;
                label.dirty();
            }
        }

        if (VALUE_CHANGED(overlayfontpointsize , *new_prefs, old_prefs))
        {
            auto& label = registry.get_or_emplace<rocky::Label>(entity);
            label.style.pointSize = new_prefs->overlayfontpointsize();
            label.dirty();
        }

        if (VALUE_CHANGED(alignment, *new_prefs, old_prefs))
        {
            auto& label = registry.get_or_emplace<rocky::Label>(entity);
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
