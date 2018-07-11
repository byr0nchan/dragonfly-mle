-- ----------------------------------------------
-- Example input processor for EVE log
-- ----------------------------------------------


-- ----------------------------------------------
--
-- ----------------------------------------------
function setup()
	print ("Suricata filter running")
end

-- ----------------------------------------------
--
-- ----------------------------------------------
function loop(msg)
    local eve = cjson.decode(msg)
    if eve then
        if (eve.event_type=="alert") then
            analyze_event ("alert", msg)
        else    
            analyzer_event("nsm", msg)
        end
    end
end

