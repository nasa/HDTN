function ParseHdtnConfig(paramHdtnConfig, paramDeclutter, paramShrink, paramD3FaultsMap, PARAM_MAP_NAMES, PARAM_SUB_MAP_NAMES, PARAM_D3_SHAPE_ATTRIBUTES, PARAM_ABS_POSITION_MAP, PARAM_FLIP_HORIZONTAL_MAP) {

    var BUSBAR_WIDTH_PX = 5;

    var CHILD_HEIGHT_PX = 20;
    var CHILD_BOTTOM_MARGIN_PX = 10;
    var CHILD_SIDE_MARGIN_PX = 5;
    var PARENT_TOP_HEADER_PX = 20;
    var PARENT_GROUP_VERTICAL_SPACING_PX = 20;


    //Initialization

    wireConnections = [];

    var ingressAbsPosition = PARAM_ABS_POSITION_MAP["ingress"];
    var ingressObj = {};
    ingressObj.parent = null;
    ingressObj.d3ChildArray = [];
    ingressObj.id = "ingress"
    ingressObj.absX = ingressAbsPosition.X;
    ingressObj.absY = ingressAbsPosition.Y;
    ingressObj.width = ingressAbsPosition.WIDTH;
    ingressObj.height = ingressAbsPosition.HEIGHT;
    var ingressD3Array = [ingressObj];

    var inductsConfig = paramHdtnConfig["inductsConfig"];
    var inductVector = inductsConfig["inductVector"];

    var subObjRelYLeft =  PARENT_TOP_HEADER_PX;// + CHILD_HEIGHT_PX/2;


    inductVector.forEach(function(ind, i) {
        //console.log(i);

        ///////////induct
        var induct = {};
        induct.linkIsUp = true;
        induct.parent = egressObj;
        induct.id = "induct_" + i;
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
        induct.relY = subObjRelYLeft;
        subObjRelYLeft += CHILD_HEIGHT_PX + CHILD_BOTTOM_MARGIN_PX;

        induct.absWireOutY = ingressObj.absY + induct.relY + induct.height/2;
        induct.absWireInY = induct.absWireOutY;
        induct.absWireOutX = ingressObj.absX + induct.relX + induct.width;
        induct.absWireInX = ingressObj.absX + induct.relX;



        console.log(induct);
        ingressObj.d3ChildArray.push(induct);


    });

    //obj.height = Math.max(subObjRelYLeft, subObjRelYRight);
    ingressObj.topHeaderHeight = PARENT_TOP_HEADER_PX;

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
    egressObj.id = "egress"
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



    //obj=oru, subObj=switch

    var subObjRelYRight = PARENT_TOP_HEADER_PX;


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
        outduct.relY = subObjRelYRight;
        subObjRelYRight += CHILD_HEIGHT_PX + CHILD_BOTTOM_MARGIN_PX;

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

            wireConnections.push({
                "src": nextHop,
                "dest": finalDest,
                "groupName": "nextHop_finalDest"
            });
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
            wireConnections.push({
                "src": outduct,
                "dest": nextHopsObj,
                "groupName": "outduct_nextHop"
            });
        }



        /*
        var map = paramPowerSystem[mapName];
        var objShapeAttr = PARAM_D3_SHAPE_ATTRIBUTES[mapName];
        objShapeAttr.d3Array = [];

        for(var objName in outduct) {
            console.log(objName);
            /*var obj = map[objName];
            obj.parent = null;
            obj.pathName = mapName + "." + obj.id;
            var absPosition = PARAM_ABS_POSITION_MAP[obj.pathName];
            obj.flipHorizontal = (obj.pathName in PARAM_FLIP_HORIZONTAL_MAP);

            var subObjRelYRight = objShapeAttr.top_header;
            var subObjRelYLeft =  objShapeAttr.top_header + objShapeAttr.switch_height/2;
            if(!paramDeclutter || (obj.state > 0.5) || (obj.pathName in paramD3FaultsMap)) { //if cluttered or on or faulted
                activeObjectsMap[obj.pathName] = obj;
                objShapeAttr.d3Array.push(obj);

                obj.absX = absPosition.X;
                obj.absY = absPosition.Y;

                obj.width = objShapeAttr.width;
            }
        }


            if(PARAM_SUB_MAP_NAMES[i].length == 0) { //oru with no switches

                obj.height = objShapeAttr.height;
                obj.terminalWidth = (objShapeAttr.hasOwnProperty("terminalWidth")) ? objShapeAttr.terminalWidth : 0;

                var wireXScale = (objShapeAttr.hasOwnProperty("wireXScale")) ? objShapeAttr.wireXScale : 1;
                var wireYScale = (objShapeAttr.hasOwnProperty("wireYScale")) ? objShapeAttr.wireYScale : 0.5;

                obj.absWireOutY = obj.absY + (obj.height * wireYScale);
                obj.absWireInY = obj.absWireOutY;


                if(obj.flipHorizontal) {
                    obj.absWireInX = obj.absX + (obj.width * wireXScale);
                    obj.absWireOutX = obj.absX - obj.terminalWidth;
                }
                else {
                    obj.absWireOutX = obj.absX + (obj.width * wireXScale) + obj.terminalWidth;
                    obj.absWireInX = obj.absX;
                }


            }
            else {
                obj.d3ChildArray = [];
                PARAM_SUB_MAP_NAMES[i].forEach(function(subMapName) {
                    var subMap = obj[subMapName];
                    var subObjShapeAttr = objShapeAttr[subMapName]; //move this up

                    //support custom ordering
                    var subObjNames = [];
                    if(absPosition.hasOwnProperty("subObjOrder") && absPosition.subObjOrder.hasOwnProperty(subMapName)) {
                        subObjNames = absPosition.subObjOrder[subMapName];
                    }
                    else { //default order
                        for(var subObjName in subMap) {
                            subObjNames.push(subObjName);
                        }
                    }

                    subObjNames.forEach(function(subObjName) {

                        var subObj = subMap[subObjName];
                        subObj.parent = obj;
                        subObj.pathName = obj.pathName + "." + subMapName + "." + subObj.id;
                        subObj.flipHorizontal = (subObj.pathName in PARAM_FLIP_HORIZONTAL_MAP);


                        if(!paramDeclutter || (subObj.state > 0.5) || (subObj.pathName in paramD3FaultsMap)) { //if cluttered or on or faulted
                            activeObjectsMap[subObj.pathName] = subObj;
                            obj.d3ChildArray.push(subObj);
                        }
                        if(!paramShrink || (subObj.state > 0.5) || (subObj.pathName in paramD3FaultsMap)) { //if cluttered or decluttered_gaps or on or faulted
                            subObj.width = subObjShapeAttr.width;
                            subObj.height = objShapeAttr.switch_height;
                            if(subObjShapeAttr.hasOwnProperty("leftSideMap") && (subObj.pathName in subObjShapeAttr.leftSideMap)) {
                                subObj.relX = subObjShapeAttr.relXLeft;
                                subObj.relY = subObjRelYLeft;
                                subObjRelYLeft += subObj.height + objShapeAttr.switch_bottom_margin;
                            }
                            else { //right
                                subObj.relX = subObjShapeAttr.hasOwnProperty("relXRight") ? subObjShapeAttr.relXRight : subObjShapeAttr.relX;
                                subObj.relY = subObjRelYRight;
                                subObjRelYRight += subObj.height + objShapeAttr.switch_bottom_margin;
                            }


                            subObj.absWireOutY = obj.absY + subObj.relY + subObj.height/2;
                            subObj.absWireInY = subObj.absWireOutY;

                            if(subObj.flipHorizontal) {
                                subObj.absWireInX = obj.absX + subObj.relX + subObj.width;
                                subObj.absWireOutX = obj.absX + subObj.relX;
                            }
                            else {
                                subObj.absWireOutX = obj.absX + subObj.relX + subObj.width;
                                subObj.absWireInX = obj.absX + subObj.relX;
                            }
                        }


                    });
                });

                obj.height = Math.max(subObjRelYLeft, subObjRelYRight);
                obj.topHeaderHeight = objShapeAttr.top_header;

                if(objShapeAttr.hasOwnProperty("busBar")) {
                    obj.busBar = {
                        "x1": objShapeAttr.busBar.relX,
                        "y1": objShapeAttr.top_header,
                        "x2": objShapeAttr.busBar.relX,
                        "y2": objShapeAttr.top_header + obj.height - objShapeAttr.top_header - objShapeAttr.switch_bottom_margin
                    };
                }
            }
        }*/
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
        "ingressD3Array": ingressD3Array,
        "egressD3Array": egressD3Array,
        "nextHopsD3Array": nextHopsD3Array,
        "finalDestsD3Array": finalDestsD3Array,
        "wireConnections": wireConnections
    };
}
