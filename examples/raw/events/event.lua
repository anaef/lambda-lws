local body = request.body:read("*a")

-- Log event body confirm field
lws.log("info", "event confirm: " .. tostring(request.raw.body.confirm))

-- ... do some processing ...

-- Send a confirmation if "confirm" is found in the body
if request.raw.body.confirm then
	response.body:write("{\"status\": \"confirmed\"}")
else
	-- Default null response
end
