-- ----------------------------------------------
-- Flow analysis using Random Forest ML
-- ----------------------------------------------
--
local open = io.open
local mlpath = "analyzers/flow-suricata.ml"
local mlmodule = "./lib/redis-ml.so"
-- ----------------------------------------------
--
-- ----------------------------------------------
function setup()
    conn = hiredis.connect()
    assert(conn:command("PING") == hiredis.status.PONG)
    
    -- load the redis ML module
    print ("Load Redis ML module......")
    reply = conn:command_line("MODULE LOAD "..mlmodule)

    print ("Reading random forest model......")
    local file = open(mlpath, "rb") -- r read mode 
    local model = file:read ("*a")
    file:close()

    -- load the random forest tree
    reply = conn:command_line("ML.FOREST.ADD "..model)
    print ("Loaded ML model: ", reply)

end


-- ----------------------------------------------
--
-- ----------------------------------------------
function loop(msg)
    command = {}
    local eve = cjson.decode(msg)
    if eve and eve.event_type == 'flow' and (eve.proto=='TCP' or eve.proto=='UDP') then
       -- print (msg)

        -- extract the fields
        total_pkts = eve.flow.pkts_toclient + eve.flow.pkts_toserver
        total_bytes = eve.flow.bytes_toclient + eve.flow.bytes_toserver
        duration = eve.flow.age
        src_port = eve.src_port
        dest_port = eve.dest_port

        -- generate the ML redis command
        table.insert(command, "ML.FOREST.RUN")
        table.insert(command, "5-raw-plus-num-v17:tree")

        table.insert(command, "tot_pkts:"..total_pkts)
        table.insert(command, "tot_bytes:"..total_bytes)
        table.insert(command, "duration:"..duration)
        table.insert(command, "src_port_num:"..src_port)
        table.insert(command, "dest_port_num:"..dest_port)
        table.insert(command, "CLASSIFICATION")

        local ml_command = table.concat(command, " ")
        reply = conn:command_line(ml_command)
	    --print (reply)	    
    end
end

