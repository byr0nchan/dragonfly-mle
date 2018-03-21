-- ----------------------------------------------
--
-- ----------------------------------------------

local parquet = require('parquet')
local json = require('json')
local redis = require('redis')
local date = require('date')
local dt = require('dt') -- contains data transformation functions

local listen = {}
local client = {}
local model = {}

-- ----------------------------------------------
--
-- ----------------------------------------------
function initialize()
	client = redis.connect(redis_path)
	score= client:mllogset("dga",10.036346631557,-5.4223523643131,0.096433755746056,-1.0770262606749,
				-4.2319891790352,-4.3908892593232,-13.974460365592,-5.055694908585)
        print ("dns event handler listening to "..redis_path)
end

-- ----------------------------------------------
--
-- ----------------------------------------------
function score(input, model)
	return classify(transform(input), model.weights, model.num_features)
end

-- ----------------------------------------------
--
-- ----------------------------------------------
function event(msg)
	   local eve = json.decode(msg)
	   if eve and eve.dns.type == 'answer' and eve.dns.rrtype == 'A' and eve.dns.rdata and eve.dns.rrname then
	   	print (eve.timestamp)
	   	--d=date(eve.timestamp)
	   	--print (d.epoch())
		local value = nil
		local rrdata = eve.dns.rdata
		local key = "dns:"..eve.flow_id..":"..eve.dns.id
		if not client:exists(key) then
			client:zincrby("dns",1,eve.dns.rrname)
		else
			value = client:hget(key,eve.dns.rrtype)
			if value then
				rrdata = value .." "..eve.dns.rdata
			end
		end
		if not client:hmset(key, eve.dns.rrtype,rrdata,"ttl",eve.dns.ttl) then
			print ("EVE:dns - hmset ERROR!")
		end
		client:expire(key,"60")

		-- ----------------------------------------------
		-- DNS analysis
		-- ----------------------------------------------
		if eve.dns.rrtype == 'A' then
			local features = transform (eve.dns.rrname)
			score= client:mllogpredict("dga",unpack(features))
			if (score > 0.0) then
				print("SCORE: ", score, eve.dns.rrname)
				message = "matched negative dns: "..eve.dns.rrname
				dragonfly_log (eve.timestamp, "reputation", "blacklist", rrdata)
				dragonfly_iprep (eve.timestamp, "blacklist", rrdata, 300)
			end
		end
	end
end

