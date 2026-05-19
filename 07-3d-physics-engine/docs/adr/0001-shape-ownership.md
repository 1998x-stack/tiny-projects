# ADR 0001: CollisionShape is owned by RigidBody, not World

RigidBody holds `std::unique_ptr<CollisionShape>` directly rather than World owning shapes separately and handing out raw pointers. This avoids dangling pointer bugs (shapes outliving their World-offered raw ptrs) and keeps a 1:1 body-to-shape ownership that matches the domain model — every body has exactly one shape.

**Rejected:** World-owned shape registry (two indirections, lifetime complexity out of proportion for a toy engine). Inline discriminated union (would require GJK to switch on shape type, breaking polymorphic `support()` dispatch).
