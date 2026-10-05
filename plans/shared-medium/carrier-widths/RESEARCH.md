# Research: carrier-widths

## How do games keep agents in place when the walkable graph is rebuilt under them?

Recast and Detour rebuild navmesh tiles when the world changes. Each crowd agent keeps a path corridor of polygon references. Each update, the crowd checks whether the agent's current polygon is still valid. If it isn't, the crowd searches for the nearest valid polygon to the agent's ground position and puts the agent there. The agent then replans its path. The agent's identity in the old mesh doesn't matter. Only its ground position does.

This matches what the good-paths slice needs from carryOver. A place names a carrier and a distance, and an edit can re-key the carrier while leaving the ground where it was. If carryOver finds the new carrier by the old ground position, the place survives a split or a merge. Today's rule moves a place to the nearest point on the same carrier. That keeps working when the carrier only changed shape. It fails on a split, because the split-off half has a new key.

Re-finding by position needs a small tolerance. A split segment's new ground line is computed again from its points, so a position on it may differ from the old one by rounding. The feature plan sets the tolerance. It must be far smaller than any spacing a player can draw.

Rejected: keying carriers by something that survives splits, such as the original segment's key for every piece. Two carriers would then share a key, and the network refuses repeated keys. Telling holders which carriers were split or merged. That needs an edit log beside the networks, which the medium doesn't have, and holders would have to read how networks were derived (principle 6).

Sources: https://github.com/recastnavigation/recastnavigation/blob/main/DetourCrowd/Source/DetourCrowd.cpp — checkPathValidity re-finds an agent's polygon by position after the mesh changes; https://github.com/recastnavigation/recastnavigation/blob/main/DetourCrowd/Source/DetourPathCorridor.cpp — the corridor keeps polygon references and is fixed up from the agent's position.
