--Define the strings for the functions
functionnames = {
	[0] = "Scan Data",
	[1] = "Execute Command",
	[2] = "Trip",
	[3] = "Setpoint A",
	[4] = "Close",
	[5] = "Setpoint B",
	[8] = "Reset RTU",
	[9] = "Master Station Request",
	[10] = "Send New SOE",
	[11] = "Repeat SOE",
	[13] = "Unit Raise / Lower",
	[14] = "Freeze and Scan Accumulators",
	[15] = "Freeze, Scan and Reset Accumulators"
	}

BCH = {0x12,0x9,0x16,0xB,0x17,0x19,0x1E,0xF,0x15,0x18,0xC,0x6,0x3,
       0x13,0x1B,0x1F,0x1D,0x1C,0xE,0x7,0x11,0x1A,0xD,0x14,0xA,0x5}

-- declare our protocol
local conitel_proto = Proto("conitel","Conitel Protocol")

local f = conitel_proto.fields
-- Need to use ProtoFields for the Station, MtoS, Function and Data so that we can then display them as columns, which then means we can export to excel.

f.conitelfunction = ProtoField.uint8 ("conitel.function",  "Function", base.DEC)
f.station  = ProtoField.uint8 ("conitel.station",  "Station", base.DEC)
f.group = ProtoField.uint8("conitel.group", "Group", base.DEC)
f.blockcount = ProtoField.uint8("conitel.blockcount","BlockCount", base.HEX)
f.data = ProtoField.bytes("conitel.data", "Packet Data")
--f.blocks = ProtoField.uint8("conitel.data", "Payload Block")
--f.adata = ProtoField.uint16("conitel.adata","A Data", base.HEX)
--f.bdata = ProtoField.uint16("conitel.bdata","B Data", base.HEX)
f.soedata = ProtoField.uint8("conitel.soedata", "SOE")
--f.soegroup = ProtoField.uint8("conitel.soegroup","SOE Group", base.HEX)
--f.soeindex = ProtoField.uint16("conitel.soeindex","SOE Index", base.HEX)

