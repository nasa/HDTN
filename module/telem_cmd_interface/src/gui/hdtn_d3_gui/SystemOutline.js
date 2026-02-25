/**
 * @file SystemOutline.js
 * @author  Brian Tomko <brian.j.tomko@nasa.gov>
 *
 * @copyright Copyright (c) 2026 United States Government as represented by
 * the National Aeronautics and Space Administration.
 * No copyright is claimed in the United States under Title 17, U.S.Code.
 * All Other Rights Reserved.
 *
 * @section LICENSE
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @section DESCRIPTION
 *
 * The SystemOutline library is a closure that draws a single dashed svg rectangle box
 * with a label in the top right corner.
 */

function SystemOutline(paramSvgRootGroup, paramX, paramY, paramWidth, paramHeight, paramLabel) {

    var svgRootGroup = paramSvgRootGroup;
    var x = paramX;
    var y = paramY;
    var width = paramWidth;
    var height = paramHeight;
    var label = paramLabel;

    var systemOutlineGroup = svgRootGroup.append("svg:g")
        .attr("class", "system_outline_group");


    //DRAW THE NODE
    systemOutlineGroup.append("svg:rect")
        .attr("x", x)
        .attr("y", y)
        .attr("width", width)
        .attr("height", height)
        .attr("class", "system_outline_group_rect")
        .attr("rx", 10)
        .attr("ry", 10)
        .attr("fill", "none")
        .style("stroke-width", 5)
        .attr("stroke-dasharray", "10 5");

    systemOutlineGroup.append("svg:text")
        .attr("class", "system_outline_group_text")
        .attr("dy", ".35em")
        //.attr("text-anchor", "end")
        .attr("transform", "translate(" + (x+20) + "," + (y+20) + ")")
        .text(label);




    return {
        UpdateName: function(newName) {
            label = newName;
            systemOutlineGroup.select(".system_outline_group_text").text(label);
        }
    };
}


