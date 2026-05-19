# ADR 0003: Cold-start solver — no warm-starting contact persistence

The spec mentions warm-starting (carrying accumulated impulses across frames), but the implementation creates new `ContactManifold` objects each frame with zeroed impulses. Contact persistence — tracking contacts by feature ID across frames to restore previous impulses — is deferred.

**Why:** Contact persistence requires a hash map from `(body_a, body_b, feature_id)` to accumulated impulse, plus contact matching by proximity when features change. This is production-engine territory (Box2D dedicates ~400 lines to contact tracking). For a toy engine, compensating with higher solver iterations (15 vs 10) achieves adequate convergence for the target scenarios.

**Consequence:** Stacked boxes will settle more slowly and may oscillate slightly before converging. The 5-box stack success criterion (10+ seconds stable) remains achievable with `solver_iterations = 15` and `baumgarte = 0.15`.
