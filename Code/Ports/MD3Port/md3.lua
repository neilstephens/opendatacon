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
f.taggedcount = ProtoField.uint8 ("md3.taggedcount",  "TaggedCount", base.DEC)
f.modulecount = ProtoField.uint8 ("md3.modulecount",  "ModuleCount", base.DEC)
f.data = ProtoField.bytes  ("md3.data", "Data")
f.decode = ProtoField.bytes  ("md3.decode", "Decode DCOS")
f.flags = ProtoField.bool ("md3.flags", "Flag Block Present")

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
	local functionname = ""
	local functioncode = 0

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

	functioncode = buffer(1,1):uint()
	functionname = functionnames[functioncode]
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
	end
	subtree:add(f.mtos, ismaster)
	pinfo.cols.info = functionname .. extra_info

	-- Add sequence number only for those function codes that use it
	if functioncode == 11 or functioncode == 12 or functioncode == 14 then
		if ismaster == false then
			subtree:add(f.md3seq,buffer(2,1):bitfield(4,4))
		else
			subtree:add(f.md3seq,buffer(3,1):bitfield(0,4))
		end
	end
	
	-- Add module count only for those function codes that use it
	local modulecount = 0
	if functioncode == 5 or functioncode == 6 or functioncode == 7 or functioncode == 8 or functioncode == 10 or
	   functioncode == 11 or functioncode == 12 or functioncode == 13 or functioncode == 14 or
	   functioncode == 30 or functioncode == 31
	then
		subtree:add(f.modulecount, buffer(3,1):bitfield(4,4))
		modulecount = buffer(3,1):bitfield(4,4)
	end

	-- This only applies to Fn 11 digital COS, master and slave
	if functioncode == 11 then
		subtree:add(f.taggedcount, buffer(2,1):bitfield(0,4))
	else
		subtree:add(f.module, buffer(2,1):uint())
	end

	subtree:add("Data Length : " ..  pktlen)

	local datatree = subtree:add(f.data, buffer())

	-- spit out in blocks of 6, spaces on each byte
	local block = 0
	local i = 0
	while i < pktlen-1
	do
		local fmt_bit = " - "
		if (buffer(i+4,1):bitfield(0,1) == 0) then
			fmt_bit = fmt_bit.."FMT"
		end
		if (buffer(i+4,1):bitfield(1,1) == 1) then
			fmt_bit = fmt_bit.."|EOM"
		end
		if fmt_bit == " - " then
			fmt_bit = ""
		end
		local crc_check = " - CRC PASS"
		if (CheckMD3CRC(buffer(i,1):uint(),buffer(i+1,1):uint(),buffer(i+2,1):uint(),buffer(i+3,1):uint(),buffer(i+4,1):uint()) == false) then
			crc_check = " - CRC FAIL"	-- Checksum failed...
		end
		local dcos = ""
		if (functioncode == 11) and (i ~= 0) then
			if i/6 <= modulecount then
				dcos = " - DAT"
				if (buffer(i,1):uint() == 0) and (buffer(i+1,1):uint() == 0) then
					--Status Block
					dcos = dcos.." - Status Block, Addr: "..buffer(i+2,1).." Fail Status: "..buffer(i+2,1)
				else
					-- Datablock
					dcos = dcos.." - Data Block, Addr: "..buffer(i,1).." Msec Offset: "..string.format("%3d",buffer(i+1,1):uint()).." Data: "..string.format("0x%04X",buffer(i+2,2):uint())
				end
			else
				dcos = " - COS"
			end
		end
		datatree:add("Block(" .. block .. ")", buffer:bytes(i,6):tohex(false,' ')..fmt_bit..crc_check..dcos)
		block = block+1
		i = i + 6
	end

	if (functioncode == 11) then
		-- We want to display the decodes DCOS data. Skip the first packet.
		dcosbuffer = {}
		local i = 6
		local newi = 0
		local dcoshex = " "
		-- print("Pktlen "..pktlen)

		while i < pktlen
		do
			-- Skip the checksum and padding byte
			if (i % 6) == 4 then
				i = i + 2
				goto skip_to_next1
			end
			dcosbuffer[newi] = buffer(i,1):uint()
			dcoshex = dcoshex..string.format("%02X",buffer(i,1):uint())
			if newi % 4 == 3 then
				dcoshex = dcoshex.." "
			end
			newi = newi + 1
			i = i + 1
			::skip_to_next1::
		end
		-- print(dcoshex)
		local datatree = subtree:add(f.decode)
		datatree:add("Data ", newi.." - "..dcoshex)
		local has_flag_block = false
		i = 0
		while i < newi - 3
		do
			if i/4 < modulecount then
				dcos = " - DAT"
				if (dcosbuffer[i] == 0) and (dcosbuffer[i+1] == 0) then
					--Status Block
					dcos = dcos.." - Status Block, Addr: "..dcosbuffer[i+2].." Fail Status: "..dcosbuffer[i+2]
				else
					-- Datablock
					dcos = dcos.." - Data Block, Addr: "..dcosbuffer[i].." Msec Offset: "..string.format("%3d",dcosbuffer[i+1]).." Data: "..string.format("0x%04X",dcosbuffer[i+2]*256+dcosbuffer[i+3])
				end
			else
				dcos = " - COS"
				-- Look for buffer overflow and time delta blocks and mark them as such
				if dcosbuffer[i] == 0 then
					if dcosbuffer[i+1] == 0 then
						if dcosbuffer[i+2] == 0 then
							--Flag block
							has_flag_block = true
							if dcosbuffer[i+3] == 1 then
								dcos = dcos.." - Flag Block - COS BUFFER OVERFLOW!"
							else
								dcos = dcos.." - Flag Block - "..dcosbuffer[i+3]
							end
						else
							dcos = dcos.." - Decode Error! "
						end
					else
						--TimeBlock to only two byte long, not 4 as for everything else
						dcos = dcos.." - Time Block msec: "..dcosbuffer[i+1]*256
						datatree:add("Block ", string.format("0x%04X",dcosbuffer[i]*256+dcosbuffer[i+1])..dcos)
						i = i + 2
						goto skip_to_next
					end
				else
					-- Datablock
					dcos = dcos.." - Data Block, Addr: "..dcosbuffer[i].." Msec Offset: "..string.format("%3d",dcosbuffer[i+1]).." Data: "..string.format("0x%04X",dcosbuffer[i+2]*256+dcosbuffer[i+3])
				end
			end
			if (i+3 < newi) then
				datatree:add("Block ", string.format("0x%02X",dcosbuffer[i])..string.format("%02X",dcosbuffer[i+1])..string.format("%02X",dcosbuffer[i+2])..string.format("%02X",dcosbuffer[i+3])..dcos)
			else
				datatree:add("Index Error ",i.." "..newi)
			end
			i = i + 4
			::skip_to_next::
		end
		subtree:add(f.flags, has_flag_block)
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
