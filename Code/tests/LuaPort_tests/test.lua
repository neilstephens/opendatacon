

function Enable()
	log.info("Enable()");
	return;
end

function Disable()
	log.info("Disable()");
	return;
end

function ProcessConfig(Conf)
	log.info("ProcessConfig()");
	return;
end

function EventHandler(EventType, Index, Time, Quality, Payload, Sender)
	log.info("EventHandler( "..EventType..","..Index..")");
	return;
end

log.info("QualityFlag.COMM_LOST : " .. QualityFlag.COMM_LOST);
log.info("QualityFlag.RESTART : " .. QualityFlag.RESTART);

B = BinaryEventType();
A = AnalogEventType();
Q = QualityFlags(QualityFlag.COMM_LOST,QualityFlag.RESTART);

log.info("AnalogEventType() returns " .. A);
log.info("BinaryEventType() returns " .. B);
log.info("QualityFlags() returns " .. Q);
