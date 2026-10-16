-- Replaces the body of a code block that has an `include` attribute with the named file.
-- The path is relative to the directory where pandoc runs. Usage:
--   ```{.cpp include="examples/widgets/button.cpp"}
--   ```

function CodeBlock(block)
  local path = block.attributes["include"]
  if path == nil then
    return nil
  end
  local file = io.open(path, "r")
  if file == nil then
    error("include.lua cannot open " .. path)
  end
  local text = file:read("*a")
  file:close()
  block.text = text:gsub("\n$", "")
  block.attributes["include"] = nil
  return block
end
