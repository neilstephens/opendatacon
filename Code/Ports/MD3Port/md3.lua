--Define the strings for the functions
functionnames = {[5] =  "Analog Unconditional",
				[6] = 	"Analog Delta Scan",
				[7] = 	"Obsolete Digital Unconditional",
				[8] =  "Digital Delta Scan",
				[9] = 	"HRER List Scan",
				[10] = "Digital Change Of State",
				[11] = "Digital COS Time Tagged", 
				[12] = "Digital Unconditional",
				[13] = "Analog No Change Reply", 
				[14] = "Digital No Change Reply", 
				[15] = "Control Request OK",
				[16] = "Freeze And Reset",
				[17] = "POM Type Control",
				[19] = "DOM Type Control",
				[20] = "Input Point Control",
				[21] = "Raise Lower Type Control",
				[23] = "AOM Type Control",
				[27] = "Change of State (COS) Report",
				[30] = "Control or Scan Request Rejected",
				[31] = "Counter Scan",
				[35] = "Floating Point Unconditional",
				[36] = "Floating Point Delta",
				[37] = "Floating Point No Change",
				[38] = "Floating Point O/P Command",
				[39] = "Floating Point COS / Time-tagged Scan",
				[40] = "System SIGNON Control",
				[41] = "System SIGNOFF Control", 
				[42] = "System RESRART Control", 
				[43] = "System SET DATE AND TIME Control",
				[44] = "System SET DATE AND TIME Control w TimeZone Offset",
				[50] = "file download",
				[51] = "file upload",
				[52] = "System Flag Scan",
				[60] = "Low Resolution Event List Scan" }
fcstab =
{
	0x00, 0x6c, 0xd8, 0xb4, 0xdc, 0xb0, 0x04, 0x68, 0xd4, 0xb8, 0x0c, 0x60, 0x08, 0x64, 0xd0, 0xbc,
	0xc4, 0xa8, 0x1c, 0x70, 0x18, 0x74, 0xc0, 0xac, 0x10, 0x7c, 0xc8, 0xa4, 0xcc, 0xa0, 0x14, 0x78,
	0xe4, 0x88, 0x3c, 0x50, 0x38, 0x54, 0xe0, 0x8c, 0x30, 0x5c, 0xe8, 0x84, 0xec, 0x80, 0x34, 0x58,
	0x20, 0x4c, 0xf8, 0x94, 0xfc, 0x90, 0x24, 0x48, 0xf4, 0x98, 0x2c, 0x40, 0x28, 0x44, 0xf0, 0x9c,
	0xa4, 0xc8, 0x7c, 0x10, 0x78, 0x14, 0xa0, 0xcc, 0x70, 0x1c, 0xa8, 0xc4, 0xac, 0xc0, 0x74, 0x18,
	0x60, 0x0c, 0xb8, 0xd4, 0xbc, 0xd0, 0x64, 0x08, 0xb4, 0xd8, 0x6c, 0x00, 0x68, 0x04, 0xb0, 0xdc,
	0x40, 0x2c, 0x98, 0xf4, 0x9c, 0xf0, 0x44, 0x28, 0x94, 0xf8, 0x4c, 0x20, 0x48, 0x24, 0x90, 0xfc,
	0x84, 0xe8, 0x5c, 0x30, 0x58, 0x34, 0x80, 0xec, 0x50, 0x3c, 0x88, 0xe4, 0x8c, 0xe0, 0x54, 0x38,
	0x24, 0x48, 0xfc, 0x90, 0xf8, 0x94, 0x20, 0x4c, 0xf0, 0x9c, 0x28, 0x44, 0x2c, 0x40, 0xf4, 0x98,
	0xe0, 0x8c, 0x38, 0x54, 0x3c, 0x50, 0xe4, 0x88, 0x34, 0x58, 0xec, 0x80, 0xe8, 0x84, 0x30, 0x5c,
	0xc0, 0xac, 0x18, 0x74, 0x1c, 0x70, 0xc4, 0xa8, 0x14, 0x78, 0xcc, 0xa0, 0xc8, 0xa4, 0x10, 0x7c,
	0x04, 0x68, 0xdc, 0xb0, 0xd8, 0xb4, 0x00, 0x6c, 0xd0, 0xbc, 0x08, 0x64, 0x0c, 0x60, 0xd4, 0xb8,
	0x80, 0xec, 0x58, 0x34, 0x5c, 0x30, 0x84, 0xe8, 0x54, 0x38, 0x8c, 0xe0, 0x88, 0xe4, 0x50, 0x3c,
	0x44, 0x28, 0x9c, 0xf0, 0x98, 0xf4, 0x40, 0x2c, 0x90, 0xfc, 0x48, 0x24, 0x4c, 0x20, 0x94, 0xf8,
	0x64, 0x08, 0xbc, 0xd0, 0xb8, 0xd4, 0x60, 0x0c, 0xb0, 0xdc, 0x68, 0x04, 0x6c, 0x00, 0xb4, 0xd8,
	0xa0, 0xcc, 0x78, 0x14, 0x7c, 0x10, 0xa4, 0xc8, 0x74, 0x18, 0xac, 0xc0, 0xa8, 0xc4, 0x70, 0x1c
}

