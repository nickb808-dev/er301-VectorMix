-- toc.lua — VectorMix package manifest
-- Read by the ER-301 firmware when the package is installed.
--
-- The `version` field is non-standard (firmware ignores unknown keys).
-- It is used by er301-tool to name the .pkg file: vectormix-0.2.0.pkg.

return {
  name    = "vectormix",       -- package ID; er301-tool looks for lib/am335x/libvectormix.so
  version = "0.3.0",
  title   = "Vector Mix",
  units   = {
    {
      title      = "Vector Mix",
      moduleName = "VectorMix",  -- → assets/VectorMix.lua
      category   = "Mixing",
    },
  },
}
