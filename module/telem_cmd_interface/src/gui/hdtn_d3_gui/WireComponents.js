/**
 * @file WireComponents.js
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
 * The WireComponents library is a closure that represents a set of svg paths.
 * The path supports dash array animation, arrow head, arc jumpovers, collision avoidance, drawing (through d3 enter()),
 * and undrawing (through d3 exit()).
 * The path consists of one horizontal line, followed by one vertical line, followed by one horizontal line.
 * Arc jumpovers can occur on the vertical line only (vertical line shown jumping over a horizontal line from another wire).
 */

function WireComponents(paramSvgRootGroup, paramSvgRootGroupClass, paramArrowMarker, paramSpeedUpperLimitBitsPerSec) {


    var svgRootGroup = paramSvgRootGroup;
    var svgRootGroupClass = paramSvgRootGroupClass;
    var arrowMarker = paramArrowMarker;
    var speedUpperLimitBitsPerSec = paramSpeedUpperLimitBitsPerSec;
    var WIRE_DASHARRAY = "6 2";
    var updateCounter = 0;
    var d3WiresArray = [];

    function GetWireBitsPerSec(wire) {
        if(wire.src.hasOwnProperty("rateBitsPerSec")) {
            return wire.src.rateBitsPerSec;
        }
        else if(wire.dest.hasOwnProperty("rateBitsPerSec")) {
            return wire.dest.rateBitsPerSec;
        }
        else {
            return 0;
        }
    }

    function GetWireTextComponents(wire) {
        let retVal = {
            "above": "",
            "below": "",
            "bothSet": false
        };
        let objWithVal = null;
        if(wire.src.hasOwnProperty("rateBitsPerSecHumanReadable")) {
            objWithVal = wire.src;
        }
        else if(wire.dest.hasOwnProperty("rateBitsPerSecHumanReadable")) {
            objWithVal = wire.dest;
        }
        else {
            return retVal;
        }

        const showBitRate = document.getElementById("id_showBitRate").checked;
        const showBundleRate = document.getElementById("id_showBundleRate").checked;
        retVal.bothSet = (showBitRate && showBundleRate);
        retVal.above = ((showBitRate) ? objWithVal["rateBitsPerSecHumanReadable"] : "");
        retVal.below = ((showBundleRate) ? objWithVal["rateBundlesPerSecHumanReadable"] : "");
        return retVal;
    }
    function GetWireTextAbove(wire) {
        return GetWireTextComponents(wire).above;
    }

    function GetWireTextBelow(wire) {
        return GetWireTextComponents(wire).below;
    }

    function GetWireText(wire) { //single string above line
        let tc = GetWireTextComponents(wire);
        let separatorStr = (tc.bothSet) ? "\u00A0\u00A0" : "";
        return tc.above + separatorStr + tc.below;
    }

    function GetWireTextTransform(wire) {
        if(wire.isMiddleTextLayout) {
            const textAngleDegrees = 0;
            const srcX = wire.src.absWireOutX;
            const srcY = wire.src.absWireOutY;
            const destX = wire.dest.absWireInX;
            const destY = wire.dest.absWireInY;
            return "translate(" + ((srcX + destX) * 0.5) + "," + ((srcY + destY) * 0.5) + ") rotate(" + textAngleDegrees + ") translate(0,-5)";
        }
        else {
            if(wire.src.absWireOutX < wire.dest.absWireInX) {
                return "translate(" + (7+wire.src.absWireOutX) + "," + (wire.src.absWireOutY-10) + ")";
            }
            else {
                return "translate(" + (7+wire.dest.absWireInX) + "," + (wire.dest.absWireInY-10) + ")";
            }
        }
    }

    function GetWirePathClass(wire) {
        const srcUp = wire.src.hasOwnProperty("linkIsUp") ? wire.src.linkIsUp : false;
        const destUp = wire.dest.hasOwnProperty("linkIsUp") ? wire.dest.linkIsUp : false;
        return (((srcUp) && (destUp)) ? "wire_on" : "wire_off");
    }
    function GetWirePathClassWithDashArrayClass(wire) {
        return GetWirePathClass(wire) + " wire_stroke_dash_array_attributes";
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
                    .attr("class", GetWirePathClassWithDashArrayClass) //.classed("wire_stroke_dash_array_attributes", true)//.attr("stroke-dasharray", WIRE_DASHARRAY) //6+2 = 8 => speed must be in multiples of 8
                    .attr("stroke-dashoffset", function(wire) {
                        //var amps = parseFloat(wire.src.hasOwnProperty("currentOut") ? wire.src.currentOut : wire.dest.currentIn);
                        //var ampsAbs = Math.abs(amps);
                        //animation looks good between -80 and 80 in multiples of 8
                        //+3 => 4-3=1AMP min turn on
                        //var speed = Math.round((ampsAbs+3)/8.0) * 8;
                        //return Math.round(parseInt(amps)) * 8;
                        //return (amps < 0) ? -speed : speed;
                        var divider = speedUpperLimitBitsPerSec / 10.0;
                        let rateBitsPerSec = GetWireBitsPerSec(wire);
                        return Math.round(parseInt(Math.min(rateBitsPerSec, speedUpperLimitBitsPerSec))/divider) * 8;
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
            .attr("class", svgRootGroupClass + " wire_stroke_width_attributes");

        //https://www.visualcinnamon.com/2016/01/animating-dashed-line-d3
        var wiresPath = enter.append("svg:path")
            .attr("d", GetWirePathStrWithJumps)
            .attr("fill", "none")
            .attr("marker-end", "url(#arrow)")
            .attr("class", GetWirePathClass)
            .classed("wire_stroke_dash_array_attributes", false)
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
            .attr("text-anchor", function(wire) {
                return ((wire.isMiddleTextLayout) ? "middle" : "start");
            })
            .attr("transform", GetWireTextTransform)
            .each(function(wire, i) {
                if(wire.isMiddleTextLayout) {
                    d3.select(this)
                        .append('tspan')
                        .attr("class", "tspanAbove")
                        .attr('x', 0)
                        .attr('dy', 0);
                    d3.select(this)
                        .append('tspan')
                        .attr("class", "tspanBelow")
                        .attr('x', 0)
                        .attr('dy', '1.4em');
                }
                else {
                    d3.select(this)
                        .attr("dy", ".35em")
                        .text("");
                }
            });

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
            .classed("wire_stroke_dash_array_attributes", false)
            .attr("stroke-dasharray", null)
            .attr("stroke-dashoffset", null)
            .attr("class", GetWirePathClass);
            //.style("stroke", "black");

        var update = updateBeforeTransition.transition(paramSharedTransition);
            //.attr("class", svgRootGroupClass);


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
            .each(function(wire, i) {
                if(wire.isMiddleTextLayout) {
                    d3.select(this).select(".tspanAbove").text(GetWireTextAbove);
                    d3.select(this).select(".tspanBelow").text(GetWireTextBelow);
                }
                else {
                    d3.select(this).text(GetWireText);
                }
            });



        var exitBeforeTransition = select.exit();




        exitBeforeTransition.select("path")
            //.attr("d", function(wire) {
            //    return wire.pathWithoutJumpsPrev;
            //})
            .classed("wire_stroke_dash_array_attributes", false)
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

        //not needed as ShowPowerRepeatFunc will call GetWirePathClass (and the following causes flicker because a class gets removed and added)
        ////select.select("path")
        ////    .attr("class", GetWirePathClass);
            //.style("stroke", function(wire) {
            //    var volts = wire.src.hasOwnProperty("voltageOut") ? wire.src.voltageOut : wire.dest.voltageIn;
            //    return colorInterp(parseFloat(volts));
            //});

        select.select("text")
            .each(function(wire, i) {
                if(wire.isMiddleTextLayout) {
                    d3.select(this).select(".tspanAbove").text(GetWireTextAbove);
                    d3.select(this).select(".tspanBelow").text(GetWireTextBelow);
                }
                else {
                    d3.select(this).text(GetWireText);
                }
            });
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
        ComputeWires: function(paramActiveObjectsMap, paramPowerSystem){
            DoComputeWires(paramActiveObjectsMap, paramPowerSystem);
        }
    };

}
