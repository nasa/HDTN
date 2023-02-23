//https://stackoverflow.com/questions/15900485/correct-way-to-convert-size-in-bytes-to-kb-mb-gb-in-javascript
function formatHumanReadable(num, decimals, unitStr) {
   if(num == 0) return '0 ' + unitStr;
   var k = 1024,
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
            "totalBundleByteWriteOperationsToDisk": 0
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
        "totalBundleByteWriteOperationsToDisk": paramStorageTelem.totalBundleByteWriteOperationsToDisk
    };
    if(deltaTimestampMilliseconds > 1) {
        paramHdtnConfig["storageToDiskRateBitsPerSec"] = (currT["totalBundleByteWriteOperationsToDisk"] - lastT["totalBundleByteWriteOperationsToDisk"]) * 8000.0 / deltaTimestampMilliseconds;
        paramHdtnConfig["storageToDiskRateBundlesPerSec"] = (currT["totalBundleWriteOperationsToDisk"] - lastT["totalBundleWriteOperationsToDisk"]) * 1000.0 / deltaTimestampMilliseconds;
        paramHdtnConfig["diskToStorageRateBitsPerSec"] = (currT["totalBundleBytesSentToEgressFromStorageReadFromDisk"] - lastT["totalBundleBytesSentToEgressFromStorageReadFromDisk"]) * 8000.0 / deltaTimestampMilliseconds;
        paramHdtnConfig["diskToStorageRateBundlesPerSec"] = (currT["totalBundlesSentToEgressFromStorageReadFromDisk"] - lastT["totalBundlesSentToEgressFromStorageReadFromDisk"]) * 1000.0 / deltaTimestampMilliseconds;
        paramHdtnConfig["storageToEgressRateBitsPerSec"] = (currT["totalBundleBytesSentToEgressFromStorage"] - lastT["totalBundleBytesSentToEgressFromStorage"]) * 8000.0 / deltaTimestampMilliseconds;
        paramHdtnConfig["storageToEgressRateBundlesPerSec"] = (currT["totalBundlesSentToEgressFromStorage"] - lastT["totalBundlesSentToEgressFromStorage"]) * 1000.0 / deltaTimestampMilliseconds;
    }
    else {
        paramHdtnConfig["storageToDiskRateBitsPerSec"] = 0;
        paramHdtnConfig["storageToDiskRateBundlesPerSec"] = 0;
        paramHdtnConfig["diskToStorageRateBitsPerSec"] = 0;
        paramHdtnConfig["diskToStorageRateBundlesPerSec"] = 0;
        paramHdtnConfig["storageToEgressRateBitsPerSec"] = 0;
        paramHdtnConfig["storageToEgressRateBundlesPerSec"] = 0;
    }
    paramHdtnConfig["storageToDiskRateBitsPerSecHumanReadable"] = formatHumanReadable(paramHdtnConfig["storageToDiskRateBitsPerSec"], 2, 'bit/s');
    paramHdtnConfig["storageToDiskRateBundlesPerSecHumanReadable"] = formatHumanReadable(paramHdtnConfig["storageToDiskRateBundlesPerSec"], 2, 'Bun/s');
    paramHdtnConfig["diskToStorageRateBitsPerSecHumanReadable"] = formatHumanReadable(paramHdtnConfig["diskToStorageRateBitsPerSec"], 2, 'bit/s');
    paramHdtnConfig["diskToStorageRateBundlesPerSecHumanReadable"] = formatHumanReadable(paramHdtnConfig["diskToStorageRateBundlesPerSec"], 2, 'Bun/s');
    paramHdtnConfig["storageToEgressRateBitsPerSecHumanReadable"] = formatHumanReadable(paramHdtnConfig["storageToEgressRateBitsPerSec"], 2, 'bit/s');
    paramHdtnConfig["storageToEgressRateBundlesPerSecHumanReadable"] = formatHumanReadable(paramHdtnConfig["storageToEgressRateBundlesPerSec"], 2, 'Bun/s');

    paramHdtnConfig["lastStorageTelemetry"] = currT;
}

