-- ----------------------------------------------
--
-- ----------------------------------------------
filename = "baddomains.txt"
file_url = "https://zeustracker.abuse.ch/blocklist.php?download=baddomains"
redis_key = "bad.domain"
-- ----------------------------------------------
--
-- ----------------------------------------------
function setup()
	conn = hiredis.connect()
	if not conn then
		error ("Error connecting to the redis server")
	end
	if conn:command("PING") ~= hiredis.status.PONG then
		error ("Unable to ping redis")		
	end
	http_get (file_url, filename)
	local file, err = io.open(filename, 'rb')
	if file then
		conn:command("DEL",redis_key)
		while true do
			line = file:read()
			if line ==nil then
				break
			elseif line ~='' and not line:find("^#") then
				if (conn:command("SADD",redis_key,line)==1) then
					print (line)
				end
			end
		end
	end
	print (">>>> Bad DNS analyzer running")
end

-- ----------------------------------------------
--
-- ----------------------------------------------
function loop(msg)
	local eve = cjson.decode(msg)
	if eve and eve.dns.type == 'answer' and eve.dns.rrtype == 'A' and eve.dns.rrname then
		if conn:command("SISMEMBER",redis_key,eve.dns.rrname) == 1 then
			message = "rrname: "..eve.dns.rrname..", rdata: "..eve.dns.rdata
			-- print ("dns-alert: "..message)
			output_event ("dns", message)
		end
	end
end

