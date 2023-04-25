/**
 * @file ImageComponents.js
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
 * The ImageComponents library is a closure that displays an external svg image.
 */

function ImageComponents(paramSvgRootGroup, paramX, paramY, paramWidth, paramHeight, paramImageUrl) {


    var svgRootGroup = paramSvgRootGroup;
    var imageUrl = paramImageUrl;
    var x = paramX;
    var y = paramY;
    var width = paramWidth;
    var height = paramHeight;

    var imageGroup = svgRootGroup.append("svg:g")
        .attr("transform", "translate(" + x + "," + y +")")
        .attr("class", "disk_node_group");



    imageGroup.append("svg:image")
        //.attr("xlink:href",  imageUrl) //deprecated, use href
        .attr("href",  imageUrl)
        .attr("width", width)
        .attr("height", height);
        //.on("mouseover mousemove mouseout", mouseEventToolTipFunction);


    function GetRightWirePosition() {
        return {
            x: x + width,
            y: y + height * .5
        };
    }
    function GetLeftWirePosition() {
        return {
            x: x,
            y: y + height * .5
        };
    }
    function GetBottomWirePosition() {
        return {
            x: x + width * .5,
            y: y + height
        };
    }
    function GetTopWirePosition() {
        return {
            x: x + width * .5,
            y: y
        };
    }


    return {
        GetWirePos: function(posStr) {
            if(posStr === "right") return GetRightWirePosition();
            if(posStr === "left") return GetLeftWirePosition();
            if(posStr === "top") return GetTopWirePosition();
            if(posStr === "bottom") return GetBottomWirePosition();
        }
    };

}