-- declare our protocol
local md3_proto = Proto("md3","MD3 Protocol")

local f = md3_proto.fields
-- Need to use ProtoFields for the Station, MtoS, Function and Data so that we can then display them as columns, which then means we can export to excel.
f.md3function = ProtoField.uint8 ("md3.function",  "Function", base.DEC)
f.md3seq = ProtoField.uint8 ("md3.md3seq",  "Sequence Number", base.DEC)
f.station  = ProtoField.uint8 ("md3.station",  "Station", base.HEX) 
f.mtos = ProtoField.bool ("md3.mtos", "MasterCommand")
f.module = ProtoField.uint8 ("md3.module",  "Module Addr", base.HEX) 
f.taggedcount = ProtoField.uint8 ("md3.taggedcount",  "TaggedCount", base.HEX) 
f.data = ProtoField.bytes  ("md3.data", "Data")

function CheckMD3CRC(data0,data1,data2,data3, crcbyte)

	local CRCChar = fcstab[bit32.band(data0 , 0x0ff)+1]
	CRCChar = fcstab[bit32.band(bit32.bxor(data1 , CRCChar),0xff)+1]
	CRCChar = fcstab[bit32.band(bit32.bxor(data2 , CRCChar),0xff)+1]
	CRCChar = fcstab[bit32.band(bit32.bxor(data3 , CRCChar),0xff)+1]

	CRCChar = bit32.band(bit32.bnot(bit32.rshift(CRCChar,2)) , 0x3F)

	return bit32.band(CRCChar,0x3f) == bit32.band(crcbyte , 0x3F)
end

