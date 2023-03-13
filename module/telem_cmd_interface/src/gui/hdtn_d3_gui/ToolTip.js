/**
 * @file ToolTip.js
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
 * The ToolTip library is a closure that supports a hover tooltip svg semi-transparent rect
 * that appears when a hover occurs on a supported d3 svg object.
 */

function ToolTip() {

    var globalToolTipObject = null;

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
        divTooltip
            .style("width", "0px")
            .style("height", "0px");

        //https://stackoverflow.com/questions/1461059/is-there-an-equivalent-to-getboundingclientrect-for-text-nodes
        let range = document.createRange();
        range.selectNode(divTooltip.node());
        const rect = range.getBoundingClientRect();
        divTooltip
            .style("width", rect.width + "px")
            .style("height", rect.height + "px");


        if(isMouseEvent) {
            divTooltip.style("left", (d3.event.pageX) + "px");
            divTooltip.style("top", (d3.event.pageY - (rect.height/2.0)) + "px");
        }

        range.detach(); // frees up memory in older browsers
    }
    function MouseEventToolTip(d) {
        if(d3.event.type === "mouseover") {
            if(d.hasOwnProperty("toolTipText")) {
                divTooltip.transition()
                    .duration(200)
                    .style("opacity", .9);
                UpdateToolTipText(d, true);
            }
        }
        else if(d3.event.type === "mousemove") {
            UpdateToolTipText(d, true);
        }
        else if(d3.event.type === "mouseout") {
            if(d.hasOwnProperty("toolTipText")) {
                divTooltip.transition()
                    .duration(200)
                    .style("opacity", 0);
            }
            else {
                divTooltip.style("opacity", 0);
            }
        }
    }


    return {
        Update: function(){
            UpdateActiveToolTip();
        },
        "MouseEventToolTip" : MouseEventToolTip

    };

}
