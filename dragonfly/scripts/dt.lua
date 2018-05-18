
--[[

Feature Creation is a series of functions to transform the domain string

Functions to create the following features:
	[X] Identify domain parts: TLD (.com or .co.uk etc.), 2LD, 3LD
	[X] Count dots - how many domain parts are there
	[X] Length of domain parts
	[ ] Distinct character count
	[ ] Digit count
	[ ] Percent digits
	[ ] Percent distinct characters
	[ ] Character Entropy


]]

--[[ TODO: Future more dictionary word processing

 - Length of Dictionary Words (2LD, 3LD)
 - Percent of Dictionary Words (2LD, 3LD)
 - Length of longest meaningful substring
 - percent of the length of longest meaningful substring


]]


require 'string'


-- Function from http://lua-users.org/wiki/SplitJoin
-- Simple split on delimeter (does not escape csv)
function csplit(str,sep)
   local ret={}
   local n=1
   for w in str:gmatch("([^"..sep.."]*)") do
      ret[n] = ret[n] or w -- only set once (so the blank after a string is ignored)
      if w=="" then
         n = n + 1
      end -- step forwards on a blank but not a string
   end
   result = {parts=ret, num = n - 1}
   return result
end

--Function to produce a table with parts of the domain
function parse(splitdomain, numparts)
	tld = ""
	second = ""
	third = ""
	last = ""

	if numparts >= 2 and not(numparts > 2 and splitdomain[numparts] == "arpa" and splitdomain[numparts-1] == "in-addr") then
		-- If length of last spot is 2, and length of second to last spot is < 3, then assume a compound tld
		tldlength = 1
		if numparts > 2 and string.len(splitdomain[numparts]) == 2 and string.len(splitdomain[numparts-1]) <= 3 then
			tldlength = 2
			tld = splitdomain[numparts-1] .. "." .. splitdomain[numparts]
		else
			tld = splitdomain[numparts]
		end

		second = splitdomain[numparts - tldlength]

		if numparts > tldlength + 1 then
			third = splitdomain[numparts-(tldlength+1)]
		end
	end 

	retval = {}
	retval["tld"] = tld
	retval["second"] = second
	retval["third"] = third
	retval["last"] = splitdomain[numparts]
	return retval
end

-- Function to print out the first `numrows` of instances
function inspect(instances, numfields, numrows)
	for i=1,numrows do
		for j=1,numfields do
			print(i .. " " .. j .. " " .. instances[i][j])
		end
		print(instances[i].label)
	end
end

-- Read csv file in filename into a lua table data structure
function readcsv(filename)
	io.input(filename)
	local count = 1
	    while true do
	      local line = io.read()
	      if line == nil then break end
	      
	      result = csplit(line, ",")
	      fields = result.parts
	      numfields = result.num

	      label = fields[numfields]

	      row = {}
	      for i=1,numfields-1 do
	      	row[i] = fields[i]
	      end
	      row["label"] = tonumber(label)

	      instances[count] = row

	      count = count + 1
		end
	print("Read " .. count .. " rows from '" .. filename .. "'" )
	retval = {}
	retval["instances"] = instances
	retval["count"] = count
	return(retval)
end

-- Sample csv file in filename into a lua table data structure
-- filename = file to read in
-- numfolds = number of samples to create
-- foldsToKeep = Table of list of folds to keep (nil for all)
function samplecsv(filename, numfolds, foldToKeep)
	io.input(filename)
	local count = 1
	    while true do
	      local line = io.read()
	      if line == nil then break end
	      
	      result = csplit(line, ",")
	      fields = result.parts
	      numfields = result.num

	      label = fields[numfields]

	      row = {}
	      for i=1,numfields-1 do
	      	row[i] = fields[i]
	      end
	      row["label"] = tonumber(label)

	      instances[count] = row

	      count = count + 1
		end
	print("Sampled " .. count .. " rows from '" .. filename .. "'" )
	retval = {}
	retval["instances"] = instances
	retval["count"] = count
	return(retval)
end

--[[
	Transform contains the all of the necessary code to create functions suitable 
	for input to the model of a particular version.  We will need to version the 
	function name most likely
]]
function transform(domain)
	splits = csplit(domain, "%.")
	splitdomain = splits.parts
	numparts = splits.num

	parts = parse(splitdomain, numparts)

	newrow = {}
	newrow[1] = numparts  -- Number of domain parts
	newrow[2] = string.len(parts.tld) --Length of tld
	newrow[3] = string.len(parts.second) --Length of 2LD
	newrow[4] = string.len(parts.third) --Length of 3LD

	if string.len(parts.third) > 0 then -- Has a 3LD
		newrow[5] = 1
	else
		newrow[5] = 0
	end

	if (numparts > 3 and string.len(parts.tld) <= 3) or numparts > 4  then --Has more than 3LD
		newrow[6] = 1
	else
		newrow[6] = 0
	end

	if string.len(parts.tld) < 3 then --Is just a country code
		newrow[7] = 1
	else
		newrow[7] = 0
	end

	return newrow
end




