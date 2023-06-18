

function Enable()
	log.info("Enable()");
	return;
end

function Disable()
	log.info("Disable()");
	return;
end

function Build()
	log.info("Build()");
	return;
end

function Event(EventInfo, SenderName, StatusCallback)
	log.info("Event(): EventType "..EventInfo.EventType..
           ", Index "..EventInfo.Index..
           ", Timestamp "..msSinceEpochToDateTime(EventInfo.Timestamp)..
           ", SenderName "..SenderName);
  StatusCallback(CommandStatus.SUCCESS);
	return;
end

log.info("EventType.Analog : " .. EventType.Analog);
log.info("EventType.Binary : " .. EventType.Binary);
log.info("CommandStatus.SUCCESS : " .. CommandStatus.SUCCESS);
log.info("QualityFlag.COMM_LOST : " .. QualityFlag.COMM_LOST);
log.info("QualityFlag.RESTART : " .. QualityFlag.RESTART);

Q = QualityFlags(QualityFlag.COMM_LOST,QualityFlag.RESTART);
log.info("QualityFlags() returns " .. Q);
DT = msSinceEpochToDateTime(msSinceEpoch());
log.info("msSinceEpochToDateTime() returns "..DT);
