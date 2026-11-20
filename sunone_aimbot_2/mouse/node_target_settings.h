#pragma once

#include "mouse/AimbotTarget.h"
#include "nodes/node_graph.h"

inline TargetSelectionSettings nodeTargetSettings(const NodeGraph::Node& node)
{
    TargetSelectionSettings settings;
    settings.classPlayer = static_cast<int>(NodeGraph::parameter(node, "class_player"));
    settings.classHead = static_cast<int>(NodeGraph::parameter(node, "class_head"));
    settings.applyOffsets = NodeGraph::parameter(node, "legacy_offsets") != 0;
    settings.bodyOffset = NodeGraph::parameter(node, "body_y_offset");
    settings.headOffset = NodeGraph::parameter(node, "head_y_offset");
    settings.filterClasses = NodeGraph::parameter(node, "filter_classes") != 0;
    settings.preferHead = NodeGraph::parameter(node, "prefer_head") != 0;
    settings.mergeHeadBody = NodeGraph::parameter(node, "merge_head_body") != 0;
    settings.maxMissedFrames = static_cast<int>(NodeGraph::parameter(node, "max_missed_frames"));
    return settings;
}