-- create a function to dissect it
function conitel_proto.dissector(buffer,pinfo,tree)

	local pktlen = buffer:reported_length_remaining()

	--debug("entered the conitel dissector");

	-- Check to see if we actually have an conitel packet
	-- Some out of order TCP packets don't get called for the dissector, so they are conitel but don't get classified....

	if ((pktlen % 4) ~= 0) then
		--debug("Exited conitel dissector - not mod 4");
		return 0	-- Not an conitel Packet, must be *4
	end
	--debug("Got past mod 4 check");

	if (pktlen > 4*16) then
		return 0	-- Not a Conitel Packet, too many blocks
	end
	--debug("Got past max packet size check");

	local blockcount = pktlen / 4
	local block = buffer(0,4):uint()		-- 4 Bytes per block
	local conitelfunction = buffer(0,1):bitfield(0,4)
	local functionname = functionnames[conitelfunction]
	local group = buffer(1,1):bitfield(0,4)
	local station = buffer(0,1):bitfield(4,4)

	-- Just a double check we are getting the same thing...
	--debug("Function " .. conitelfunction .. " - " .. GetFunction(block));
	--debug("Station " .. station .. " - " .. GetStation(block));
	--debug("Group " .. group .. " - " .. GetGroup(block));

	if ( functionname == nil) then
		functionname = "Unknown conitel Function"	-- Not an conitel Packet
	end

	-- Check all blocks for correct bits
	for i = 0, blockcount-1,1
	do
		block = buffer(i*4,4):uint()

		-- Check the A bit is zero in the first block and one in each subsequent block
		if (i == 0) then
			if (GetBlockABit(block) ~= 0) then
				--debug("BlockABit not zero in first block");
				return 0
			end
		else
			if (GetBlockABit(block) ~= 1) then
				--debug("BlockABit not one in non-first block");
				return 0
			end
		end

		-- BlockB Bit Must always be zero
		if (GetBlockBBit(block) ~= 0) then
			--debug("BlockBBit not zero");
			return 0
		end

		-- Only the last block will have the EOM flag set
		if (i == blockcount-1) then
			if (GetEOM(block) ~= 1) then
				--debug("EOMBit not set in last block");
				return 0
			end
		else
			if (GetEOM(block) ~= 0) then
				--debug("EOMBit set in non-last block");
				return 0
			end
		end

		-- Check BCH Does the calculation match the stored value?
		if (GetBCH(block) ~= CalculateBCH(block))then
			--debug("BCH Failed in block" .. i);
			return 0
		end
	end

    pinfo.cols.protocol = "conitel"
	pinfo.cols.info:set("Stn: "..station.." Grp: "..group.." Fn: "..functionname )
	pinfo.cols.info:fence()

    local subtree = tree:add(conitel_proto,buffer(),"Conitel/Baker Protocol Data")
	subtree:append_text (" - Fn: " .. functionname)

	-- Now add the sub trees for the fields we are interested in
	subtree:add(f.conitelfunction,conitelfunction) -- The buffer notation (highlighting in data) only works for full bytes.

	local stationleaf = subtree:add(f.station, station)

	--if (buffer(0,1):bitfield(1,7) == 0x7F) then
	--	stationleaf.append_text(" - WARNING EXTENDED ADDRESS NOT DECODED");

	local groupleaf = subtree:add(f.group, group)

	-- Dont really need block count! DataLength / 4
	local blockcounttree = subtree:add(f.blockcount, blockcount)

	-- change to spit this out in blocks of 4, spaces on each byte
	local dataleaf = subtree:add(f.data, buffer())

	--	local datalenleaf = subtree:add("Data Length : " ..  pktlen)
	local blockleaf = subtree:add("Payload Data")
	blockleaf:add("1B " .. string.format("0x%03x",GetBlockA(buffer(0,4):uint())))

	for blk = 1, blockcount-1,1
	do
		local block = buffer(blk*4,4):uint()
		
		blockleaf:add(blk+1 .. "A " .. string.format("0x%03x",GetBlockA(block)))
		blockleaf:add(blk+1 .. "B " .. string.format("0x%03x",GetBlockB(block)))
	end
	
	if (conitelfunction == 10) then
	
		local block = buffer(0,4):uint()
		
		local soeblockcount = 1		
		local payloads = {}
		payloads[soeblockcount] = GetBlockB(block)
		soeblockcount = soeblockcount + 1
		
		for blk = 1, blockcount-1,1
		do
			block = buffer(blk*4,4):uint()
			payloads[soeblockcount] = GetBlockA(block)
			soeblockcount = soeblockcount + 1
			payloads[soeblockcount] = GetBlockB(block)
			soeblockcount = soeblockcount + 1
		end
		
		-- So now we have the payloads, need to extract the group, index, timestamp for the SOE records 
		-- variable length - 41 or 30 bits long. The first record should always be 41 bits
		local payloadbitindex = 0
		local maxpayloadbitindex = 12 * #payloads
		local soegroup = 0
		local soeindex = 0
		local soebitvalue = 0
		local result = false
		local soeblk = 1
		
		while true
		do
			result, payloadbitindex, soegroup, soeindex, soebitvalue = DecodeSOERecord(payloads, payloadbitindex, maxpayloadbitindex )
			-- If the preceeding function failed, exit loop
			if (result == false) then break end
			
			local blockleaf = subtree:add(f.soedata, soeblk)
			blockleaf:add("SOE Group " .. soegroup)
			blockleaf:add("SOE Index " .. soeindex)
			blockleaf:add("SOE Bitvalue "..soebitvalue)
			soeblk = soeblk + 1
		end
	end
	
	-- If not our protocol, we should return 0. If it is ours, return nothing.
	--debug("Packet is good");
	pinfo.conversation = conitel_proto
end

