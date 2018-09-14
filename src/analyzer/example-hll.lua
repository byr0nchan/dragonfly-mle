-- ----------------------------------------------
-- Copyright 2018, CounterFlow AI, Inc. 
-- 
-- Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following 
-- conditions are met:
--
-- 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
-- 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer 
--    in the documentation and/or other materials provided with the distribution.
-- 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products 
--    derived from this software without specific prior written permission.
--
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, 
-- BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
-- SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
-- DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
-- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
-- OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
--
-- author: Andrew Fast <af@counterflowai.com>
-- ----------------------------------------------

-- ----------------------------------------------
-- example-hll.lua
-- Tracks number of distinct sources for each dest_ip using a HyperLogLog "sketch" data structure.
-- HyperLogLog is included in the base functionality of Redis. It is a set-like data structure
-- with constant memory and insert time, but that returns an approximate result with rate
-- tied to the size of the hash being used.
-- ----------------------------------------------


local hash_id = "distinct_src_ip_counter"

-- ----------------------------------------------
--
-- ----------------------------------------------
function setup()
    conn = hiredis.connect()
    assert(conn:command("PING") == hiredis.status.PONG)
    starttime = 0 --mle.epoch()
    print (">>>>>>>>> Distinct IP analyzer")
end


-- ----------------------------------------------
--
-- ----------------------------------------------
function loop(msg)
    local eve = cjson.decode(msg)
    if eve and eve.event_type == 'flow' and (eve.proto=='TCP' or eve.proto=='UDP') then
        
        key = hash_id .. ":" ..eve.dest_ip
        reply = conn:command("PFADD", key, eve.src_ip) -- PFADD returns 1 if at least one internal register has altered.
        count = conn:command("PFCOUNT", key)

        -- Create table to hold results of analysis, if it doesn't already exist
        analytics = eve.analytics
        if not analytics then
            analytics = {}
        end
        unique_src = {}
        unique_src["since"] = starttime
        unique_src["count"] = count
        unique_src["source"] = "example-hll.lua"

        analytics["unique_src"] = unique_src
        eve["analytics"] = analytics

        dragonfly.output_event("log", cjson.encode(eve))
    else 
		dragonfly.output_event("log", msg)
	end
end