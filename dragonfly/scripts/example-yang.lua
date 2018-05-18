-- ----------------------------------------------
-- 
-- ----------------------------------------------

local counter = 0
local timelast = 0 
local timemark = 0

-- ----------------------------------------------
--
-- ----------------------------------------------
function setup()
    conn = hiredis.connect()
    assert(conn:command("PING") == hiredis.status.PONG)
end

-- ----------------------------------------------
--
-- ----------------------------------------------
function loop(msg)
    log_event (os.date("%c"),"yang", tostring(counter))
    if counter % 1000000 == 0 then
        local timemark = os.clock()
        if timelast > 0 then
            delta_sec = os.difftime(timemark, timelast) 
            print("yang: ", (counter/delta_sec), " ops/sec")
            counter = 0
        end
        timelast = timemark
    end
    counter = counter + 1
end

