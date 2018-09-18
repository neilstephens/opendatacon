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

f.conitelfunction = ProtoField.uint8 ("conitel.function",  "Function", base.HEX)
f.station  = ProtoField.uint8 ("conitel.station",  "Station", base.HEX)
f.group = ProtoField.uint8("conitel.group", "Group", base.HEX)
f.blockcount = ProtoField.uint8("conitel.blockcount","BlockCount", base.HEX)
f.data = ProtoField.bytes("conitel.data", "Packet Data")
f.blocks = ProtoField.uint8("conitel.data", "Block")
f.adata = ProtoField.uint16("conitel.adata","A Data", base.HEX)
f.bdata = ProtoField.uint16("conitel.bdata","B Data", base.HEX)

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

	for blk = 1, blockcount-1,1
	do
		block = buffer(blk*4,4):uint()
		local blockleaf = subtree:add(f.blocks, blk)
		blockleaf:add(f.adata,GetBlockA(block))
		blockleaf:add(f.bdata,GetBlockB(block))
	end

	-- If not our protocol, we should return 0. If it is ours, return nothing.
	--debug("Packet is good");
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

-- load the tcp.port table
local tcp_table = DissectorTable.get("tcp.port")
-- register our protocol to handle tcp port 5001
-- Add the other ports we use...
tcp_table:add(5001,conitel_proto)
tcp_table:add(5002,conitel_proto)
tcp_table:add(5003,conitel_proto)
tcp_table:add(5004,conitel_proto)
tcp_table:add(5005,conitel_proto)
tcp_table:add(5006,conitel_proto)
tcp_table:add(5007,conitel_proto)
tcp_table:add(5008,conitel_proto)