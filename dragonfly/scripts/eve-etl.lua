-- ----------------------------------------------
-- Example input processor for EVE log
-- ----------------------------------------------


-- ----------------------------------------------
--
-- ----------------------------------------------
function setup()
	print ("EVE ETL running")
end

-- ----------------------------------------------
--
-- ----------------------------------------------
function loop(msg)
    local eve = cjson.decode(msg)
    if eve then
        -- print ("EVE type: "..eve.event_type)
        analyze_event (eve.event_type, msg)
    end
end