-- The payload is an array of 12 bit values, stored in the lower 12 bits.
-- payloadbitindex is the index into this array. So 0 is bit 11 in the first payload array value. 12 is bit 11 in the second payload entry.
-- We return true (succeed) or false. The payloadbitindex will be updated and returned
function DecodeSOERecord(payloads, payloadbitindex, maxpayloadbitindex)

	soegroup = 0
	soeindex = 0
	
	if (payloadbitindex + 30 > maxpayloadbitindex) then	-- make sure there is at least enough data for a short SOE record
		return false, payloadbitindex, soegroup, soeindex
	end
	
	-- Collect bit values into variables, pass in the payload array, the bit index into that array and the number of bits
	soegroup,payloadbitindex = ExtractBits(payloads, payloadbitindex,3)
	
	soeindex,payloadbitindex = ExtractBits(payloads, payloadbitindex,7)

	soebitvalue,payloadbitindex = ExtractBits(payloads, payloadbitindex,1)
	QualityBit,payloadbitindex = ExtractBits(payloads, payloadbitindex,1)
	TimeFormatBit,payloadbitindex = ExtractBits(payloads, payloadbitindex,1)

	if (TimeFormatBit == 1) then -- Long time
		print("Long Time SOE")
		Hour,payloadbitindex = ExtractBits(payloads, payloadbitindex,5)
		Minute,payloadbitindex = ExtractBits(payloads, payloadbitindex,6)
	end
	
	Second,payloadbitindex = ExtractBits(payloads, payloadbitindex,6)
	Millisecond,payloadbitindex = ExtractBits(payloads, payloadbitindex,10)
	LastEventFlag,payloadbitindex = ExtractBits(payloads, payloadbitindex,1)
	
	return true, payloadbitindex, soegroup, soeindex, soebitvalue
end

function ExtractBits(payloads, payloadbitindex, numberofbits)

	local result = 0;
	for bit = numberofbits-1, 0 ,-1
	do
		result = result + GetBit(payloads, payloadbitindex, bit)
		payloadbitindex = payloadbitindex + 1
	end
	print("Result, numbits "..result..", "..numberofbits)
	return result, payloadbitindex
end

function GetBit(payloads, payloadbitindex, bitleftshift)
	local block = math.floor(payloadbitindex / 12)
	local bit = payloadbitindex % 12
	
	if (block+1 > #payloads) then
		return 0
	end
	
	local bitvalue = bit32.band(bit32.rshift(payloads[block+1],11-bit), 0x01)
	-- Now left shift to put the bit in the correct position for the answer
	bitvalue = bit32.lshift(bitvalue, bitleftshift)
	return bitvalue
end

function CalculateBCH(block)

    local bch = 0

    -- xor each of the sub-remainders corresponding to set bits in the first 26 bits of the payload
	for bit = 0, 25 ,1
	do
        if (bit32.band(bit32.lshift(block, bit),0x80000000) == 0x80000000)	then -- The bit is set
            bch = bit32.band(bit32.bxor(bch,BCH[bit+1]),0x1F)	-- LUA Array index is 1 based, not 0. Limit to 5 bits
		end
	end

    return bch
end

function GetFunction(block)
	return bit32.band(bit32.rshift(block,28),0x0f)
end
function GetStation(block)
	return bit32.band(bit32.rshift(block,24),0x0f)
end
function GetGroup(block)
	return bit32.band(bit32.rshift(block,20),0x0f)
end
function GetBlockABit(block)
	return bit32.band(bit32.rshift(block,19),0x01)
end
function GetBlockBBit(block)
	return bit32.band(bit32.rshift(block,6),0x01)
end
function GetBlockA(block)
	return bit32.band(bit32.rshift(block,20),0x0FFF)
end
function GetBlockB(block)
	return bit32.band(bit32.rshift(block,7),0x0FFF)
end
function GetBCH(block)
	return bit32.band(bit32.rshift(block,1),0x01F)
end
function GetEOM(block)
	return bit32.band(block,0x01)
end

--register as a heuristic dissector
conitel_proto:register_heuristic("tcp",conitel_proto.dissector)

-- load the tcp.port table
--local tcp_table = DissectorTable.get("tcp.port")
-- register our protocol to handle tcp port 5001
-- Add the other ports we use...
--tcp_table:add(5001,conitel_proto)
--tcp_table:add(5002,conitel_proto)
--tcp_table:add(5003,conitel_proto)
--tcp_table:add(5004,conitel_proto)
--tcp_table:add(5005,conitel_proto)
--tcp_table:add(5006,conitel_proto)
--tcp_table:add(5007,conitel_proto)
--tcp_table:add(5008,conitel_proto)
--tcp_table:add(5009,conitel_proto)
--tcp_table:add(5010,conitel_proto)
