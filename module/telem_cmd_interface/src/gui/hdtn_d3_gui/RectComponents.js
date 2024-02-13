/**
 * @file RectComponents.js
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
 * The RectComponents library is a closure that represents an svg rect that also
 * contains child rects (child rects are represented in parentObj.d3ChildArray).
 */

function RectComponents(paramSvgRootGroup, paramSvgRootGroupClass, paramSvgChildGroupClass,
    paramParentRectStyleClass, paramHideParent, paramMouseEventToolTipFunction, paramContextMenuEventFunction,
    paramGetParentRectClassFunction, paramGetChildRectClassFunction, paramIsTextAnchorEnd) {


    var svgRootGroup = paramSvgRootGroup;
    var svgRootGroupClass = paramSvgRootGroupClass;
    var svgChildGroupClass = paramSvgChildGroupClass;
    var parentRectStyleClass = paramParentRectStyleClass;
    var hideParent = paramHideParent;
    var mouseEventToolTipFunction = paramMouseEventToolTipFunction;
    var contextMenuEventFunction = paramContextMenuEventFunction;
    var getParentRectClassFunction = paramGetParentRectClassFunction;
    var getChildRectClassFunction = paramGetChildRectClassFunction;
    var isTextAnchorEnd = paramIsTextAnchorEnd;



    function DoUpdate(paramD3Array, paramSharedTransition) {
        var select = svgRootGroup.selectAll("." + svgRootGroupClass)
            .data(paramD3Array, function(d) {
                return d.id;
            });

        var enter = select.enter().append("svg:g")
            .attr("class", function(d) {
                return svgRootGroupClass;
            })
            .attr("transform", function(d) {
                return "translate(" + d.absX + "," + d.absY + ")";
            });
            //.on("click", LeftClick)
            //.on("contextmenu", RightClick);


        enter.append("svg:rect")
            .attr("width", function(d) {
                return d.width;
            })
            .attr("height", function(d) {
                return 0;
            })
            .attr("rx", 5)
            .attr("ry", 5)
            .attr("class", parentRectStyleClass === "" ? getParentRectClassFunction : parentRectStyleClass)
            .style("opacity", hideParent ? 0 : 1) //hide loadbank orus
            .on("mouseover mousemove mouseout", mouseEventToolTipFunction)
            .on("contextmenu", contextMenuEventFunction);

        enter.each(function(parentObj, i) {
            if(parentObj.hasOwnProperty("busBar")) {
                d3.select(this).append("svg:line")
                    .attr("class", "bus_bar")
                    .attr("x1", parentObj.busBar.x1)
                    .attr("x2", parentObj.busBar.x1)
                    .attr("y1", parentObj.busBar.y1)
                    .attr("y2", parentObj.busBar.y1);
            }
        });

        function GetParentTransform(d) {
            return "translate(" + 5 + "," + (d.hasOwnProperty("d3ChildArray") ? d.topHeaderHeight/2 : d.height/2) + ")";
        }

        enter.append("svg:text")
            .attr("dominant-baseline", "central")//.attr("dominant-baseline", "middle")//.attr("dy", ".35em")
            //.attr("text-anchor", "end")
            .attr("transform", GetParentTransform)
            .style("opacity", hideParent ? 0 : 1) //hide loadbank orus
            .text(function(d) {
                return d.hasOwnProperty("name") ? d.name : d.id;
            });


        var update = enter.merge(select).transition(paramSharedTransition)
            .attr("transform", function(d) {
                return "translate(" + d.absX + "," + d.absY + ")";
            });

        update.select("line.bus_bar")
            .attr("x2", function(d) {
                return d.busBar.x2;
            })
            .attr("y2", function(d) {
                return d.busBar.y2;
            });


        update.select("rect")
            .attr("width", function(d) {
                return d.width;
            })
            .attr("height", function(d) {
                return d.height;
            })
            .attr("class", parentRectStyleClass === "" ? getParentRectClassFunction : parentRectStyleClass);

        update.select("text")
            .attr("transform", GetParentTransform);

        //should not be needed:
        //update.select("text")
        //    .text(function(d) {
        //        return d.id;
        //    });

        update.each(function(parentObj, i) {
            if(parentObj.hasOwnProperty("d3ChildArray")) {
                var selChild = d3.select(this).selectAll("." + svgChildGroupClass)
                    .data(parentObj.d3ChildArray, function(childObj) {
                        return childObj.id;
                    });
                var childEnter = selChild.enter()
                    .append("svg:g")
                    .attr("class", svgChildGroupClass)
                    .attr("transform", function(childObj) {
                        return "translate(" + childObj.relX + "," + childObj.relY + ")";
                    });

                childEnter.append("svg:rect")
                    .attr("width", function(childObj) {
                        return childObj.width;
                    })
                    .attr("height", function(d) {
                        return 0;
                    })
                    .attr("class", getChildRectClassFunction)
                    .attr("rx", 5)
                    .attr("ry", 5)
                    .on("mouseover mousemove mouseout", mouseEventToolTipFunction)
                    .on("contextmenu", contextMenuEventFunction);

                function GetChildTextTransform(childObj) {
                    return "translate(" + (isTextAnchorEnd ? (childObj.width - 5) : 5) + "," + childObj.height/2 + ")";
                }

                childEnter.append("svg:text")
                    .attr("dominant-baseline", "central")//.attr("dominant-baseline", "middle")//.attr("dy", ".35em")
                    .attr("text-anchor", (isTextAnchorEnd) ? "end" : "start")
                    .attr("transform", GetChildTextTransform)
                    .text(function(childObj) {
                        return childObj.hasOwnProperty("name") ? childObj.name : childObj.id;
                    });


                var childUpdate = childEnter.merge(selChild).transition(paramSharedTransition)
                    .attr("transform", function(childObj) {
                        return "translate(" + childObj.relX + "," + childObj.relY + ")";
                    });

                childUpdate.select("rect")
                    .attr("class", getChildRectClassFunction)
                    .attr("width", function(childObj) {
                        return childObj.width;
                    })
                    .attr("height", function(childObj) {
                        return childObj.height;
                    });
                childUpdate.select("text")
                    .attr("transform", GetChildTextTransform);

                var childExit = selChild.exit().transition(paramSharedTransition).remove();

                childExit.select("rect")
                    .attr("height", function(d) {
                        return 0;
                    });

                childExit.select("text")
                    .style("opacity", 0);
            }
        });


        // Transition exiting nodes to the parent's new position.
        var exit = select.exit().transition(paramSharedTransition)
            .remove();

        exit.select("rect")
            .attr("height", function(d) {
                return 0;
            });

        exit.select("text")
            .style("opacity", 0);

        exit.select("line.bus_bar")
            .attr("x2", function(d) {
                return d.busBar.x1;
            })
            .attr("y2", function(d) {
                return d.busBar.y1;
            });


        exit.each(function(parentObj, i) {
            d3.select(this).selectAll("." + svgChildGroupClass).remove();
        });

    }


    return {
        Update: function(paramD3Array, paramSharedTransition){
            DoUpdate(paramD3Array, paramSharedTransition);
        }
    };

}
