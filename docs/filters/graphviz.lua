-- Renders ```dot code blocks as images with Graphviz.
-- Images go to the directory in the BASALT_DIAGRAM_DIR environment variable.
-- HTML output uses SVG. Other formats use PNG.

local out_dir = os.getenv("BASALT_DIAGRAM_DIR") or ".build/docs/diagrams"
local extension = FORMAT:match("html") and "svg" or "png"

function CodeBlock(block)
  if not block.classes:includes("dot") then
    return nil
  end
  local name = pandoc.sha1(block.text)
  local source = out_dir .. "/" .. name .. ".dot"
  local image = out_dir .. "/" .. name .. "." .. extension
  os.execute("mkdir -p '" .. out_dir .. "'")
  local file = assert(io.open(source, "w"))
  file:write(block.text)
  file:close()
  local ok = os.execute(string.format("dot -T%s -Gdpi=144 '%s' -o '%s'", extension, source, image))
  if not ok then
    error("dot failed for a diagram; is Graphviz installed?")
  end
  return pandoc.Para({ pandoc.Image(block.attributes["caption"] or "", image) })
end
