-- ----------------------------------------------
--
-- ----------------------------------------------

local dt = require('dt') -- contains data transformation functions

-- ----------------------------------------------
--
-- ----------------------------------------------
function score(input, model)
	return classify(transform(input), model.weights, model.num_features)
end

-- ----------------------------------------------
--
-- ----------------------------------------------
function setup()
	conn = hiredis.connect()
	assert(conn:command("PING") == hiredis.status.PONG)
	reply = conn:command ("ML.LINREG.SET","dga","10.036346631557","-5.4223523643131","0.096433755746056","-1.0770262606749",
				"-4.2319891790352","-4.3908892593232","-13.974460365592","-5.055694908585")
    print (">>>> Loaded ML model: ", reply)
end

-- ----------------------------------------------
--
-- ----------------------------------------------
function loop(msg)
	local eve = cjson.decode(msg)
	if eve and eve.dns.type == 'answer' and eve.dns.rrtype == 'A' and eve.dns.rdata and eve.dns.rrname then

		-- ----------------------------------------------
		-- DNS analysis
		-- ----------------------------------------------
		if eve.dns.rrtype == 'A' then
			local features = transform (eve.dns.rrname)
			reply = conn:command("ML.LINREG.PREDICT", "dga",unpack(features))
		    --print ("predict: ",reply)
			if reply.type ~= hiredis.REPLY_ERROR then 
				--print("SCORE: ", reply, eve.dns.rrname)
				message = "matched negative dns: "..eve.dns.rrname
				output_event ("alert", message)
				--ip_reputation (eve.timestamp, "blacklist", eve.dns.rdata, 300)
			end
		end
	end
end

