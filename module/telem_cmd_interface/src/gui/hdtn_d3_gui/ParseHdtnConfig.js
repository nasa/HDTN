function UpdateActiveInductConnections(paramHdtnConfig, paramActiveInductConnections) {
    let inductsConfig = paramHdtnConfig["inductsConfig"];
    let inductVector = inductsConfig["inductVector"];
    inductVector.forEach(function(ind, i) {
        //console.log(i);
        ind["activeConnections"] = paramActiveInductConnections.inductActiveConnections[i].activeConnections;
    });
}
function InitActiveInductConnections(paramHdtnConfig) {
    let inductsConfig = paramHdtnConfig["inductsConfig"];
    let inductVector = inductsConfig["inductVector"];
    inductVector.forEach(function(ind, i) {
        //console.log(i);
        ind["activeConnections"] = ["null"];
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

function ParseHdtnConfig(paramWireConnectionsOldMap, paramHdtnOldDrawHash, paramNewHdtnConfig, paramDeclutter, paramShrink, paramD3FaultsMap, PARAM_MAP_NAMES, PARAM_SUB_MAP_NAMES, PARAM_D3_SHAPE_ATTRIBUTES, PARAM_ABS_POSITION_MAP, PARAM_FLIP_HORIZONTAL_MAP) {

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

    let newHashStr = GetDrawHash(paramNewHdtnConfig);
    let needsRedraw = (paramHdtnOldDrawHash == null) || (paramHdtnOldDrawHash !== newHashStr);
    if(!needsRedraw) {
        console.log("does not need redraw");
        return null;//todo
    }
    else {
        console.log("needs redraw");
        paramHdtnOldDrawHash = newHashStr;
    }

    var BUSBAR_WIDTH_PX = 5;

    var CHILD_HEIGHT_PX = 20;
    var CHILD_BOTTOM_MARGIN_PX = 10;
    var CHILD_SIDE_MARGIN_PX = 5;
    var PARENT_TOP_HEADER_PX = 20;
    var PARENT_GROUP_VERTICAL_SPACING_PX = 20;


    //Initialization




    var storageAbsPosition = PARAM_ABS_POSITION_MAP["storage"];
    var storageObj = {};
    storageObj.parent = null;
    //storageObj.d3ChildArray = []; //keep undefined
    storageObj.id = "storage";
    storageObj.name = "Storage";
    storageObj.absX = storageAbsPosition.X;
    storageObj.absY = storageAbsPosition.Y;
    storageObj.width = storageAbsPosition.WIDTH;
    storageObj.height = storageAbsPosition.HEIGHT;
    var storageD3Array = [storageObj];
    storageObj.topHeaderHeight = PARENT_TOP_HEADER_PX;


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


    var inductsConfig = paramNewHdtnConfig["inductsConfig"];
    var inductVector = inductsConfig["inductVector"];

    var inductRelYLeft =  PARENT_TOP_HEADER_PX;// + CHILD_HEIGHT_PX/2;
    var activeInductConnsRelY = PARENT_TOP_HEADER_PX;

    inductVector.forEach(function(ind, i) {
        //console.log(i);

        var activeInductConnsNames = ind["activeConnections"];
        activeInductConnsNames.forEach(function(connName, j) {

            var baseId = "induct_" + i + "_conn_" + connName;// + "_" + j; //don't use j, will affect redraw on deletion
            ///////////induct
            var induct = {};
            induct.linkIsUp = true;
            induct.parent = egressObj;
            induct.id = "induct_" + baseId;
            var cvName = "??";
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
            induct.name = cvName + "[" + i + "]";

            induct.width = (ingressAbsPosition.WIDTH - 2*CHILD_SIDE_MARGIN_PX) * (3/4) - BUSBAR_WIDTH_PX/2;
            induct.height = CHILD_HEIGHT_PX;
            //left side
            induct.relX = CHILD_SIDE_MARGIN_PX;
            induct.relY = inductRelYLeft;
            inductRelYLeft += CHILD_HEIGHT_PX + CHILD_BOTTOM_MARGIN_PX;

            induct.absWireOutY = ingressObj.absY + induct.relY + induct.height/2;
            induct.absWireInY = induct.absWireOutY;
            induct.absWireOutX = ingressObj.absX + induct.relX + induct.width;
            induct.absWireInX = ingressObj.absX + induct.relX;

            //console.log(induct);
            ingressObj.d3ChildArray.push(induct);



            ////////////////active induct connections
            var connObj = {};
            connObj.linkIsUp = true;
            connObj.parent = activeInductConnsObj;
            connObj.id = "conn_" + baseId;
            connObj.name = connName;

            connObj.width = (activeInductConnsAbsPosition.WIDTH - 2*CHILD_SIDE_MARGIN_PX);
            connObj.height = CHILD_HEIGHT_PX;
            connObj.relX = CHILD_SIDE_MARGIN_PX;
            connObj.relY = activeInductConnsRelY;
            activeInductConnsRelY += CHILD_HEIGHT_PX + CHILD_BOTTOM_MARGIN_PX;


            connObj.absWireOutY = activeInductConnsObj.absY + connObj.relY + connObj.height/2;
            connObj.absWireInY = connObj.absWireOutY;
            connObj.absWireOutX = activeInductConnsObj.absX + connObj.relX + connObj.width;
            connObj.absWireInX = activeInductConnsObj.absX + connObj.relX;

            activeInductConnsObj.d3ChildArray.push(connObj);

            AddWire(connObj, induct, "conn_induct");
        });


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

    var outductsConfig = paramNewHdtnConfig["outductsConfig"];
    var outductVector = outductsConfig["outductVector"];
    //console.log(outductVector);



    var outductRelYRight = PARENT_TOP_HEADER_PX;


    var finalDestRelY = PARENT_TOP_HEADER_PX;

    outductVector.forEach(function(od, i) {
        //console.log(i);

        ///////////outduct
        var outduct = {};
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
        var nextHopsObj = {};
        nextHopsObj.topHeaderHeight = PARENT_TOP_HEADER_PX;
        nextHopsObj.parent = null;
        nextHopsObj.d3ChildArray = [];
        nextHopsObj.id = "next_hop_node_id_" + od.nextHopNodeId;
        nextHopsObj.name = "Node " + od.nextHopNodeId;
        nextHopsObj.absX = nextHopsAbsPosition.X;
        nextHopsObj.absY = nextHopsAbsPositionY;
        nextHopsObj.width = nextHopsAbsPosition.WIDTH;
        nextHopsD3Array.push(nextHopsObj);




        var finalDestinationEidUris = od["finalDestinationEidUris"];
        var nextHopRelY =  PARENT_TOP_HEADER_PX;// + CHILD_HEIGHT_PX/2;
        finalDestinationEidUris.forEach(function(fd, j) {

            ///////////next hop out port
            var nextHop = {};
            nextHop.linkIsUp = true;
            nextHop.parent = nextHopsObj;
            nextHop.id = "next_hop_" + fd;
            nextHop.name = "";

            nextHop.width = (nextHopsAbsPosition.WIDTH - 2*CHILD_SIDE_MARGIN_PX) * (1/2) - BUSBAR_WIDTH_PX/2;
            nextHop.height = CHILD_HEIGHT_PX;

            nextHop.relX = CHILD_SIDE_MARGIN_PX + (nextHopsAbsPosition.WIDTH - 2*CHILD_SIDE_MARGIN_PX) * (1/2) + BUSBAR_WIDTH_PX/2;
            nextHop.relY = nextHopRelY;
            nextHopRelY += CHILD_HEIGHT_PX + CHILD_BOTTOM_MARGIN_PX;
            nextHopsObj.height = nextHopRelY;

            nextHop.absWireOutY = nextHopsObj.absY + nextHop.relY + nextHop.height/2;
            nextHop.absWireInY = nextHop.absWireOutY;
            nextHop.absWireOutX = nextHopsObj.absX + nextHop.relX + nextHop.width;
            nextHop.absWireInX = nextHopsObj.absX + nextHop.relX;


            //console.log(nextHop);
            nextHopsObj.d3ChildArray.push(nextHop);

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
        nextHopsObj.busBar = {
            "x1": nextHopsAbsPosition.WIDTH / 2.0,
            "y1": PARENT_TOP_HEADER_PX,
            "x2": nextHopsAbsPosition.WIDTH / 2.0,
            "y2": PARENT_TOP_HEADER_PX + nextHopsObj.height - PARENT_TOP_HEADER_PX - CHILD_BOTTOM_MARGIN_PX
        };


        nextHopsObj.absWireOutY = nextHopsAbsPositionY + nextHopsObj.height/2;
        nextHopsObj.absWireInY = nextHopsObj.absWireOutY;
        nextHopsObj.absWireOutX = nextHopsObj.absX + nextHopsObj.width;
        nextHopsObj.absWireInX = nextHopsObj.absX;

        nextHopsAbsPositionY += nextHopsObj.height + PARENT_GROUP_VERTICAL_SPACING_PX;



        if(!paramDeclutter || outduct.linkIsUp) { //if cluttered or on
            AddWire(outduct, nextHopsObj, "outduct_nextHop");
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
        "egressD3Array": egressD3Array,
        "storageD3Array": storageD3Array,
        "nextHopsD3Array": nextHopsD3Array,
        "finalDestsD3Array": finalDestsD3Array,
        "wireConnections": wireConnections
    };
}
