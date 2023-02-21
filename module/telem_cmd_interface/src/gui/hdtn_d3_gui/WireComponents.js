
function WireComponents(paramSvgRootGroup, paramSvgRootGroupClass, paramArrowMarker) {


    var svgRootGroup = paramSvgRootGroup;
    var svgRootGroupClass = paramSvgRootGroupClass;
    var arrowMarker = paramArrowMarker;
    var WIRE_DASHARRAY = "6 2";
    var WIRE_WIDTH_PX = 3;
    var updateCounter = 0;
    var d3WiresArray = [];

    function GetWireText(wire) {
        let objWithMap = null;
        if(wire.src.hasOwnProperty("totalBundlesReceived")) {
            objWithMap = wire.src;
        }
        else if(wire.dest.hasOwnProperty("totalBundlesReceived")) {
            objWithMap = wire.dest;
        }
        else {
            return "";
        }
        return "" + objWithMap["totalBundlesReceived"] + "";
        var showCurrent = document.getElementById("id_showCurrent").checked;
        var showVoltage = document.getElementById("id_showVoltage").checked;
        var amps = wire.src.hasOwnProperty("currentOut") ? wire.src.currentOut : wire.dest.currentIn;
        var volts = wire.src.hasOwnProperty("voltageOut") ? wire.src.voltageOut : wire.dest.voltageIn;
        var ampsStr = showCurrent ? (Number.parseFloat(amps).toFixed(1) + "A") : "";
        var separatorStr = showCurrent ? " " : "";
        var voltsStr = showVoltage ? (Number.parseFloat(volts).toFixed(1) + "V") : "";
        return "" + (ampsStr.startsWith("-") ? "" : "\u00A0") + ampsStr + separatorStr + (voltsStr.startsWith("-") ? "" : "\u00A0") + voltsStr;
    }

    function GetWireTextTransform(wire) {
        if(wire.src.absWireOutX < wire.dest.absWireInX) {
            return "translate(" + (7+wire.src.absWireOutX) + "," + (wire.src.absWireOutY-10) + ")";
        }
        else {
            return "translate(" + (7+wire.dest.absWireInX) + "," + (wire.dest.absWireInY-10) + ")";
        }
    }

    function GetWirePathClass(wire) {
        var stateSrc = wire.src.hasOwnProperty("state") ? wire.src.state : 1;
        var stateDest = wire.dest.hasOwnProperty("state") ? wire.dest.state : 1;
        return ((stateSrc > 0.5) && (stateDest > 0.5)) ? "wire_on" : "wire_off";
    }

    function GetWirePathStrWithJumps(wire) {
        var wire_x0 = wire.src.absWireOutX;
        var wire_y0 = wire.src.absWireOutY;
        var wire_xf = wire.dest.absWireInX;
        var wire_yf = wire.dest.absWireInY;
        var xMid = wire_x0 + (wire_xf - wire_x0) * wire.xDropNorm;
        var arcR = 8;
        var lineYs = wire.y_jumps; //[50, 80];

        var path_d = "M" + wire_x0 + " " + wire_y0 + " L" + xMid + " " + wire_y0;
        if(wire_y0 < wire_yf) {
            for(var i=0; i<lineYs.length; ++i) {
                path_d += " L" + xMid + " " + (lineYs[i] - arcR) +
                    " a" + arcR + " " + arcR + " 0 0 1 0 " + 2*arcR;
            }
        }
        else {
            for(var i=lineYs.length-1; i>=0; --i) {
            //for(var i=0; i<lineYs.length; ++i) {
                path_d += " L" + xMid + " " + (lineYs[i] + arcR) +
                    " a" + arcR + " " + arcR + " 0 0 0 0 " + (-2*arcR);
            }
        }

        path_d += " L" + xMid + " " + wire_yf;
        path_d += " L" + wire_xf + " " + wire_yf;
        return path_d;
    }

    function GetWirePathStrWithoutJumps(wire) {
        var wire_x0 = wire.src.absWireOutX;
        var wire_y0 = wire.src.absWireOutY;
        var wire_xf = wire.dest.absWireInX;
        var wire_yf = wire.dest.absWireInY;
        var xMid = wire_x0 + (wire_xf - wire_x0) * wire.xDropNorm;


        var path_d = "M" + wire_x0 + " " + wire_y0 + " L" + xMid + " " + wire_y0;
        path_d += " L" + xMid + " " + wire_yf;
        path_d += " L" + wire_xf + " " + wire_yf;
        return path_d;
    }

    function ShowPowerRepeatFunc(obj) {
        var currentCount = updateCounter;
        DoRepeat();
        function DoRepeat() {
            if(currentCount == updateCounter) {
                d3.select(obj)
                    .attr("stroke-dasharray", WIRE_DASHARRAY) //6+2 = 8 => speed must be in multiples of 8
                    .attr("stroke-dashoffset", function(wire) {
                        var amps = parseFloat(wire.src.hasOwnProperty("currentOut") ? wire.src.currentOut : wire.dest.currentIn);
                        var ampsAbs = Math.abs(amps);
                        //animation looks good between -80 and 80 in multiples of 8
                        //+3 => 4-3=1AMP min turn on
                        var speed = Math.round((ampsAbs+3)/8.0) * 8;
                        //return Math.round(parseInt(amps)) * 8;
                        return (amps < 0) ? -speed : speed;
                    })
                    .transition().ease(d3.easeLinear).duration(1000)
                    .attr("stroke-dashoffset", 0)
                    //.on("interrupt",function(){console.log("interrupt");})
                    .on("end", DoRepeat);  // when the transition finishes start again
            }
            //else {
            //    console.log("passed");
            //}
        }

    };


    function DoUpdate(paramSharedTransition) {
        ++updateCounter;
        arrowMarker.attr("markerWidth", 4).attr("markerHeight", 4);

        var select = svgRootGroup.selectAll("." + svgRootGroupClass)
            .data(d3WiresArray, function(wire) {
                return wire.src.id + "_" + wire.dest.id;
            });

        var enter = select.enter().append("svg:g")
            .attr("class", svgRootGroupClass);

        //https://www.visualcinnamon.com/2016/01/animating-dashed-line-d3
        var wiresPath = enter.append("svg:path")
            .attr("d", GetWirePathStrWithJumps)
            .attr("fill", "none")
            .style("stroke-width", WIRE_WIDTH_PX)
            .attr("marker-end", "url(#arrow)")
            .attr("class", GetWirePathClass)
            .attr("stroke-dasharray", function() {
                var totalLength = d3.select(this).node().getTotalLength();
                return totalLength + " " + totalLength;
            })
            .attr("stroke-dashoffset", function() {
                var totalLength = d3.select(this).node().getTotalLength();
                return totalLength;
            });

        enter.append("svg:text")
            .attr("class", "wire_text")
            .attr("dy", ".35em")
            //.attr("text-anchor", function(wire) {
            //    return "end";
            //})
            .attr("transform", GetWireTextTransform)
            .text("");

        enter.transition(paramSharedTransition)
            .select("path")
            .attr("stroke-dashoffset", 0)
            .on("end", function() {
                //d3.select(this)
                //    .attr("stroke-dasharray", null)
                //    .attr("stroke-dashoffset", null);
                ShowPowerRepeatFunc(this);
            });

        var updateBeforeTransition = select;
        updateBeforeTransition.select("path")
            .attr("d", function(wire) {
                return wire.pathWithoutJumpsPrev;
            })
            .attr("stroke-dasharray", null)
            .attr("stroke-dashoffset", null)
            .attr("class", GetWirePathClass);
            //.style("stroke", "black");

        var update = updateBeforeTransition.transition(paramSharedTransition)
            .attr("class", svgRootGroupClass);


        update.select("path")
            .attr("d", function(wire) {
                return wire.pathWithoutJumps;
            })
            .on("end", function() {
                d3.select(this).attr("d", GetWirePathStrWithJumps);
                ShowPowerRepeatFunc(this);
            });

        update.select("text")
            .attr("transform", GetWireTextTransform)
            .text(GetWireText);



        var exitBeforeTransition = select.exit();




        exitBeforeTransition.select("path")
            //.attr("d", function(wire) {
            //    return wire.pathWithoutJumpsPrev;
            //})
            .attr("stroke-dasharray", function() {
                var totalLength = d3.select(this).node().getTotalLength();
                return totalLength + " " + totalLength;
            })
            .attr("stroke-dashoffset", 0);

        var exit = exitBeforeTransition.transition(paramSharedTransition).remove();

        exit.select("path")
            .attr("stroke-dashoffset", function() {
                var totalLength = d3.select(this).node().getTotalLength();
                return totalLength;
            });
    }

    function DoUpdateLite() {

        var select = svgRootGroup.selectAll("." + svgRootGroupClass)
            .data(d3WiresArray, function(wire) {
                return wire.src.id + "_" + wire.dest.id;
            });

        select.select("path")
            .attr("class", GetWirePathClass);
            //.style("stroke", function(wire) {
            //    var volts = wire.src.hasOwnProperty("voltageOut") ? wire.src.voltageOut : wire.dest.voltageIn;
            //    return colorInterp(parseFloat(volts));
            //});

        select.select("text")
            .text(GetWireText);
    }

    function DoComputeWires(paramWireConnections) {

        d3WiresArray = [];
        var wiresGroupedMap = {};
        paramWireConnections.forEach(function(conn) {
            d3WiresArray.push(conn);
            if (!wiresGroupedMap.hasOwnProperty(conn.groupName)) {
                wiresGroupedMap[conn.groupName] = [];
            }
            wiresGroupedMap[conn.groupName].push(conn);
        });
        function WiresOverlap(w1, w2) {
            var w1Low = Math.min(w1.src.absWireOutY, w1.dest.absWireInY);
            var w1High = Math.max(w1.src.absWireOutY, w1.dest.absWireInY);

            var w2Low = Math.min(w2.src.absWireOutY, w2.dest.absWireInY);
            var w2High = Math.max(w2.src.absWireOutY, w2.dest.absWireInY);

            return ( ((w2Low  > (w1Low-3 )) && (w2Low  < (w1High+3))) ||
                     ((w2High > (w1Low-3 )) && (w2High < (w1High+3))) ); //between -> overlap
        }
        function GetAllOverlappingWires(wire, allWiresArray, overlappingWiresMap, overlappingWiresArray) {
            overlappingWiresMap[wire.src.id + "_" + wire.dest.id] = 1;
            overlappingWiresArray.push(wire);
            for(var i=0; i<allWiresArray.length; ++i) {
                var w2 = allWiresArray[i];
                if (!( (w2.src.id + "_" + w2.dest.id) in overlappingWiresMap)) {
                    if(WiresOverlap(wire, w2)) {
                        GetAllOverlappingWires(w2, allWiresArray, overlappingWiresMap, overlappingWiresArray);
                    }
                }

            }
        }
        function line_line_intersect2(vertLine, horizLine) {
            function btwn(a, b1, b2) {
                if ((a >= b1) && (a <= b2)) { return true; }
                if ((a >= b2) && (a <= b1)) { return true; }
                return false;
            }

            if(btwn(vertLine.x, horizLine.x1, horizLine.x2) && btwn(horizLine.y, vertLine.y1, vertLine.y2)) {
                return {'x': vertLine.x, 'y': horizLine.y};
            }
            return null;
        }
        function compareByAbsWireOutY(a,b) {
            if (a.src.absWireOutY < b.src.absWireOutY)
                return -1;
            if (a.src.absWireOutY > b.src.absWireOutY)
                return 1;
            return 0;
        }
        for(var propWiresGrouped in wiresGroupedMap) { //compute xDropNorm
            var groupArray = wiresGroupedMap[propWiresGrouped];
            groupArray.sort(compareByAbsWireOutY);
            for(var i=0; i<groupArray.length; ++i) {
                var wire = groupArray[i];
                wire.isDrawn = false;
            }
            for(var i=0; i<groupArray.length; ++i) {
                var wire = groupArray[i];
                if(!wire.isDrawn) {
                    overlappingWiresMap = {};
                    overlappingWiresArray = [];
                    GetAllOverlappingWires(wire, groupArray, overlappingWiresMap, overlappingWiresArray);
                    //console.log(overlappingWiresMap);
                    var firstWireIsGoingDown = (overlappingWiresArray[0].src.absWireOutY < overlappingWiresArray[0].dest.absWireInY);
                    for(var j=0; j<overlappingWiresArray.length; ++j) {
                        overlappingWiresArray[j].isDrawn = true;
                        if(overlappingWiresArray[j].hasOwnProperty("manualXDropNorm")) {
                            overlappingWiresArray[j].xDropNorm = overlappingWiresArray[j].manualXDropNorm;
                        }
                        else {
                            overlappingWiresArray[j].xDropNorm = (firstWireIsGoingDown) ? (overlappingWiresArray.length - j) / (overlappingWiresArray.length + 1)
                                                                                        : (j + 1) / (overlappingWiresArray.length + 1);
                        }
                    }
                }
                if (wire.hasOwnProperty("pathWithoutJumps")) {
                    wire.pathWithoutJumpsPrev = wire.pathWithoutJumps;
                }
                wire.pathWithoutJumps = GetWirePathStrWithoutJumps(wire);
            }
        }
        //now find the wire jumpovers
        for(var propWiresGrouped in wiresGroupedMap) { //compute xDropNorm
            var groupArray = wiresGroupedMap[propWiresGrouped];
            for(var i=0; i<groupArray.length; ++i) {
                var w0 = groupArray[i];
                var w0_x0 = w0.src.absWireOutX;
                var w0_y0 = w0.src.absWireOutY;
                var w0_xf = w0.dest.absWireInX;
                var w0_yf = w0.dest.absWireInY;

                w0.y_jumps = [];
                var w0X = w0_x0 + (w0_xf - w0_x0) * w0.xDropNorm;
                var vertLine = {x: w0X, y1: w0_y0, y2: w0_yf};

                for(var j=0; j<groupArray.length; ++j) {
                    if(j==i) continue;
                    var w1 = groupArray[j];
                    var w1_x0 = w1.src.absWireOutX;
                    var w1_y0 = w1.src.absWireOutY;
                    var w1_xf = w1.dest.absWireInX;
                    var w1_yf = w1.dest.absWireInY;
                    //console.log("testing " + w0.name + " against " + w1.name);
                    var w1XMid = w1_x0 + (w1_xf - w1_x0) * w1.xDropNorm;
                    var horizontalLine1 = {x1: w1_x0, x2: w1XMid, y: w1_y0};
                    var horizontalLine2 = {x1: w1XMid, x2: w1_xf, y: w1_yf};

                    var pt = line_line_intersect2(vertLine, horizontalLine1);
                    if (pt != null) {
                        w0.y_jumps.push(pt.y);
                    }
                    pt = line_line_intersect2(vertLine, horizontalLine2);
                    if (pt != null) {
                        w0.y_jumps.push(pt.y);
                    }
                }
                w0.y_jumps.sort(function(a,b){return a-b;});
            }
        }

    }


    return {
        Update: function(paramSharedTransition){
            DoUpdate(paramSharedTransition);
        },
        UpdateLite: function(){
            DoUpdateLite();
        },
        SetDashArray: function(paramDashArray){
            WIRE_DASHARRAY = paramDashArray;
            //d3.selectAll("." + svgRootGroupClass + " path").attr("stroke-dasharray", WIRE_DASHARRAY);
        },
        SetStrokeWidth: function(paramStrokeWidth){
            WIRE_WIDTH_PX = parseInt(paramStrokeWidth);
            d3.selectAll("." + svgRootGroupClass + " path").style("stroke-width", WIRE_WIDTH_PX);
        },
        ComputeWires: function(paramActiveObjectsMap, paramPowerSystem){
            DoComputeWires(paramActiveObjectsMap, paramPowerSystem);
        }
    };

}
