function ToolTip() {

    var globalToolTipObject = null;
    var d3FaultsMapLocal = {};

    // Define the div for the tooltip
    var divTooltip = d3.select("body").append("div")
        .attr("class", "tooltip")
        .style("opacity", 0);

    function UpdateActiveToolTip() {
        if(globalToolTipObject != null) {
            UpdateToolTipText(globalToolTipObject, false);
        }
    }

    function UpdateToolTipText(obj, isMouseEvent) {
        globalToolTipObject = obj;
        var textStr = obj.id;
        for(var propName in obj) {
            if(["currentIn", "currentOut", "voltageIn", "voltageOut", "soc", "nominalCapacityAmpHours", "state"].indexOf(propName) != -1) {
                var numToStr = Number.parseFloat(obj[propName]).toFixed(1);
                textStr += "<br/>" + propName + ": " + numToStr;
            }
        }
        if(obj.pathName in d3FaultsMapLocal) {
            textStr += "<br/>Faults:";
            var faults = d3FaultsMapLocal[obj.pathName].faultTypes;
            for(var i=0; i< faults.length; ++i) {
                textStr += "<br/> " + faults[i];
            }
        }
        divTooltip.html(textStr);
        if(isMouseEvent) {
            divTooltip.style("left", (d3.event.pageX) + "px");
            divTooltip.style("top", (d3.event.pageY - 160) + "px");
        }
    }
    function MouseEventToolTip(d) {
        if(d3.event.type === "mouseover") {
            divTooltip.transition()
                .duration(200)
                .style("opacity", .9);
            UpdateToolTipText(d, true);
        }
        else if(d3.event.type === "mousemove") {
            UpdateToolTipText(d, true);
        }
        else if(d3.event.type === "mouseout") {
            divTooltip.transition()
                .duration(200)
                .style("opacity", 0);
        }
    }


    return {
        Update: function(){
            UpdateActiveToolTip();
        },
        "MouseEventToolTip" : MouseEventToolTip,
        SetD3FaultsMap: function(paramD3FaultsMap) {
            d3FaultsMapLocal = paramD3FaultsMap;
        }

    };

}
