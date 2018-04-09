local socket = require("socket")

-- ----------------------------------------------
--
-- ----------------------------------------------
function setup()
		conn = hiredis.connect()
		assert(conn:command("PING") == hiredis.status.PONG)
		print ("TLS analyzer running")
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
				print(">> TLS CHECKING: "..a_record)
				local ip, status = socket.dns.toip(a_record) 
				if not ip then
					print(">> TLS LOOKUP ERROR: "..status)
					message = "Questionable TLS domain/IP: "..sha1.."\n\tissuer: "..eve.tls.issuerdn.."\n\tsubject: "..eve.tls.subject
                    log_event (eve.timestamp, "tls", message)
				else
					-- print(">> TLS RESP: "..ip)
					if ip == "127.0.0.2" or ip == "127.0.0.1" then
						conn:command("sadd","tls:valid",sha1) 
						conn:command ("expire", key,'300')
					else
						message = "Invalid TLS: "..sha1.."\n\tissuer: "..eve.tls.issuerdn.."\n\tsubject: "..eve.tls.subject
                        log_event (eve.timestamp, "TLS", message)
					end
				end
			end
		end
	end
end