function UpdateActiveInductConnections(paramHdtnConfig, paramActiveInductConnections) {
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
    paramHdtnConfig["ingressToStorageRateBitsPerSecHumanReadable"] = formatHumanReadable(paramHdtnConfig["ingressToStorageRateBitsPerSec"], 2, 'bit/s');
    paramHdtnConfig["ingressToStorageRateBundlesPerSecHumanReadable"] = formatHumanReadable(paramHdtnConfig["ingressToStorageRateBundlesPerSec"], 2, 'Bun/s');
    paramHdtnConfig["ingressToEgressRateBitsPerSecHumanReadable"] = formatHumanReadable(paramHdtnConfig["ingressToEgressRateBitsPerSec"], 2, 'bit/s');
    paramHdtnConfig["ingressToEgressRateBundlesPerSecHumanReadable"] = formatHumanReadable(paramHdtnConfig["ingressToEgressRateBundlesPerSec"], 2, 'Bun/s');

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
            d3Obj["rateBitsPerSecHumanReadable"] = formatHumanReadable(d3Obj["rateBitsPerSec"], 2, 'bit/s');
            d3Obj["rateBundlesPerSecHumanReadable"] = formatHumanReadable(d3Obj["rateBundlesPerSec"], 2, 'Bun/s');
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
    let timestampMilliseconds = paramAot.timestampMilliseconds;
    let deltaTimestampMilliseconds = 0;
    if(paramHdtnConfig.hasOwnProperty("lastOutductTelemTimestampMilliseconds")) {
        deltaTimestampMilliseconds = timestampMilliseconds - paramHdtnConfig.lastOutductTelemTimestampMilliseconds;
    }
    paramHdtnConfig.lastOutductTelemTimestampMilliseconds = timestampMilliseconds;

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
            outductTelem["rateBitsPerSec"] = (od.outductTelem["totalBundleBytesAcked"] - od.outductPreviousTelem["totalBundleBytesAcked"]) * 8000.0 / deltaTimestampMilliseconds;
            outductTelem["rateBundlesPerSec"] = (od.outductTelem["totalBundlesAcked"] - od.outductPreviousTelem["totalBundlesAcked"]) * 1000.0 / deltaTimestampMilliseconds;
        }
        od["rateBitsPerSecHumanReadable"] = formatHumanReadable(outductTelem["rateBitsPerSec"], 2, 'bit/s');
        od["rateBundlesPerSecHumanReadable"] = formatHumanReadable(outductTelem["rateBundlesPerSec"], 2, 'Bun/s');


    });
}

