local posix = require("posix")

-- Simulate a streaming response
response.status = lws.status.OK
response.headers["Content-Type"] = "text/plain"
for i = 3, 0, -1 do
	response.body:write(tostring(i), "\n")
	response.body:flush()
	posix.sleep(1)
end