-- create a function to dissect it
function md3_proto.dissector(buffer,pinfo,tree)

	local pktlen = buffer:reported_length_remaining()
	local functionname = functionnames[buffer(1,1):uint()]
	
	--debug("entered the md3 dissector");
	
	-- Check to see if we actually have an MD3 packet
	-- Some out of order TCP packets don't get called for the dissector, so they are MD3 but don't get classified....

	if ((pktlen % 6) ~= 0) then
	--debug("Exited md3 dissector - not mod 6");
		return 0	-- Not an MD3 Packet, must be *6
	end
	
	if (buffer(4,1):bitfield(0,1) ~= 0) then
		return 0 --not the first block (format blk) of an MD3 message: probably TCP framing issue
	end
	if (buffer(5,1):uint() ~= 0) then
		return 0	-- Not an MD3 Packet, padding byte must be zero
	end
	if (buffer(0,1):uint() == 0) then
		return 0	-- Not an MD3 Packet, station cannot be zero
	end
	if ((pktlen >= 12) and (buffer(11,1):uint() ~= 0)) then
		return 0	-- Not an MD3 Packet, message contains at least two blocks and second padding byte is not zero.
	end
		
	-- Check the CRC of the 1st block only
	if (CheckMD3CRC(buffer(0,1):uint(),buffer(1,1):uint(),buffer(2,1):uint(),buffer(3,1):uint(),buffer(4,1):uint()) == false) then	
		return 0	-- Checksum failed...
	end
	
	if ( functionname == nil) then
		functionname = "Unknown MD3 Function"	-- Not an MD3 Packet
	end
	
    pinfo.cols.protocol = "md3"
	
    local subtree = tree:add(md3_proto,buffer(),"MD3 Protocol Data")		
	subtree:append_text (" - " .. functionname)
	
	-- Now add the sub trees for the fields we are interested in
	subtree:add(f.md3function,buffer(1,1))
	local stationleaf = subtree:add(f.station, buffer(0,1):bitfield(1,7))
	if (buffer(0,1):bitfield(1,7) == 0x7F) then
		stationleaf.append_text(" - WARNING EXTENDED ADDRESS NOT DECODED");
	end
	local ismaster = true;
	local extra_info = " - Master"
	if (buffer(0,1):bitfield(0,1) == 1) then
		ismaster = false;
		extra_info = " - Slave"
		subtree:add(f.md3seq,buffer(2,1):bitfield(4,4))
	else
		subtree:add(f.md3seq,buffer(3,1):bitfield(0,4))
	end
	pinfo.cols.info = functionname .. extra_info
	
	subtree:add(f.mtos, ismaster)
	
	subtree:add(f.module, buffer(2,1):uint())
	
	-- This only applies to Fn 11 digital COS, but I need it for Excel analysis. Coment out if not using.
	--subtree:add(f.taggedcount, buffer(2,1):bitfield(0,4))
	
	subtree:add("Data Length : " ..  pktlen)
	
	local datatree = subtree:add(f.data, buffer())
	
	-- spit out in blocks of 6, spaces on each byte
	local block = 0
	for i = 0,pktlen-1,6 
	do
		local fmt_bit = " - "
		if (buffer(i+4,1):bitfield(0,1) == 0) then
			fmt_bit = fmt_bit.."FMT"
		end
		if (buffer(i+4,1):bitfield(1,1) == 1) then
			fmt_bit = fmt_bit.."|EOM"
		end
		local crc_check = " - CRC PASS"
		if (CheckMD3CRC(buffer(i,1):uint(),buffer(i+1,1):uint(),buffer(i+2,1):uint(),buffer(i+3,1):uint(),buffer(i+4,1):uint()) == false) then	
			crc_check = " - CRC FAIL"	-- Checksum failed...
		end
		datatree:add("Block(" .. block .. ")", buffer:bytes(i,6):tohex(false,' ')..fmt_bit..crc_check)
		block = block+1 
	end
	
	-- If not our protocol, we should return 0. If it is ours, return nothing.
	--mark the conversation as md3 - this is just for when we're called as a heuristic
	pinfo.conversation = md3_proto
end

--register as a heuristic dissector
--TODO: move the heuristics out of the dissection function, into a separate one
md3_proto:register_heuristic("tcp",md3_proto.dissector)

-- OR register for specific ports:

-- load the tcp.port table
--local tcp_table = DissectorTable.get("tcp.port")
-- register our protocol to handle tcp port 5001
-- Add the other ports we use...
--tcp_table:add(5001,md3_proto)
--tcp_table:add(5002,md3_proto)
--tcp_table:add(5003,md3_proto)
--tcp_table:add(5004,md3_proto)
--tcp_table:add(5005,md3_proto)
--tcp_table:add(5006,md3_proto)
--tcp_table:add(5007,md3_proto)
--tcp_table:add(5008,md3_proto)
--tcp_table:add(5009,md3_proto)
--tcp_table:add(5010,md3_proto)
