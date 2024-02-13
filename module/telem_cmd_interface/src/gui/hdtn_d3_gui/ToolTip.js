/**
 * @file ToolTip.js
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2021 United States Government as represented by
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
    const MARGIN_PX = 5;

    var toolTipGroup = d3.select("#tool_tip_group_id");

    var toolTipTransformGroup = toolTipGroup.append("svg:g")
        .style("opacity", 0);
    var rectTooltip = toolTipTransformGroup.append("svg:rect")
        .attr("class", "tooltip");
    var textTooltip = toolTipTransformGroup.append("svg:text");
        //.attr("class", "tooltip")
        //.style("fill", "red")
        //.style("opacity", 0);


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

        textTooltip.selectAll('tspan').remove();
        const lineArray = textStr.split("<br />");
        lineArray.forEach(function(line) {
            textTooltip.append('tspan')
                //.attr("class", "wire_tspan_above")
                .attr('dy', "1.1em")
                .attr('x', MARGIN_PX)
                .text(line);
        });



        //draw this in a separate svg outside the viewbox so getBoundingClientRect() won't return scaled results
        let svgMeas = d3.select("#hiddenTextMeasurementDiv").append('svg')
            .attr('x', -1000)
            .attr('y', -1000);

        svgMeas.append(() => textTooltip.clone(true).node());
        const rect = svgMeas.select('text').node().getBoundingClientRect();
        // cleanup
        svgMeas.remove();


        const MARGINX2_PX = 2 * MARGIN_PX;
        const WIDTH = rect.width + MARGINX2_PX;
        const HEIGHT = rect.height + MARGINX2_PX;
        rectTooltip
            .style("width", WIDTH + "px")
            .style("height", HEIGHT + "px")
            .style("rx", MARGINX2_PX + "px");


        if(isMouseEvent) {
            const mouseX = d3.mouse(toolTipGroup.node())[0];
            const mouseY = d3.mouse(toolTipGroup.node())[1];
            let yCalc = Math.max(mouseY - (HEIGHT), 0);
            toolTipTransformGroup
                .attr("transform", "translate(" + (mouseX+2) + "," + yCalc + ")");
        }

        //range.detach(); // frees up memory in older browsers
    }
    function MouseEventToolTip(d) {
        if(d3.event.type === "mouseover") {
            if(d.hasOwnProperty("toolTipText")) {
                toolTipTransformGroup.transition()
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
                toolTipTransformGroup.transition()
                    .duration(200)
                    .style("opacity", 0);
            }
            else {
                toolTipTransformGroup.style("opacity", 0);
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
