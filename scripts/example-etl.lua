function setup()
    
end

function loop(msg)
   local eve = cjson.decode(msg)
   if eve then
       analyze_event (eve.event_type, msg)
   end
end
