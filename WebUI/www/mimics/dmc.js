function updateMimic(mimic, data)
{
    //mimic.getElementById("Location").textContent = target;
    //mimic.getElementById("SwA_Amp_Group").setAttributeNS(null, 'visibility', 'hidden');

    mimic.getElementById("SwB_Amp_Group").setAttributeNS(null, 'visibility', 'visible');
    mimic.getElementById("SwA_EFI_Group").setAttributeNS(null, 'visibility', 'hidden');
    mimic.getElementById("SwB_EFI_Group").setAttributeNS(null, 'visibility', 'visible');

    //Analogue inputs
    mimic.getElementById("SwA_Amp_A").textContent = data.AnalogCurrent[0];
    mimic.getElementById("SwA_Amp_B").textContent = data.AnalogCurrent[1];
    mimic.getElementById("SwA_Amp_C").textContent = data.AnalogCurrent[2];
    mimic.getElementById("SwB_Amp_A").textContent = data.AnalogCurrent[3];
    mimic.getElementById("SwB_Amp_B").textContent = data.AnalogCurrent[4];
    mimic.getElementById("SwB_Amp_C").textContent = data.AnalogCurrent[5];
    mimic.getElementById("Voltage_230V_APh").textContent = data.AnalogCurrent[6];
    mimic.getElementById("Voltage_230V_BPh").textContent = data.AnalogCurrent[7];
    mimic.getElementById("Voltage_230V_CPh").textContent = data.AnalogCurrent[8];

    //Binary inputs
    //mimic.getElementById("CommsStatus").setAttributeNS(null, 'visibility', data.BinaryCurrent[] ? 'visible' : 'hidden');
    //mimic.getElementById("DeviceStatus").setAttributeNS(null, 'visibility', data.BinaryCurrent[] ? 'visible' : 'hidden');

    mimic.getElementById("LowBattery").setAttributeNS(null, 'visibility', data.BinaryCurrent[0] ? 'visible' : 'hidden');
    mimic.getElementById("LowGas").setAttributeNS(null, 'visibility', data.BinaryCurrent[1] ? 'visible' : 'hidden');
    mimic.getElementById("LocalControl").setAttributeNS(null, 'visibility', data.BinaryCurrent[2] ? 'visible' : 'hidden');
    mimic.getElementById("SwA_Open").setAttributeNS(null, 'visibility', data.BinaryCurrent[3] ? 'hidden' : 'visible');
    mimic.getElementById("SwA_Position_Open").setAttributeNS(null, 'visibility', data.BinaryCurrent[3] ? 'hidden' : 'visible');
    mimic.getElementById("SwA_Closed").setAttributeNS(null, 'visibility', data.BinaryCurrent[4] ? 'visible' : 'hidden');;
    mimic.getElementById("SwA_Position_Closed").setAttributeNS(null, 'visibility', data.BinaryCurrent[4] ? 'visible' : 'hidden');;
    mimic.getElementById("SwA_Earthed").setAttributeNS(null, 'visibility', data.BinaryCurrent[5] ? 'visible' : 'hidden');;
    mimic.getElementById("SwA_EFI_Flagged").setAttributeNS(null, 'visibility', data.BinaryCurrent[6] ? 'inherit' : 'hidden');;
    mimic.getElementById("SwA_EFI_Unflagged").setAttributeNS(null, 'visibility', data.BinaryCurrent[6] ? 'hidden' : 'inherit');;
    mimic.getElementById("SwB_Open").setAttributeNS(null, 'visibility', data.BinaryCurrent[7] ? 'hidden' : 'visible');
    mimic.getElementById("SwB_Position_Open").setAttributeNS(null, 'visibility', data.BinaryCurrent[7] ? 'hidden' : 'visible');
    mimic.getElementById("SwB_Closed").setAttributeNS(null, 'visibility', data.BinaryCurrent[8] ? 'visible' : 'hidden');;
    mimic.getElementById("SwB_Position_Closed").setAttributeNS(null, 'visibility', data.BinaryCurrent[8] ? 'visible' : 'hidden');;
    mimic.getElementById("SwB_Earthed").setAttributeNS(null, 'visibility', data.BinaryCurrent[9] ? 'visible' : 'hidden');;
    mimic.getElementById("SwB_EFI_Flagged").setAttributeNS(null, 'visibility', data.BinaryCurrent[10] ? 'inherit' : 'hidden');;
    mimic.getElementById("SwB_EFI_Unflagged").setAttributeNS(null, 'visibility', data.BinaryCurrent[10] ? 'hidden' : 'inherit');;
}
