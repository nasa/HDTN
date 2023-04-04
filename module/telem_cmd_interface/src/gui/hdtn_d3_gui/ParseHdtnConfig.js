/**
 * @file ParseHdtnConfig.js
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright © 2021 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Released under the NASA Open Source Agreement (NOSA)
 * See LICENSE.md in the source root directory for more information.
 *
 * @section DESCRIPTION
 *
 * The ParseHdtnConfig library handles all json telemetry received from the backend
 * and keeps the hdtn object up to date so it can be redrawn in the gui.
 */

//https://stackoverflow.com/questions/15900485/correct-way-to-convert-size-in-bytes-to-kb-mb-gb-in-javascript
function formatHumanReadable(num, decimals, unitStr, thousand) {
   if(num == 0) return '0 ' + unitStr;
   var k = thousand, //1024,
       dm = decimals <= 0 ? 0 : decimals || 2,
       sizes = ['', 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y'],
       i = Math.floor(Math.log(num) / Math.log(k));
   return ((num / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i] + unitStr;
}
function ObjToTooltipText(obj) {
    const json = JSON.stringify(obj);
    const unbracketed = json.replace(/[{}]/g, '');
    const unquoted = unbracketed.replace(/"([^"]+)":/g, '$1:'); //https://stackoverflow.com/questions/11233498/json-stringify-without-quotes-on-properties
    const text = unquoted.replace(/[,]/g, '<br />');
    return text;
}
function UpdateStorageTelemetry(paramHdtnConfig, paramStorageTelem) {
    paramHdtnConfig.storageD3Obj.toolTipText = ObjToTooltipText(paramStorageTelem);
    let timestampMilliseconds = paramStorageTelem.timestampMilliseconds;
    let deltaTimestampMilliseconds = 0;
    if(paramHdtnConfig.hasOwnProperty("lastStorageTelemTimestampMilliseconds")) {
        deltaTimestampMilliseconds = timestampMilliseconds - paramHdtnConfig.lastStorageTelemTimestampMilliseconds;
    }
    paramHdtnConfig.lastStorageTelemTimestampMilliseconds = timestampMilliseconds;

    if(!paramHdtnConfig.hasOwnProperty("lastStorageTelemetry")) {
        paramHdtnConfig["lastStorageTelemetry"] = {//rate calculation subset
            //disk to storage wire
            "totalBundlesSentToEgressFromStorageReadFromDisk": 0,
            "totalBundleBytesSentToEgressFromStorageReadFromDisk": 0,
            //part of computation for storage to egress wire (summed with above)
            "totalBundlesSentToEgressFromStorageForwardCutThrough": 0,
            "totalBundleBytesSentToEgressFromStorageForwardCutThrough": 0,
            //storage to egress wire (summed with above)
            "totalBundlesSentToEgressFromStorage": 0,
            "totalBundleBytesSentToEgressFromStorage": 0,
            //storage to disk wire
            "totalBundleWriteOperationsToDisk": 0,
            "totalBundleByteWriteOperationsToDisk": 0,
            //disk erase (to trash)
            "totalBundleEraseOperationsFromDisk": 0,
            "totalBundleByteEraseOperationsFromDisk": 0
        };
    }
    let lastT = paramHdtnConfig["lastStorageTelemetry"];
    let currT = {
        //disk to storage wire
        "totalBundlesSentToEgressFromStorageReadFromDisk": paramStorageTelem.totalBundlesSentToEgressFromStorageReadFromDisk,
        "totalBundleBytesSentToEgressFromStorageReadFromDisk": paramStorageTelem.totalBundleBytesSentToEgressFromStorageReadFromDisk,
        //part of computation for storage to egress wire (summed with above)
        "totalBundlesSentToEgressFromStorageForwardCutThrough": paramStorageTelem.totalBundlesSentToEgressFromStorageForwardCutThrough,
        "totalBundleBytesSentToEgressFromStorageForwardCutThrough": paramStorageTelem.totalBundleBytesSentToEgressFromStorageForwardCutThrough,
        //storage to egress wire (summed with above)
        "totalBundlesSentToEgressFromStorage": paramStorageTelem.totalBundlesSentToEgressFromStorageReadFromDisk
            + paramStorageTelem.totalBundlesSentToEgressFromStorageForwardCutThrough,
        "totalBundleBytesSentToEgressFromStorage": paramStorageTelem.totalBundleBytesSentToEgressFromStorageReadFromDisk
            + paramStorageTelem.totalBundleBytesSentToEgressFromStorageForwardCutThrough,
        //storage to disk wire
        "totalBundleWriteOperationsToDisk": paramStorageTelem.totalBundleWriteOperationsToDisk,
        "totalBundleByteWriteOperationsToDisk": paramStorageTelem.totalBundleByteWriteOperationsToDisk,
        //disk erase (to trash)
        "totalBundleEraseOperationsFromDisk": paramStorageTelem.totalBundleEraseOperationsFromDisk,
        "totalBundleByteEraseOperationsFromDisk": paramStorageTelem.totalBundleByteEraseOperationsFromDisk
    };
    if(deltaTimestampMilliseconds > 1) {
        paramHdtnConfig["storageToDiskRateBitsPerSec"] = (currT["totalBundleByteWriteOperationsToDisk"] - lastT["totalBundleByteWriteOperationsToDisk"]) * 8000.0 / deltaTimestampMilliseconds;
        paramHdtnConfig["storageToDiskRateBundlesPerSec"] = (currT["totalBundleWriteOperationsToDisk"] - lastT["totalBundleWriteOperationsToDisk"]) * 1000.0 / deltaTimestampMilliseconds;
        paramHdtnConfig["diskToStorageRateBitsPerSec"] = (currT["totalBundleBytesSentToEgressFromStorageReadFromDisk"] - lastT["totalBundleBytesSentToEgressFromStorageReadFromDisk"]) * 8000.0 / deltaTimestampMilliseconds;
        paramHdtnConfig["diskToStorageRateBundlesPerSec"] = (currT["totalBundlesSentToEgressFromStorageReadFromDisk"] - lastT["totalBundlesSentToEgressFromStorageReadFromDisk"]) * 1000.0 / deltaTimestampMilliseconds;
        paramHdtnConfig["diskEraseRateBitsPerSec"] = (currT["totalBundleByteEraseOperationsFromDisk"] - lastT["totalBundleByteEraseOperationsFromDisk"]) * 8000.0 / deltaTimestampMilliseconds;
        paramHdtnConfig["diskEraseRateBundlesPerSec"] = (currT["totalBundleEraseOperationsFromDisk"] - lastT["totalBundleEraseOperationsFromDisk"]) * 1000.0 / deltaTimestampMilliseconds;
        paramHdtnConfig["storageToEgressRateBitsPerSec"] = (currT["totalBundleBytesSentToEgressFromStorage"] - lastT["totalBundleBytesSentToEgressFromStorage"]) * 8000.0 / deltaTimestampMilliseconds;
        paramHdtnConfig["storageToEgressRateBundlesPerSec"] = (currT["totalBundlesSentToEgressFromStorage"] - lastT["totalBundlesSentToEgressFromStorage"]) * 1000.0 / deltaTimestampMilliseconds;
    }
    else {
        paramHdtnConfig["storageToDiskRateBitsPerSec"] = 0;
        paramHdtnConfig["storageToDiskRateBundlesPerSec"] = 0;
        paramHdtnConfig["diskToStorageRateBitsPerSec"] = 0;
        paramHdtnConfig["diskToStorageRateBundlesPerSec"] = 0;
        paramHdtnConfig["diskEraseRateBitsPerSec"] = 0;
        paramHdtnConfig["diskEraseRateBundlesPerSec"] = 0;
        paramHdtnConfig["storageToEgressRateBitsPerSec"] = 0;
        paramHdtnConfig["storageToEgressRateBundlesPerSec"] = 0;
    }
    paramHdtnConfig["storageToDiskRateBitsPerSecHumanReadable"] = formatHumanReadable(paramHdtnConfig["storageToDiskRateBitsPerSec"], paramHdtnConfig["numDecimals"], 'bit/s', 1000);
    paramHdtnConfig["storageToDiskRateBundlesPerSecHumanReadable"] = formatHumanReadable(paramHdtnConfig["storageToDiskRateBundlesPerSec"], paramHdtnConfig["numDecimals"], 'Bun/s', 1000);
    paramHdtnConfig["diskToStorageRateBitsPerSecHumanReadable"] = formatHumanReadable(paramHdtnConfig["diskToStorageRateBitsPerSec"], paramHdtnConfig["numDecimals"], 'bit/s', 1000);
    paramHdtnConfig["diskToStorageRateBundlesPerSecHumanReadable"] = formatHumanReadable(paramHdtnConfig["diskToStorageRateBundlesPerSec"], paramHdtnConfig["numDecimals"], 'Bun/s', 1000);
    paramHdtnConfig["diskEraseRateBitsPerSecHumanReadable"] = formatHumanReadable(paramHdtnConfig["diskEraseRateBitsPerSec"], paramHdtnConfig["numDecimals"], 'bit/s', 1000);
    paramHdtnConfig["diskEraseRateBundlesPerSecHumanReadable"] = formatHumanReadable(paramHdtnConfig["diskEraseRateBundlesPerSec"], paramHdtnConfig["numDecimals"], 'Bun/s', 1000);
    paramHdtnConfig["storageToEgressRateBitsPerSecHumanReadable"] = formatHumanReadable(paramHdtnConfig["storageToEgressRateBitsPerSec"], paramHdtnConfig["numDecimals"], 'bit/s', 1000);
    paramHdtnConfig["storageToEgressRateBundlesPerSecHumanReadable"] = formatHumanReadable(paramHdtnConfig["storageToEgressRateBundlesPerSec"], paramHdtnConfig["numDecimals"], 'Bun/s', 1000);

    paramHdtnConfig["lastStorageTelemetry"] = currT;
}

function UpdateActiveInductConnections(paramHdtnConfig, paramActiveInductConnections) {
    paramHdtnConfig.ingressD3Obj.toolTipText = ObjToTooltipText( {
        "timestampMilliseconds": paramActiveInductConnections.timestampMilliseconds,
        "bundleCountEgress": paramActiveInductConnections.bundleCountEgress,
        "bundleCountStorage": paramActiveInductConnections.bundleCountStorage,
        "bundleByteCountEgress": paramActiveInductConnections.bundleByteCountEgress,
        "bundleByteCountStorage": paramActiveInductConnections.bundleByteCountStorage
    });
    let timestampMilliseconds = paramActiveInductConnections.timestampMilliseconds;
    let deltaTimestampMilliseconds = 0;
    if(paramHdtnConfig.hasOwnProperty("lastInductTelemTimestampMilliseconds")) {
        deltaTimestampMilliseconds = timestampMilliseconds - paramHdtnConfig.lastInductTelemTimestampMilliseconds;
    }
    paramHdtnConfig.lastInductTelemTimestampMilliseconds = timestampMilliseconds;

    if(!paramHdtnConfig.hasOwnProperty("lastIngressTelemetry")) {
        paramHdtnConfig["lastIngressTelemetry"] = {
            "bundleCountEgress": 0,
            "bundleCountStorage": 0,
            "bundleByteCountEgress": 0,
            "bundleByteCountStorage": 0
        };
    }
    let lastIt = paramHdtnConfig["lastIngressTelemetry"];
    let currIt = {
        "bundleCountEgress": paramActiveInductConnections.bundleCountEgress,
        "bundleCountStorage": paramActiveInductConnections.bundleCountStorage,
        "bundleByteCountEgress": paramActiveInductConnections.bundleByteCountEgress,
        "bundleByteCountStorage": paramActiveInductConnections.bundleByteCountStorage
    };
    if(deltaTimestampMilliseconds > 1) {
        paramHdtnConfig["ingressToStorageRateBitsPerSec"] = (currIt["bundleByteCountStorage"] - lastIt["bundleByteCountStorage"]) * 8000.0 / deltaTimestampMilliseconds;
        paramHdtnConfig["ingressToStorageRateBundlesPerSec"] = (currIt["bundleCountStorage"] - lastIt["bundleCountStorage"]) * 1000.0 / deltaTimestampMilliseconds;
        paramHdtnConfig["ingressToEgressRateBitsPerSec"] = (currIt["bundleByteCountEgress"] - lastIt["bundleByteCountEgress"]) * 8000.0 / deltaTimestampMilliseconds;
        paramHdtnConfig["ingressToEgressRateBundlesPerSec"] = (currIt["bundleCountEgress"] - lastIt["bundleCountEgress"]) * 1000.0 / deltaTimestampMilliseconds;
    }
    else {
        paramHdtnConfig["ingressToStorageRateBitsPerSec"] = 0;
        paramHdtnConfig["ingressToStorageRateBundlesPerSec"] = 0;
        paramHdtnConfig["ingressToEgressRateBitsPerSec"] = 0;
        paramHdtnConfig["ingressToEgressRateBundlesPerSec"] = 0;
    }
    paramHdtnConfig["ingressToStorageRateBitsPerSecHumanReadable"] = formatHumanReadable(paramHdtnConfig["ingressToStorageRateBitsPerSec"], paramHdtnConfig["numDecimals"], 'bit/s', 1000);
    paramHdtnConfig["ingressToStorageRateBundlesPerSecHumanReadable"] = formatHumanReadable(paramHdtnConfig["ingressToStorageRateBundlesPerSec"], paramHdtnConfig["numDecimals"], 'Bun/s', 1000);
    paramHdtnConfig["ingressToEgressRateBitsPerSecHumanReadable"] = formatHumanReadable(paramHdtnConfig["ingressToEgressRateBitsPerSec"], paramHdtnConfig["numDecimals"], 'bit/s', 1000);
    paramHdtnConfig["ingressToEgressRateBundlesPerSecHumanReadable"] = formatHumanReadable(paramHdtnConfig["ingressToEgressRateBundlesPerSec"], paramHdtnConfig["numDecimals"], 'Bun/s', 1000);

    paramHdtnConfig["lastIngressTelemetry"] = currIt;

    let inductsConfig = paramHdtnConfig["inductsConfig"];
    let inductVector = inductsConfig["inductVector"];
    inductVector.forEach(function(ind, i) {
        //console.log(i);
        let inductTelem = paramActiveInductConnections.allInducts[i];
        let activeConnectionsArray = [];
        let mapConnectionNameToD3Obj = ind["mapConnectionNameToD3Obj"];
        inductTelem.inductConnections.forEach(function(connTelem) {
            activeConnectionsArray.push(connTelem.connectionName);
            if(!mapConnectionNameToD3Obj.hasOwnProperty(connTelem.connectionName)) {
                mapConnectionNameToD3Obj[connTelem.connectionName] = {
                    "lastTotalBundlesReceived": 0,
                    "lastTotalBundleBytesReceived": 0,
                    "rateBitsPerSec": 0,
                    "rateBundlesPerSec": 0
                };
            }
            let d3Obj = mapConnectionNameToD3Obj[connTelem.connectionName];
            d3Obj.toolTipText = ObjToTooltipText(connTelem);
            d3Obj["lastTotalBundlesReceived"] = d3Obj["totalBundlesReceived"];
            d3Obj["lastTotalBundleBytesReceived"] = d3Obj["totalBundleBytesReceived"];
            d3Obj["totalBundlesReceived"] = connTelem.totalBundlesReceived;
            d3Obj["totalBundleBytesReceived"] = connTelem.totalBundleBytesReceived;
            if(deltaTimestampMilliseconds > 1) {
                d3Obj["rateBitsPerSec"] = (d3Obj["totalBundleBytesReceived"] - d3Obj["lastTotalBundleBytesReceived"]) * 8000.0 / deltaTimestampMilliseconds;
                d3Obj["rateBundlesPerSec"] = (d3Obj["totalBundlesReceived"] - d3Obj["lastTotalBundlesReceived"]) * 1000.0 / deltaTimestampMilliseconds;
            }
            d3Obj["rateBitsPerSecHumanReadable"] = formatHumanReadable(d3Obj["rateBitsPerSec"], paramHdtnConfig["numDecimals"], 'bit/s', 1000);
            d3Obj["rateBundlesPerSecHumanReadable"] = formatHumanReadable(d3Obj["rateBundlesPerSec"], paramHdtnConfig["numDecimals"], 'Bun/s', 1000);
            d3Obj["name"] = connTelem.inputName;
            d3Obj["remoteConnD3Obj"] = {}

        });
        ind["activeConnections"] = activeConnectionsArray;
    });
}
function InitActiveInductConnections(paramHdtnConfig) {
    let inductsConfig = paramHdtnConfig["inductsConfig"];
    let inductVector = inductsConfig["inductVector"];
    inductVector.forEach(function(ind, i) {
        //console.log(i);
        ind["activeConnections"] = [];
        ind["mapConnectionNameToD3Obj"] = {};
    });
}

function UpdateAllOutductCapabilities(paramHdtnConfig, paramAoct) {
    let outductsConfig = paramHdtnConfig["outductsConfig"];
    let outductVector = outductsConfig["outductVector"];
    paramAoct.outductCapabilityTelemetryList.forEach(function(oct) {
        let i = oct.outductArrayIndex;
        let od = outductVector[i];
        if(od.nextHopNodeId != oct.nextHopNodeId) {
            console.log("invalid AOCT");
            return;
        }
        od["finalDestinationEidUris"] = oct.finalDestinationEidsList;
    });
}

function UpdateAllOutductTelemetry(paramHdtnConfig, paramAot) {
    paramHdtnConfig.egressD3Obj.toolTipText = ObjToTooltipText( {
        "timestampMilliseconds": paramAot.timestampMilliseconds,
        "totalBundlesGivenToOutducts": paramAot.totalBundlesGivenToOutducts,
        "totalBundleBytesGivenToOutducts": paramAot.totalBundleBytesGivenToOutducts,
        "totalTcpclBundlesReceived": paramAot.totalTcpclBundlesReceived,
        "totalTcpclBundleBytesReceived": paramAot.totalTcpclBundleBytesReceived,
        "totalStorageToIngressOpportunisticBundles": paramAot.totalStorageToIngressOpportunisticBundles,
        "totalStorageToIngressOpportunisticBundleBytes": paramAot.totalStorageToIngressOpportunisticBundleBytes,
        "totalBundlesSuccessfullySent": paramAot.totalBundlesSuccessfullySent,
        "totalBundleBytesSuccessfullySent": paramAot.totalBundleBytesSuccessfullySent
    });
    let timestampMilliseconds = paramAot.timestampMilliseconds;
    let deltaTimestampMilliseconds = 0;
    if(paramHdtnConfig.hasOwnProperty("lastOutductTelemTimestampMilliseconds")) {
        deltaTimestampMilliseconds = timestampMilliseconds - paramHdtnConfig.lastOutductTelemTimestampMilliseconds;
    }
    paramHdtnConfig.lastOutductTelemTimestampMilliseconds = timestampMilliseconds;

    if(!paramHdtnConfig.hasOwnProperty("lastEgressTelemetry")) {
        paramHdtnConfig["lastEgressTelemetry"] = {
            "totalBundlesGivenToOutducts": 0,
            "totalBundleBytesGivenToOutducts": 0,
            "totalTcpclBundlesReceived": 0,
            "totalTcpclBundleBytesReceived": 0,
            "totalStorageToIngressOpportunisticBundles": 0,
            "totalStorageToIngressOpportunisticBundleBytes": 0,
            "totalBundlesSuccessfullySent": 0,
            "totalBundleBytesSuccessfullySent": 0
        };
    }
    let lastIt = paramHdtnConfig["lastEgressTelemetry"];
    let currIt = {
        "totalBundlesGivenToOutducts": paramAot.totalBundlesGivenToOutducts,
        "totalBundleBytesGivenToOutducts": paramAot.totalBundleBytesGivenToOutducts,
        "totalTcpclBundlesReceived": paramAot.totalTcpclBundlesReceived,
        "totalTcpclBundleBytesReceived": paramAot.totalTcpclBundleBytesReceived,
        "totalStorageToIngressOpportunisticBundles": paramAot.totalStorageToIngressOpportunisticBundles,
        "totalStorageToIngressOpportunisticBundleBytes": paramAot.totalStorageToIngressOpportunisticBundleBytes,
        "totalBundlesSuccessfullySent": paramAot.totalBundlesSuccessfullySent,
        "totalBundleBytesSuccessfullySent": paramAot.totalBundleBytesSuccessfullySent
    };
    if(deltaTimestampMilliseconds > 1) {
        paramHdtnConfig["egressToIngressRateBitsPerSec"] =
            ((currIt["totalTcpclBundleBytesReceived"] - lastIt["totalTcpclBundleBytesReceived"])
            + (currIt["totalStorageToIngressOpportunisticBundleBytes"] - lastIt["totalStorageToIngressOpportunisticBundleBytes"])) * 8000.0 / deltaTimestampMilliseconds;
        paramHdtnConfig["egressToIngressRateBundlesPerSec"] =
            ((currIt["totalTcpclBundlesReceived"] - lastIt["totalTcpclBundlesReceived"])
            + (currIt["totalStorageToIngressOpportunisticBundles"] - lastIt["totalStorageToIngressOpportunisticBundles"])) * 1000.0 / deltaTimestampMilliseconds;
    }
    else {
        paramHdtnConfig["egressToIngressRateBitsPerSec"] = 0;
        paramHdtnConfig["egressToIngressRateBundlesPerSec"] = 0;
    }
    paramHdtnConfig["egressToIngressRateBitsPerSecHumanReadable"] = formatHumanReadable(paramHdtnConfig["egressToIngressRateBitsPerSec"], paramHdtnConfig["numDecimals"], 'bit/s', 1000);
    paramHdtnConfig["egressToIngressRateBundlesPerSecHumanReadable"] = formatHumanReadable(paramHdtnConfig["egressToIngressRateBundlesPerSec"], paramHdtnConfig["numDecimals"], 'Bun/s', 1000);

    paramHdtnConfig["lastEgressTelemetry"] = currIt;

    let outductsConfig = paramHdtnConfig["outductsConfig"];
    let outductVector = outductsConfig["outductVector"];
    paramAot.allOutducts.forEach(function(outductTelem, i) {
        let od = outductVector[i];
        if(od.convergenceLayer != outductTelem.convergenceLayer) {
            console.log("invalid AOT");
            return;
        }
        od.toolTipText = ObjToTooltipText(outductTelem);
        if(!od.hasOwnProperty("outductPreviousTelem")) {
            od["outductPreviousTelem"] = outductTelem;
            od["outductTelem"] = outductTelem;
            od.outductPreviousTelem["totalBundleBytesAcked"] = 0;
            od.outductPreviousTelem["totalBundlesAcked"] = 0;
            return;
        }
        od["outductPreviousTelem"] = od["outductTelem"];
        od["outductTelem"] = outductTelem;
        if(deltaTimestampMilliseconds > 1) {
            od["rateBitsPerSec"] = (od.outductTelem["totalBundleBytesAcked"] - od.outductPreviousTelem["totalBundleBytesAcked"]) * 8000.0 / deltaTimestampMilliseconds;
            od["rateBundlesPerSec"] = (od.outductTelem["totalBundlesAcked"] - od.outductPreviousTelem["totalBundlesAcked"]) * 1000.0 / deltaTimestampMilliseconds;
        }
        od["rateBitsPerSecHumanReadable"] = formatHumanReadable(od["rateBitsPerSec"], paramHdtnConfig["numDecimals"], 'bit/s', 1000);
        od["rateBundlesPerSecHumanReadable"] = formatHumanReadable(od["rateBundlesPerSec"], paramHdtnConfig["numDecimals"], 'Bun/s', 1000);


    });
}

function ParseHdtnConfig(paramWireConnectionsOldMap, paramHdtnOldDrawHash, paramHdtnConfig, paramDeclutter, paramShrink, PARAM_ABS_POSITION_MAP, fontSizePx, numDecimals) {
    paramHdtnConfig.numDecimals = numDecimals;

    function GetDrawHash(receivedConfig) {
        let hashStr = "fontSizePx=" + fontSizePx + " | ";
        hashStr += "numDecimals=" + numDecimals + " | ";
        if(receivedConfig != null) {

            let inductsConfig = receivedConfig["inductsConfig"];
            let inductVector = inductsConfig["inductVector"];
            inductVector.forEach(function(ind, i) {
                //console.log(i);
                hashStr += "induct=" + ind.convergenceLayer + "[" + i + "](" + ind["activeConnections"].toString() + ");";
            });

            let outductsConfig = receivedConfig["outductsConfig"];
            let outductVector = outductsConfig["outductVector"];
            outductVector.forEach(function(od, i) {
                hashStr += "outduct=" + od.convergenceLayer + "[" + i + "]->" + od.nextHopNodeId + "(" + od["finalDestinationEidUris"].toString() + ") ";
                hashStr += "linkIsUpPhysically=" + (od.hasOwnProperty("outductTelem") ? od.outductTelem.linkIsUpPhysically : false);
                hashStr += "linkIsUpPerTimeSchedule=" + (od.hasOwnProperty("outductTelem") ? od.outductTelem.linkIsUpPerTimeSchedule : false) + ";";
            });
        }

        return hashStr;
    }

    wireConnections = [];

    function AddWire(src, dest, groupName, isMiddleTextLayout) {
        //preserve pathWithoutJumps (stored within wire obj)
        const wireId = src.id + "_" + dest.id;
        let wireObj = {};
        if(paramWireConnectionsOldMap.hasOwnProperty(wireId)) {
            wireObj = paramWireConnectionsOldMap[wireId];
        }
        wireObj.src = src;
        wireObj.dest = dest;
        wireObj.groupName = groupName;
        wireObj.isMiddleTextLayout = isMiddleTextLayout;
        paramWireConnectionsOldMap[wireId] = wireObj;

        wireConnections.push(wireObj);
    }

    let newHashStr = GetDrawHash(paramHdtnConfig);
    let needsRedraw = (paramHdtnOldDrawHash.strVal == null) || (paramHdtnOldDrawHash.strVal !== newHashStr);
    if(!needsRedraw) {
        //console.log("does not need redraw");
        return null;//todo
    }
    else {
        //console.log("needs redraw");
        paramHdtnOldDrawHash.strVal = newHashStr;
    }

    var TEXT_WIRE_OFFSET_PX = 7; //currently hardcoded in GetWireTextTransform of WireComponents.js
    //draw this in a separate svg outside the viewbox so getBoundingClientRect() won't return scaled results
    let svgMeas = d3.select("#hiddenTextMeasurementDiv").append('svg')
        .attr('x', -1000)
        .attr('y', -1000);

    const decimalToStringArray = ["", ".0", ".00"];
    let svgMeasText = svgMeas.append("svg:text")
        .text("300" + decimalToStringArray[numDecimals] + " Kbit/s" + "\u00A0\u00A0" + "300" + decimalToStringArray[numDecimals] + " KBun/s");
    const singleLineWidth = svgMeas.select('text').node().getBoundingClientRect().width + (2 * TEXT_WIRE_OFFSET_PX);
    svgMeasText.text("300" + decimalToStringArray[numDecimals] + " KBun/s");
    const doubleLineWidth = svgMeas.select('text').node().getBoundingClientRect().width + (2 * TEXT_WIRE_OFFSET_PX);
    const emptyLineWidth = 100;
    // cleanup
    svgMeas.remove();

    var BUSBAR_WIDTH_PX = 5;

    var TEXT_VERTICAL_MARGIN = 2;
    var CHILD_HEIGHT_PX = fontSizePx + (2 * TEXT_VERTICAL_MARGIN);
    var CHILD_BOTTOM_MARGIN_PX = fontSizePx/2.0;
    var CHILD_SIDE_MARGIN_PX = 5;
    var PARENT_TOP_HEADER_PX = fontSizePx + (2 * TEXT_VERTICAL_MARGIN);
    var PARENT_GROUP_VERTICAL_SPACING_PX = fontSizePx + (2 * TEXT_VERTICAL_MARGIN);


    //Initialization




    var storageAbsPosition = PARAM_ABS_POSITION_MAP["storage"];
    if(!paramHdtnConfig.hasOwnProperty("storageD3Obj")) {
        paramHdtnConfig.storageD3Obj = {
            "parent": null,
            //d3ChildArray = []; //keep undefined
            "id": "storage",
            "name": "Storage",
            "absX": storageAbsPosition.X,
            "absY": storageAbsPosition.Y,
            "width": storageAbsPosition.WIDTH,
            "height": storageAbsPosition.HEIGHT,
            "topHeaderHeight": PARENT_TOP_HEADER_PX
        };
    }
    var storageD3Array = [paramHdtnConfig.storageD3Obj];


    var ingressAbsPosition = PARAM_ABS_POSITION_MAP["ingress"];
    if(!paramHdtnConfig.hasOwnProperty("ingressD3Obj")) {
        paramHdtnConfig.ingressD3Obj = {
            "parent": null,
            "d3ChildArray": [], //not used, just drawing inducts over top manually
            "id": "ingress",
            "name": "Ingress",
            "absX": ingressAbsPosition.X,
            "absY": ingressAbsPosition.Y,
            "width": ingressAbsPosition.WIDTH,
            "height": ingressAbsPosition.HEIGHT,
            "topHeaderHeight": PARENT_TOP_HEADER_PX
        };
    }
    var ingressD3Array = [paramHdtnConfig.ingressD3Obj];


    var activeInductConnsAbsPosition = PARAM_ABS_POSITION_MAP["active_induct_conns"];
    var activeInductConnsObj = {};
    activeInductConnsObj.parent = null;
    activeInductConnsObj.d3ChildArray = [];
    activeInductConnsObj.id = "final_dests"
    activeInductConnsObj.absX = (ingressAbsPosition.X - singleLineWidth) - activeInductConnsAbsPosition.WIDTH;
    activeInductConnsObj.absY = activeInductConnsAbsPosition.Y;
    activeInductConnsObj.width = activeInductConnsAbsPosition.WIDTH;
    activeInductConnsObj.height = 500;
    var activeInductConnsD3Array = [activeInductConnsObj];


    var inductsConfig = paramHdtnConfig["inductsConfig"];
    var inductVector = inductsConfig["inductVector"];

    var inductRelYLeft =  PARENT_TOP_HEADER_PX;// + CHILD_HEIGHT_PX/2;
    var activeInductConnsRelY = PARENT_TOP_HEADER_PX;

    //var indAbsPosition = PARAM_ABS_POSITION_MAP["next_hops"];
    var indAbsPositionY = paramHdtnConfig.ingressD3Obj.absY + PARENT_TOP_HEADER_PX;
    var inductsD3Array = [];

    inductVector.forEach(function(ind, i) {
        //console.log(i);
        let mapConnectionNameToD3Obj = ind["mapConnectionNameToD3Obj"];

        ///////////induct (using the hdtn config object itself to store d3 info)
        ind.topHeaderHeight = PARENT_TOP_HEADER_PX;
        ind.parent = null;
        ind.d3ChildArray = [];
        ind.id = "induct_" + i;
        let cvName = "??";
        if(ind.convergenceLayer === "ltp_over_udp") {
            cvName = "LTP";
        }
        else if(ind.convergenceLayer === "udp") {
            cvName = "UDP";
        }
        else if(ind.convergenceLayer === "tcpcl_v3") {
            cvName = "TCP3";
        }
        else if(ind.convergenceLayer === "tcpcl_v4") {
            cvName = "TCP4";
        }
        else if(ind.convergenceLayer === "stcp") {
            cvName = "STCP";
        }
        ind.name = cvName + "[" + i + "]";
        ind.absX = paramHdtnConfig.ingressD3Obj.absX + CHILD_SIDE_MARGIN_PX;
        ind.absY = indAbsPositionY;
        ind.width = (ingressAbsPosition.WIDTH - 2*CHILD_SIDE_MARGIN_PX) * (3/4) - BUSBAR_WIDTH_PX/2;
        inductsD3Array.push(ind);





        var activeInductConnsNames = ind["activeConnections"];
        var inductInputConnectionRelY =  PARENT_TOP_HEADER_PX;// + CHILD_HEIGHT_PX/2;
        ind.height = inductInputConnectionRelY;
        activeInductConnsNames.forEach(function(connName, j) {

            ///////////induct connection input port
            if(!mapConnectionNameToD3Obj.hasOwnProperty(connName)) {
                return;
            }
            let inductInputConnectionObj = mapConnectionNameToD3Obj[connName];
            inductInputConnectionObj.linkIsUp = true;
            inductInputConnectionObj.parent = ind;
            inductInputConnectionObj.id = "induct_conn_" + ind.id + "_" + connName;
            //inductInputConnectionObj.name = ""; //already set

            inductInputConnectionObj.width = ind.width - 2*CHILD_SIDE_MARGIN_PX;
            inductInputConnectionObj.height = CHILD_HEIGHT_PX;

            inductInputConnectionObj.relX = CHILD_SIDE_MARGIN_PX;
            inductInputConnectionObj.relY = inductInputConnectionRelY;
            inductInputConnectionRelY += CHILD_HEIGHT_PX + CHILD_BOTTOM_MARGIN_PX;
            ind.height = inductInputConnectionRelY;

            inductInputConnectionObj.absWireOutY = ind.absY + inductInputConnectionObj.relY + inductInputConnectionObj.height/2;
            inductInputConnectionObj.absWireInY = inductInputConnectionObj.absWireOutY;
            inductInputConnectionObj.absWireOutX = ind.absX + inductInputConnectionObj.relX + inductInputConnectionObj.width;
            inductInputConnectionObj.absWireInX = ind.absX + inductInputConnectionObj.relX;


            ind.d3ChildArray.push(inductInputConnectionObj);

            if(connName == "null") { //reserved so tcp connections can show bound port but no connection
                return;
            }



            var baseId = "induct_" + i + "_conn_" + connName;// + "_" + j; //don't use j, will affect redraw on deletion




            ////////////////active induct connections
            let connObj = inductInputConnectionObj.remoteConnD3Obj;
            connObj.linkIsUp = true;
            connObj.parent = activeInductConnsObj;
            connObj.id = "conn_" + baseId;
            connObj.name = connName;

            connObj.width = (activeInductConnsAbsPosition.WIDTH - 2*CHILD_SIDE_MARGIN_PX);
            connObj.height = CHILD_HEIGHT_PX;
            connObj.relX = CHILD_SIDE_MARGIN_PX;
            connObj.relY = inductInputConnectionObj.relY + ind.absY - paramHdtnConfig.ingressD3Obj.absY;
            activeInductConnsRelY += CHILD_HEIGHT_PX + CHILD_BOTTOM_MARGIN_PX;


            connObj.absWireOutY = activeInductConnsObj.absY + connObj.relY + connObj.height/2;
            connObj.absWireInY = connObj.absWireOutY;
            connObj.absWireOutX = activeInductConnsObj.absX + connObj.relX + connObj.width;
            connObj.absWireInX = activeInductConnsObj.absX + connObj.relX;

            activeInductConnsObj.d3ChildArray.push(connObj);

            AddWire(connObj, inductInputConnectionObj, "conn_induct", false);
        });

        indAbsPositionY += ind.height + PARENT_GROUP_VERTICAL_SPACING_PX;

    });

    //obj.height = Math.max(subObjRelYLeft, subObjRelYRight);

    activeInductConnsObj.topHeaderHeight = PARENT_TOP_HEADER_PX;

    paramHdtnConfig.ingressD3Obj.busBar = {
        "x1": ingressAbsPosition.WIDTH * 3.0 / 4.0,
        "y1": PARENT_TOP_HEADER_PX,
        "x2": ingressAbsPosition.WIDTH * 3.0 / 4.0,
        "y2": PARENT_TOP_HEADER_PX + ingressAbsPosition.HEIGHT - PARENT_TOP_HEADER_PX - CHILD_BOTTOM_MARGIN_PX
    };




    var egressAbsPosition = PARAM_ABS_POSITION_MAP["egress"];
    if(!paramHdtnConfig.hasOwnProperty("egressD3Obj")) {
        paramHdtnConfig.egressD3Obj = {
            "parent": null,
            "d3ChildArray": [],
            "id": "egress",
            "name": "Egress",
            "absX": egressAbsPosition.X,
            "absY": egressAbsPosition.Y,
            "width": egressAbsPosition.WIDTH,
            "height": egressAbsPosition.HEIGHT,
            "topHeaderHeight": PARENT_TOP_HEADER_PX
        };
    }
    var egressD3Array = [paramHdtnConfig.egressD3Obj];
    paramHdtnConfig.egressD3Obj.d3ChildArray = []; //clear array before repushing outduct objects to it!

    var nextHopsAbsPosition = PARAM_ABS_POSITION_MAP["next_hops"];
    var nextHopsAbsPositionY = nextHopsAbsPosition.Y;
    const nextHopsAbsPositionX = egressAbsPosition.X + egressAbsPosition.WIDTH + doubleLineWidth;
    var nextHopsD3Array = [];



    var finalDestsAbsPosition = PARAM_ABS_POSITION_MAP["final_dests"];
    if(!paramHdtnConfig.hasOwnProperty("finalDestsD3Obj")) { //parent is invisible
        paramHdtnConfig.finalDestsD3Obj = {
            "parent": null,
            "d3ChildArray": [],
            "id": "final_dests",
            "name": "",
            "absX": 0, //not constant (set below)
            "absY": finalDestsAbsPosition.Y,
            "width": finalDestsAbsPosition.WIDTH,
            "height": 500,
            "topHeaderHeight": PARENT_TOP_HEADER_PX
        };
    }
    paramHdtnConfig.finalDestsD3Obj.absX = nextHopsAbsPositionX + nextHopsAbsPosition.WIDTH + emptyLineWidth;
    var finalDestsD3Array = [paramHdtnConfig.finalDestsD3Obj];
    paramHdtnConfig.finalDestsD3Obj.d3ChildArray = []; //clear array before repushing objects to it!

    var outductsConfig = paramHdtnConfig["outductsConfig"];
    var outductVector = outductsConfig["outductVector"];
    //console.log(outductVector);



    //var outductRelYRight = PARENT_TOP_HEADER_PX;


    var finalDestRelY = PARENT_TOP_HEADER_PX;
    var finalDestRelYArray = [];
    var finalDestRelYCompactArray = [];

    outductVector.forEach(function(outduct, i) {
        //console.log(i);

        ///////////outduct
        const outductLinkUpPhysically = (outduct.hasOwnProperty("outductTelem") ? outduct.outductTelem.linkIsUpPhysically : false);
        outduct.linkIsUp = (outduct.hasOwnProperty("outductTelem") ? outduct.outductTelem.linkIsUpPerTimeSchedule : false); //for class color of child object
        outduct.parent = paramHdtnConfig.egressD3Obj;
        outduct.id = "outduct_" + i;
        var cvName = "??";
        if(outduct.convergenceLayer === "ltp_over_udp") {
            cvName = "LTP";
        }
        else if(outduct.convergenceLayer === "udp") {
            cvName = "UDP";
        }
        else if(outduct.convergenceLayer === "tcpcl_v3") {
            cvName = "TCP3";
        }
        else if(outduct.convergenceLayer === "tcpcl_v4") {
            cvName = "TCP4";
        }
        else if(outduct.convergenceLayer === "stcp") {
            cvName = "STCP";
        }
        outduct.name = cvName + "[" + i + "]";

        outduct.width = (egressAbsPosition.WIDTH - 2*CHILD_SIDE_MARGIN_PX) * (3/4) - BUSBAR_WIDTH_PX/2;
        outduct.height = CHILD_HEIGHT_PX;
        //right side
        outduct.relX = CHILD_SIDE_MARGIN_PX + (egressAbsPosition.WIDTH - 2*CHILD_SIDE_MARGIN_PX) * (1/4) + BUSBAR_WIDTH_PX/2;
        //outduct.relY = outductRelYRight;
        //outductRelYRight += CHILD_HEIGHT_PX + CHILD_BOTTOM_MARGIN_PX;

        //outduct.absWireOutY = paramHdtnConfig.egressD3Obj.absY + outduct.relY + outduct.height/2;
        //outduct.absWireInY = outduct.absWireOutY;
        outduct.absWireOutX = paramHdtnConfig.egressD3Obj.absX + outduct.relX + outduct.width;
        outduct.absWireInX = paramHdtnConfig.egressD3Obj.absX + outduct.relX;



        //console.log(outduct);
        paramHdtnConfig.egressD3Obj.d3ChildArray.push(outduct);

        ///////////next hop
        var nextHopObj = {};
        nextHopObj.topHeaderHeight = PARENT_TOP_HEADER_PX;
        nextHopObj.parent = null;
        nextHopObj.linkIsUp = true;
        nextHopObj.d3ChildArray = [];
        nextHopObj.id = "next_hop_node_id_" + outduct.nextHopNodeId;
        nextHopObj.name = "Node " + outduct.nextHopNodeId;
        nextHopObj.nodeId = outduct.nextHopNodeId;
        nextHopObj.absX = nextHopsAbsPositionX;
        nextHopObj.absY = nextHopsAbsPositionY;
        nextHopObj.width = nextHopsAbsPosition.WIDTH;
        nextHopsD3Array.push(nextHopObj);




        var finalDestinationEidUris = outduct["finalDestinationEidUris"];
        var nextHopRelY =  PARENT_TOP_HEADER_PX;// + CHILD_HEIGHT_PX/2;
        finalDestinationEidUris.forEach(function(fd, j) {

            ///////////next hop out port
            var nextHopOutPort = {};
            nextHopOutPort.linkIsUp = true;
            nextHopOutPort.parent = nextHopObj;
            nextHopOutPort.id = "next_hop_" + fd;
            nextHopOutPort.name = "";

            nextHopOutPort.width = (nextHopsAbsPosition.WIDTH - 2*CHILD_SIDE_MARGIN_PX) * (1/2) - BUSBAR_WIDTH_PX/2;
            nextHopOutPort.height = CHILD_HEIGHT_PX;

            nextHopOutPort.relX = CHILD_SIDE_MARGIN_PX + (nextHopsAbsPosition.WIDTH - 2*CHILD_SIDE_MARGIN_PX) * (1/2) + BUSBAR_WIDTH_PX/2;
            nextHopOutPort.relY = nextHopRelY;
            nextHopRelY += CHILD_HEIGHT_PX + CHILD_BOTTOM_MARGIN_PX;
            nextHopObj.height = nextHopRelY;

            nextHopOutPort.absWireOutY = nextHopObj.absY + nextHopOutPort.relY + nextHopOutPort.height/2;
            nextHopOutPort.absWireInY = nextHopOutPort.absWireOutY;
            nextHopOutPort.absWireOutX = nextHopObj.absX + nextHopOutPort.relX + nextHopOutPort.width;
            nextHopOutPort.absWireInX = nextHopObj.absX + nextHopOutPort.relX;


            //console.log(nextHopOutPort);
            nextHopObj.d3ChildArray.push(nextHopOutPort);
            finalDestRelYArray.push(nextHopObj.absY + nextHopOutPort.relY - finalDestsAbsPosition.Y);
            finalDestRelYCompactArray.push(finalDestRelY);
            finalDestRelY += CHILD_HEIGHT_PX + CHILD_BOTTOM_MARGIN_PX;

            ////////////////final dests
            //console.log(fd);
            var finalDest = {};
            finalDest.linkIsUp = true;
            finalDest.parent = paramHdtnConfig.finalDestsD3Obj;
            finalDest.id = "fd_" + fd;
            finalDest.name = fd;
            paramHdtnConfig.finalDestsD3Obj.d3ChildArray.push(finalDest);

            AddWire(nextHopOutPort, finalDest, "nextHop_finalDest", false);
        });


        nextHopObj.busBar = {
            "x1": nextHopsAbsPosition.WIDTH / 2.0,
            "y1": PARENT_TOP_HEADER_PX,
            "x2": nextHopsAbsPosition.WIDTH / 2.0,
            "y2": PARENT_TOP_HEADER_PX + nextHopObj.height - PARENT_TOP_HEADER_PX - CHILD_BOTTOM_MARGIN_PX
        };


        nextHopObj.absWireOutY = nextHopsAbsPositionY + nextHopObj.height/2;
        nextHopObj.absWireInY = nextHopObj.absWireOutY;
        nextHopObj.absWireOutX = nextHopObj.absX + nextHopObj.width;
        nextHopObj.absWireInX = nextHopObj.absX;

        nextHopsAbsPositionY += nextHopObj.height + PARENT_GROUP_VERTICAL_SPACING_PX;

        //line up outduct perfectly with center of next hop
        outduct.relY = (nextHopObj.absWireInY - (outduct.height/2)) - paramHdtnConfig.egressD3Obj.absY;
        outduct.absWireOutY = paramHdtnConfig.egressD3Obj.absY + outduct.relY + outduct.height/2;
        outduct.absWireInY = outduct.absWireOutY;

        if(/*!paramDeclutter ||*/ outductLinkUpPhysically) { //if cluttered or on
            AddWire(outduct, nextHopObj, "outduct_nextHop", true);
        }


    });

    let relYArray = finalDestRelYArray;
    if(false) { //experiment with wire jumpovers
        function compare( a, b ) {
            if ( a.name < b.name ) {
                return -1;
            }
            if ( a.name > b.name ) {
                return 1;
            }
            return 0;
        }

        paramHdtnConfig.finalDestsD3Obj.d3ChildArray.sort(compare);
        relYArray = finalDestRelYCompactArray;
    }

    paramHdtnConfig.finalDestsD3Obj.d3ChildArray.forEach(function(finalDest, i) {
        finalDest.width = (finalDestsAbsPosition.WIDTH - 2*CHILD_SIDE_MARGIN_PX);
        finalDest.height = CHILD_HEIGHT_PX;
        finalDest.relX = CHILD_SIDE_MARGIN_PX;
        finalDest.relY = relYArray[i]; //nextHopObj.absY + nextHopOutPort.relY - finalDestsAbsPosition.Y;


        finalDest.absWireOutY = paramHdtnConfig.finalDestsD3Obj.absY + finalDest.relY + finalDest.height/2;
        finalDest.absWireInY = finalDest.absWireOutY;
        finalDest.absWireOutX = paramHdtnConfig.finalDestsD3Obj.absX + finalDest.relX + finalDest.width;
        finalDest.absWireInX = paramHdtnConfig.finalDestsD3Obj.absX + finalDest.relX;




    });

    //obj.height = Math.max(subObjRelYLeft, subObjRelYRight);


    paramHdtnConfig.egressD3Obj.busBar = {
        "x1": egressAbsPosition.WIDTH / 4.0,
        "y1": PARENT_TOP_HEADER_PX,
        "x2": egressAbsPosition.WIDTH / 4.0,
        "y2": PARENT_TOP_HEADER_PX + egressAbsPosition.HEIGHT - PARENT_TOP_HEADER_PX - CHILD_BOTTOM_MARGIN_PX
    };

    return {
        "activeInductConnsD3Array": activeInductConnsD3Array,
        "ingressD3Array": ingressD3Array,
        "inductsD3Array": inductsD3Array,
        "egressD3Array": egressD3Array,
        "storageD3Array": storageD3Array,
        "nextHopsD3Array": nextHopsD3Array,
        "finalDestsD3Array": finalDestsD3Array,
        "wireConnections": wireConnections
    };
}
