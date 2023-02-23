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
        var textStr = "";
        if(obj.hasOwnProperty("toolTipText")) {
            textStr = obj.toolTipText;
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
