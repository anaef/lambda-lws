local tz = require("tz")

-- Read request body
local body = request.body:read("*a")

-- Compatibility with Lua 5.1
local lwspairs, lwsipairs
if _VERSION > "Lua 5.1" then
	lwspairs, lwsipairs = pairs, ipairs
else
	lwspairs, lwsipairs = lws.pairs, lws.ipairs
end

-- Prepare the response body
response.body:write("Time: ", tz.date(), "\n")
response.body:write("IP: ", request.ip, "\n")
response.body:write("Method: ", request.method, "\n")
response.body:write("Path: ", request.path, "\n")
response.body:write("Path info: ", request.path_info, "\n")
response.body:write("Query args:\n")
local args = lws.parseargs(request.args)
for k, v in pairs(args) do
	response.body:write("\t", k, ": ", v, "\n")
end
response.body:write("Headers:\n")
for k, v in lwspairs(request.headers) do
	response.body:write("\t", k, ": ", v, "\n")
end
response.body:write("Body: ", body, "\n")
response.body:write("Raw headers:\n")
for k, v in lwspairs(request.raw.headers) do
	response.body:write("\t", k, ": ", v, "\n")
end
response.body:write("Raw body:\n")
for k, v in lwspairs(request.raw.body) do
	response.body:write("\t", k, ": ", tostring(v), "\n")
end
response.body:write("Raw cookies:\n")
for i, c in lwsipairs(request.raw.body.cookies or { }) do
	response.body:write("\t", c, "\n")
end

-- Set status and headers
response.status = lws.status.OK
response.headers["Content-Type"] = "text/plain"
response.headers["Set-Cookie"] = "cookie3=value3, cookie4=value4"
