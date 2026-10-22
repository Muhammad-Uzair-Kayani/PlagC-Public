-- Also reject incomplete checkouts when Premake is invoked directly.
local root = path.getabsolute("..")
for entry in io.lines(path.join(root, "Build/DependencyFiles.txt")) do
    entry = entry:gsub("\r", "")
    if entry ~= "" and entry:sub(1, 1) ~= "#" then
        if not os.isfile(path.join(root, entry)) then
            error("Missing dependency file: " .. entry .. ". Run FetchDependencies.bat or bash FetchDependencies.sh.")
        end
    end
end
