local input = "src/lpp/metaenv.lua"
local output = "src/generated/metaenv.h"

local buffer = require "string.buffer"

local f = io.open(input, "r")

if not f then
    error("failed to open "..input)
end

local metaenv_str = f:read("*all")
local metaenv_len = #metaenv_str

f:close()

local header = buffer.new()
 
header:put [[
static const char metaenv_bytes[] =
{]]
header:put "\t"

for i=1,metaenv_len do
    if (i - 1) % 16 == 0 then
        header:put "\n\t"
    end
    header:putf("0x%02x,", metaenv_str:byte(i))
end

header:put([[

};
static const int metaenv_byte_count = ]], metaenv_len, ";")

f = io.open(output, "w")

if not f then
    error("failed to open "..output)
end

f:write(header:tostring())
f:close()


