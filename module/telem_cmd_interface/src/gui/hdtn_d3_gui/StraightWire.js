function StraightWire(paramSvgRootGroup, paramTextAngleDegrees, paramFormatValueHumanReadableFn, paramSpeedUpperLimit,
    paramSpeedBpsAverageCalculator, paramSrcX, paramSrcY, paramDestX, paramDestY) {

    var svgRootGroup = paramSvgRootGroup;
    var srcX = paramSrcX;
    var srcY = paramSrcY;
    var destX = paramDestX;
    var destY = paramDestY;
    var formatValueHumanReadableFn = paramFormatValueHumanReadableFn;
    var speedUpperLimit = paramSpeedUpperLimit;
    var textAngleDegrees = paramTextAngleDegrees;
    var speedBps = 0;
    var speedBpsAverageCalculator = paramSpeedBpsAverageCalculator;
    var WIRE_DASHARRAY = "6 2";

    function GetWirePathStr() {
        return "M" + paramSrcX + " " + paramSrcY + " L" + paramDestX + " " + paramDestY;
    }




    function GetWireText() {
        if(formatValueHumanReadableFn != null) {
            return formatValueHumanReadableFn(speedBps, 2);
        }
        return "";
    }





    var wirePathObj = svgRootGroup.append("svg:path")
        .attr("d", GetWirePathStr)
        .attr("marker-end", "url(#arrow)")
        .attr("class", "wire_tx")
        .attr("stroke-dasharray", (paramFormatValueHumanReadableFn == null) ? null : WIRE_DASHARRAY)
        .attr("stroke-dashoffset", 0);



    var wireText = svgRootGroup.append("svg:text")
            .attr("class", "wire_txt")
            .attr("text-anchor", "middle")
            .attr("transform", "translate(" + ((srcX + destX) * 0.5) + "," + ((srcY + destY) * 0.5) + ") rotate(" + textAngleDegrees + ") translate(0,-5)")
            .text(GetWireText);

    WireRepeatFunc();
    function WireRepeatFunc() {
        if(paramFormatValueHumanReadableFn != null) {
            DoRepeat();
        }
        function DoRepeat() {
            wirePathObj
                .attr("stroke-dasharray", WIRE_DASHARRAY)
                .attr("stroke-dashoffset", function(link) {
                    //scale between 0 and 10 for speed

                    //var divider = 1000.0; //for bundles (0 to 10000) -> (0 to 10)
                    //if(unit === "bytes") divider = 250000000.0; //for bytes (0 to 20Gbps, 2.5GBps) -> (0 to 10)
                    //return Math.round(parseInt(speedBps)/divider) * 8;

                    var divider = speedUpperLimit / 10.0;
                    return Math.round(parseInt(speedBps)/divider) * 8;
                })
                .transition().ease(d3.easeLinear).duration(1000)
                .attr("stroke-dashoffset", 0)
                .on("end", DoRepeat);  // when the transition finishes start again

        }

    };



    return {
        SetSpeedBps: function(paramSpeedBps){
            speedBps = speedBpsAverageCalculator.Update(paramSpeedBps);
            wireText.text(GetWireText);
        }
    };
}
