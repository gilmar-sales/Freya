# Containers (internal)

`SparseSet` and `MeshSet` live under `src/Freya/Containers/` and are **not**
part of the public Freya API. They are used by pool implementations.

Do not `#include "Freya/Containers/..."` from application code. Install
packages only ship `include/Freya/`.

Unit tests reach them via the private `src/` include path used by
`FreyaTests`.
