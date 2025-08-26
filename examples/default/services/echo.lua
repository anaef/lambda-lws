local tz = require("tz")

-- Read request body
local body = request.body:read("*a")

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
for k, v in pairs(request.headers) do
	response.body:write("\t", k, ": ", v, "\n")
end
response.body:write("Body: ", body, "\n")

-- Set status and headers
response.status = lws.status.OK
response.headers["Content-Type"] = "text/plain"
response.headers["Set-Cookie"] = "cookie3=value3, cookie4=value4"