function ParseHdtnConfig(paramWireConnectionsOldMap, paramHdtnOldDrawHash, paramHdtnConfig, paramDeclutter, paramShrink, paramD3FaultsMap, PARAM_MAP_NAMES, PARAM_SUB_MAP_NAMES, PARAM_D3_SHAPE_ATTRIBUTES, PARAM_ABS_POSITION_MAP, PARAM_FLIP_HORIZONTAL_MAP) {

    function GetDrawHash(receivedConfig) {
        let hashStr = "";
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
                hashStr += "outduct=" + od.convergenceLayer + "[" + i + "]->" + od.nextHopNodeId + "(" + od["finalDestinationEidUris"].toString() + ");";
            });
        }

        return hashStr;
    }

    wireConnections = [];

    function AddWire(src, dest, groupName) {
        //preserve pathWithoutJumps (stored within wire obj)
        const wireId = src.id + "_" + dest.id;
        let wireObj = {};
        if(paramWireConnectionsOldMap.hasOwnProperty(wireId)) {
            wireObj = paramWireConnectionsOldMap[wireId];
        }
        wireObj.src = src;
        wireObj.dest = dest;
        wireObj.groupName = groupName;
        paramWireConnectionsOldMap[wireId] = wireObj;

        wireConnections.push(wireObj);
    }

    let newHashStr = GetDrawHash(paramHdtnConfig);
    let needsRedraw = (paramHdtnOldDrawHash.strVal == null) || (paramHdtnOldDrawHash.strVal !== newHashStr);
    if(!needsRedraw) {
        console.log("does not need redraw");
        return null;//todo
    }
    else {
        console.log("needs redraw");
        paramHdtnOldDrawHash.strVal = newHashStr;
    }

    var BUSBAR_WIDTH_PX = 5;

    var CHILD_HEIGHT_PX = 20;
    var CHILD_BOTTOM_MARGIN_PX = 10;
    var CHILD_SIDE_MARGIN_PX = 5;
    var PARENT_TOP_HEADER_PX = 20;
    var PARENT_GROUP_VERTICAL_SPACING_PX = 20;


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
    var ingressObj = {};
    ingressObj.parent = null;
    ingressObj.d3ChildArray = [];
    ingressObj.id = "ingress";
    ingressObj.name = "Ingress";
    ingressObj.absX = ingressAbsPosition.X;
    ingressObj.absY = ingressAbsPosition.Y;
    ingressObj.width = ingressAbsPosition.WIDTH;
    ingressObj.height = ingressAbsPosition.HEIGHT;
    var ingressD3Array = [ingressObj];


    var activeInductConnsAbsPosition = PARAM_ABS_POSITION_MAP["active_induct_conns"];
    var activeInductConnsObj = {};
    activeInductConnsObj.parent = null;
    activeInductConnsObj.d3ChildArray = [];
    activeInductConnsObj.id = "final_dests"
    activeInductConnsObj.absX = activeInductConnsAbsPosition.X;
    activeInductConnsObj.absY = activeInductConnsAbsPosition.Y;
    activeInductConnsObj.width = activeInductConnsAbsPosition.WIDTH;
    activeInductConnsObj.height = 500;
    var activeInductConnsD3Array = [activeInductConnsObj];


    var inductsConfig = paramHdtnConfig["inductsConfig"];
    var inductVector = inductsConfig["inductVector"];

    var inductRelYLeft =  PARENT_TOP_HEADER_PX;// + CHILD_HEIGHT_PX/2;
    var activeInductConnsRelY = PARENT_TOP_HEADER_PX;

    //var indAbsPosition = PARAM_ABS_POSITION_MAP["next_hops"];
    var indAbsPositionY = ingressObj.absY + PARENT_TOP_HEADER_PX;
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
        ind.absX = ingressObj.absX + CHILD_SIDE_MARGIN_PX;
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


            //console.log(nextHop);
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
            connObj.relY = inductInputConnectionObj.relY + ind.absY - ingressObj.absY;
            activeInductConnsRelY += CHILD_HEIGHT_PX + CHILD_BOTTOM_MARGIN_PX;


            connObj.absWireOutY = activeInductConnsObj.absY + connObj.relY + connObj.height/2;
            connObj.absWireInY = connObj.absWireOutY;
            connObj.absWireOutX = activeInductConnsObj.absX + connObj.relX + connObj.width;
            connObj.absWireInX = activeInductConnsObj.absX + connObj.relX;

            activeInductConnsObj.d3ChildArray.push(connObj);

            AddWire(connObj, inductInputConnectionObj, "conn_induct");
        });

        indAbsPositionY += ind.height + PARENT_GROUP_VERTICAL_SPACING_PX;

    });

    //obj.height = Math.max(subObjRelYLeft, subObjRelYRight);
    ingressObj.topHeaderHeight = PARENT_TOP_HEADER_PX;

    activeInductConnsObj.topHeaderHeight = PARENT_TOP_HEADER_PX;

    ingressObj.busBar = {
        "x1": ingressAbsPosition.WIDTH * 3.0 / 4.0,
        "y1": PARENT_TOP_HEADER_PX,
        "x2": ingressAbsPosition.WIDTH * 3.0 / 4.0,
        "y2": PARENT_TOP_HEADER_PX + ingressAbsPosition.HEIGHT - PARENT_TOP_HEADER_PX - CHILD_BOTTOM_MARGIN_PX
    };








    var egressAbsPosition = PARAM_ABS_POSITION_MAP["egress"];
    var egressObj = {};
    egressObj.parent = null;
    egressObj.d3ChildArray = [];
    egressObj.id = "egress";
    egressObj.name = "Egress";
    egressObj.absX = egressAbsPosition.X;
    egressObj.absY = egressAbsPosition.Y;
    egressObj.width = egressAbsPosition.WIDTH;
    egressObj.height = egressAbsPosition.HEIGHT;
    var egressD3Array = [egressObj];

    var nextHopsAbsPosition = PARAM_ABS_POSITION_MAP["next_hops"];
    var nextHopsAbsPositionY = nextHopsAbsPosition.Y;
    var nextHopsD3Array = [];



    var finalDestsAbsPosition = PARAM_ABS_POSITION_MAP["final_dests"];
    var finalDestsObj = {};
    finalDestsObj.parent = null;
    finalDestsObj.d3ChildArray = [];
    finalDestsObj.id = "final_dests"
    finalDestsObj.absX = finalDestsAbsPosition.X;
    finalDestsObj.absY = finalDestsAbsPosition.Y;
    finalDestsObj.width = finalDestsAbsPosition.WIDTH;
    finalDestsObj.height = 500;
    var finalDestsD3Array = [finalDestsObj];

    var outductsConfig = paramHdtnConfig["outductsConfig"];
    var outductVector = outductsConfig["outductVector"];
    //console.log(outductVector);



    var outductRelYRight = PARENT_TOP_HEADER_PX;


    var finalDestRelY = PARENT_TOP_HEADER_PX;

    outductVector.forEach(function(od, i) {
        //console.log(i);

        ///////////outduct
        var outduct = od;
        outduct.linkIsUp = true;
        outduct.parent = egressObj;
        outduct.id = "outduct_" + i;
        var cvName = "??";
        if(od.convergenceLayer === "ltp_over_udp") {
            cvName = "LTP";
        }
        else if(od.convergenceLayer === "udp") {
            cvName = "UDP";
        }
        else if(od.convergenceLayer === "tcpcl_v3") {
            cvName = "TCP3";
        }
        else if(od.convergenceLayer === "tcpcl_v4") {
            cvName = "TCP4";
        }
        else if(od.convergenceLayer === "stcp") {
            cvName = "STCP";
        }
        outduct.name = cvName + "[" + i + "]";

        outduct.width = (egressAbsPosition.WIDTH - 2*CHILD_SIDE_MARGIN_PX) * (3/4) - BUSBAR_WIDTH_PX/2;
        outduct.height = CHILD_HEIGHT_PX;
        //right side
        outduct.relX = CHILD_SIDE_MARGIN_PX + (egressAbsPosition.WIDTH - 2*CHILD_SIDE_MARGIN_PX) * (1/4) + BUSBAR_WIDTH_PX/2;
        outduct.relY = outductRelYRight;
        outductRelYRight += CHILD_HEIGHT_PX + CHILD_BOTTOM_MARGIN_PX;

        outduct.absWireOutY = egressObj.absY + outduct.relY + outduct.height/2;
        outduct.absWireInY = outduct.absWireOutY;
        outduct.absWireOutX = egressObj.absX + outduct.relX + outduct.width;
        outduct.absWireInX = egressObj.absX + outduct.relX;



        //console.log(outduct);
        egressObj.d3ChildArray.push(outduct);

        ///////////next hop
        var nextHopObj = {};
        nextHopObj.topHeaderHeight = PARENT_TOP_HEADER_PX;
        nextHopObj.parent = null;
        nextHopObj.d3ChildArray = [];
        nextHopObj.id = "next_hop_node_id_" + od.nextHopNodeId;
        nextHopObj.name = "Node " + od.nextHopNodeId;
        nextHopObj.absX = nextHopsAbsPosition.X;
        nextHopObj.absY = nextHopsAbsPositionY;
        nextHopObj.width = nextHopsAbsPosition.WIDTH;
        nextHopsD3Array.push(nextHopObj);




        var finalDestinationEidUris = od["finalDestinationEidUris"];
        var nextHopRelY =  PARENT_TOP_HEADER_PX;// + CHILD_HEIGHT_PX/2;
        finalDestinationEidUris.forEach(function(fd, j) {

            ///////////next hop out port
            var nextHop = {};
            nextHop.linkIsUp = true;
            nextHop.parent = nextHopObj;
            nextHop.id = "next_hop_" + fd;
            nextHop.name = "";

            nextHop.width = (nextHopsAbsPosition.WIDTH - 2*CHILD_SIDE_MARGIN_PX) * (1/2) - BUSBAR_WIDTH_PX/2;
            nextHop.height = CHILD_HEIGHT_PX;

            nextHop.relX = CHILD_SIDE_MARGIN_PX + (nextHopsAbsPosition.WIDTH - 2*CHILD_SIDE_MARGIN_PX) * (1/2) + BUSBAR_WIDTH_PX/2;
            nextHop.relY = nextHopRelY;
            nextHopRelY += CHILD_HEIGHT_PX + CHILD_BOTTOM_MARGIN_PX;
            nextHopObj.height = nextHopRelY;

            nextHop.absWireOutY = nextHopObj.absY + nextHop.relY + nextHop.height/2;
            nextHop.absWireInY = nextHop.absWireOutY;
            nextHop.absWireOutX = nextHopObj.absX + nextHop.relX + nextHop.width;
            nextHop.absWireInX = nextHopObj.absX + nextHop.relX;


            //console.log(nextHop);
            nextHopObj.d3ChildArray.push(nextHop);

            ////////////////final dests
            //console.log(fd);
            var finalDest = {};
            finalDest.linkIsUp = true;
            finalDest.parent = finalDestsObj;
            finalDest.id = "fd_" + fd;
            finalDest.name = fd;

            finalDest.width = (finalDestsAbsPosition.WIDTH - 2*CHILD_SIDE_MARGIN_PX);
            finalDest.height = CHILD_HEIGHT_PX;
            finalDest.relX = CHILD_SIDE_MARGIN_PX;
            finalDest.relY = finalDestRelY;
            finalDestRelY += CHILD_HEIGHT_PX + CHILD_BOTTOM_MARGIN_PX;


            finalDest.absWireOutY = finalDestsObj.absY + finalDest.relY + finalDest.height/2;
            finalDest.absWireInY = finalDest.absWireOutY;
            finalDest.absWireOutX = finalDestsObj.absX + finalDest.relX + finalDest.width;
            finalDest.absWireInX = finalDestsObj.absX + finalDest.relX;

            finalDestsObj.d3ChildArray.push(finalDest);

            AddWire(nextHop, finalDest, "nextHop_finalDest");
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



        if(!paramDeclutter || outduct.linkIsUp) { //if cluttered or on
            AddWire(outduct, nextHopObj, "outduct_nextHop");
        }


    });

    //obj.height = Math.max(subObjRelYLeft, subObjRelYRight);
    egressObj.topHeaderHeight = PARENT_TOP_HEADER_PX;



    finalDestsObj.topHeaderHeight = PARENT_TOP_HEADER_PX;

    egressObj.busBar = {
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
