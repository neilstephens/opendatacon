function updateMimic(mimic, data)
{
    //Analogue inputs
    mimic.getElementById("weather_wind_speed").textContent = data.AnalogCurrent[0].toFixed(2);
    mimic.getElementById("weather_wind_direction").textContent = data.AnalogCurrent[1].toFixed(1);
    mimic.getElementById("weather_humidity").textContent = data.AnalogCurrent[2].toFixed(2);
    mimic.getElementById("weather_pressure").textContent = data.AnalogCurrent[5].toFixed(0);
    mimic.getElementById("weather_temp").textContent = data.AnalogCurrent[3].toFixed(2);

    var windScale = data.AnalogCurrent[0] / 20; // Normalised to 72 k/h
    mimic.getElementById("wind_arrow").setAttributeNS(null, 'transform', "translate(126, 150) rotate("+data.AnalogCurrent[1]+") scale(1,"+windScale+") translate(-126, -150) ");

    mimic.getElementById("PV_voltage").textContent = (data.AnalogCurrent[12544]/100).toFixed(2);
    mimic.getElementById("PV_current").textContent = (data.AnalogCurrent[12545]/100).toFixed(2);
    mimic.getElementById("battery_voltage").textContent = (data.AnalogCurrent[12548]/100).toFixed(2);
    mimic.getElementById("battery_current").textContent = (data.AnalogCurrent[12549]/100).toFixed(2);
    mimic.getElementById("battery_SOC").textContent = (data.AnalogCurrent[12570]).toFixed(0);
    mimic.getElementById("battery_temp").textContent = (data.AnalogCurrent[12560]/100).toFixed(2);

    mimic.getElementById("batt_SOC_display").textContent = (data.AnalogCurrent[12570]).toFixed(0);
    var batpix = 90 * (data.AnalogCurrent[12570]/100);
    mimic.getElementById("batt_SOC_meter").setAttributeNS(null, 'y', 240 - batpix);
    mimic.getElementById("batt_SOC_meter").setAttributeNS(null, 'height', batpix);

    // Picture variables
    var t1x_fixed = 25; // Target 1 x coord
    var t1y_fixed = 308; // Target 1 y coord
    var t2x_fixed = 362; // Target 2 x coord
    var t2y_fixed = 300; // Target 2 y coord
    var t3x_fixed = 308; // Line target x coord (for calculating line blowout)
    var m_per_pix = 0.03127; // meters per pixel (for calculating line blowout)

    // Picture animation - shouldn't need to change anything past here
    var t1x = data.AnalogCurrent[30]-t1x_fixed;
    var t1y = data.AnalogCurrent[31]-t1y_fixed;
    var t2x = data.AnalogCurrent[33]-t2x_fixed;
    var t2y = data.AnalogCurrent[34]-t2y_fixed;

    mimic.getElementById("target1_x").textContent = (data.AnalogCurrent[30]).toFixed(2);
    mimic.getElementById("target1_y").textContent = (data.AnalogCurrent[31]).toFixed(2);
    mimic.getElementById("target1_size").textContent = (data.AnalogCurrent[32]).toFixed(0);
    mimic.getElementById("target2_x").textContent = (data.AnalogCurrent[33]).toFixed(2);
    mimic.getElementById("target2_y").textContent = (data.AnalogCurrent[34]).toFixed(2);
    mimic.getElementById("target2_size").textContent = (data.AnalogCurrent[35]).toFixed(0);
    mimic.getElementById("target3_x").textContent = (data.AnalogCurrent[36]).toFixed(2);
    mimic.getElementById("target3_y").textContent = (data.AnalogCurrent[37]).toFixed(2);
    mimic.getElementById("target3_size").textContent = (data.AnalogCurrent[38]).toFixed(0);

    var x = data.AnalogCurrent[36]-t1x;
    var y = data.AnalogCurrent[37]-t1y;

    mimic.getElementById("BlowOut").textContent = (m_per_pix * (x - t3x_fixed)).toFixed(2);

    mimic.getElementById("gLineTarget").setAttributeNS(null, 'transform', "translate("+x+","+y+")");
    mimic.getElementById("lLineTarget").setAttributeNS(null, 'transform', "translate("+x+",0)");

    mimic.getElementById("gTarget1").setAttributeNS(null, 'transform', "translate("+t1x_fixed+","+t1y_fixed+")");
    mimic.getElementById("gTarget2").setAttributeNS(null, 'transform', "translate("+t2x_fixed+","+t2y_fixed+")");
    mimic.getElementById("fromTarget1").setAttributeNS(null, 'x1', t1x_fixed);
    mimic.getElementById("fromTarget1").setAttributeNS(null, 'y1', t1y_fixed);
    mimic.getElementById("fromTarget2").setAttributeNS(null, 'x1', t2x_fixed);
    mimic.getElementById("fromTarget2").setAttributeNS(null, 'y1', t2y_fixed);

    mimic.getElementById("fromTarget1").setAttributeNS(null, 'x2', x);
    mimic.getElementById("fromTarget1").setAttributeNS(null, 'y2', y);
    mimic.getElementById("fromTarget2").setAttributeNS(null, 'x2', x);
    mimic.getElementById("fromTarget2").setAttributeNS(null, 'y2', y);

    if ( data.AnalogCurrent[38] == 0 ) {
        mimic.getElementById("gLineTarget").setAttributeNS(null, 'visibility', 'hidden');
        mimic.getElementById("fromTarget1").setAttributeNS(null, 'visibility', 'hidden');
        mimic.getElementById("fromTarget2").setAttributeNS(null, 'visibility', 'hidden');
    } else {
        mimic.getElementById("gLineTarget").setAttributeNS(null, 'visibility', 'visible');
        mimic.getElementById("fromTarget1").setAttributeNS(null, 'visibility', 'visible');
        mimic.getElementById("fromTarget2").setAttributeNS(null, 'visibility', 'visible');
    }
}
