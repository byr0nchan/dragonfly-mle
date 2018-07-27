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

local hash_id = "total_bytes_rank"

function setup()
    conn = hiredis.connect()
    starttime = 0 --mle.epoch()
    print (">>>>>>>>> Sum Bytes analyzer")
end


-- ----------------------------------------------
--
-- ----------------------------------------------
function loop(msg)
    local eve = cjson.decode(msg)
    if eve and eve.event_type == 'flow' and (eve.proto=='TCP' or eve.proto=='UDP') then
        
        key = hash_id .. ":" ..eve.dest_ip

        dest_bytes = conn:command("ZINCRBY", hash_id ..":dest", eve.flow.bytes_toclient, eve.dest_ip)
        src_bytes = conn:command("ZINCRBY", hash_id ..":src", eve.flow.bytes_toserver, eve.src_ip)

        dest_rank = conn:command("ZRANK", hash_id ..":dest", eve.dest_ip )
        src_rank = conn:command("ZRANK", hash_id ..":src", eve.src_ip )

        analytics = eve.analytics
        if not analytics then
            analytics = {}
        end
        bytes_rank = {}
        bytes_rank["since"] = starttime
        bytes_rank["src_rank"] = src_rank
        bytes_rank["src_total"] = src_bytes
        bytes_rank["dest_rank"] = dest_rank
        bytes_rank["dest_total"] = dest_bytes
        bytes_rank["source"] = "ranked-sum.lua"

        analytics["bytes_rank"] = bytes_rank
        eve["analytics"] = analytics

        dragonfly.output_event("log", cjson.encode(eve))
    else 
		dragonfly.output_event("log", msg)
	end
end