-- The build joins every chapter into one page. This filter turns links to other chapter files
-- into links to anchors in that page. A link such as `widgets/button.md` becomes `#button`, the
-- identifier of the chapter heading. A link such as `architecture.md#frame-cycle` becomes
-- `#frame-cycle`.

function Link(link)
  local file, fragment = link.target:match("^([%w_%-%./]+)%.md(#?.*)$")
  if file == nil then
    return nil
  end
  if fragment ~= "" then
    link.target = fragment
  else
    link.target = "#" .. file:match("([^/]+)$"):gsub("_", "-")
  end
  return link
end
