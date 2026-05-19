# ADR 0002: Simplified EPA — faces removed, edges not tracked

EPA implementation removes faces visible from the new support point without rebuilding edges. The polytope may shrink rather than expand, potentially underestimating penetration depth on edge-edge and vertex-face contacts. A guard clause discards depths below 0.001 to avoid applying wrong corrections.

**Why not full EPA:** Proper edge tracking requires maintaining edge-face adjacency, rebuilding faces around removed regions, and tessellating the hull — roughly 300+ lines of geometry code. Box2D/LiquidFun serve as production references for the full algorithm. For a toy engine targeting spheres and axis-aligned boxes, the simplified version suffices.

**Consequence:** Some edge-edge contacts may be missed or minimally corrected. This is acceptable for the engine's target scenarios (falling spheres, box stacks, inclined planes). A future reader seeing occasional missed contacts should check whether EPA ran out of iterations or faces first.
