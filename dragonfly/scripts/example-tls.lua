
-- ----------------------------------------------
-- TLS look up -- https://notary.icsi.berkeley.edu/
-- ----------------------------------------------

local socket = require("socket")

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
		else
			print (">>>>TLS analyzer running")
		end
end

-- ----------------------------------------------
--
-- ----------------------------------------------
function loop(msg)
	local eve = cjson.decode(msg)
	if eve.tls.fingerprint then
		local sha1 = string.gsub(eve.tls.fingerprint, ":", "")
		if sha1 then
		    -- print ("TLS fingerprint: "..eve.tls.fingerprint)
			key = "tls:"..sha1
			conn:command ("hmset", key, "issuerdn",eve.tls.issuerdn,"subject",eve.tls.subject,"version", eve.tls.version)
			conn:command ("expire", key,'300')
			conn:command ("zincrby", "tls",1,sha1)
			reply =  conn:command ("sismember","tls:valid",sha1) 
			if reply then
				local a_record = sha1..".notary.icsi.berkeley.edu"
				---print(">> TLS CHECKING: "..a_record)
				local ip, status = socket.dns.toip(a_record) 
				if not ip then
					
					message = "time: "..eve.timestamp..", message: TLS lookup error - "..status
					-- print (message)
					output_event ("log", message)
				else
					-- print(">> TLS RESP: "..ip)
					if ip == "127.0.0.2" or ip == "127.0.0.1" then
						conn:command("sadd","tls:valid",sha1) 
						conn:command ("expire", key,'300')
					else
						message = "time:"..eve.timestamp..", dest ip:"..eve.dest_ip.."fingerprint:"..sha1..", issuer: "..eve.tls.issuerdn..", subject:"..eve.tls.subject.."sni:"..eve.tls.sni
						output_event ("tls", message)
					end
				end
			end
		end
	end
end
