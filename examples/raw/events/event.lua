local body = request.body:read("*a")

-- Log event body
lws.log("info", "event body: " .. body)

-- ... do some processing ...

-- Send a confirmation if "confirm" is found in the body
local confirm = string.find(body, "confirm")
if confirm then
	response.body:write("{\"status\": \"confirmed\"}")
else
	-- Default null response
end
