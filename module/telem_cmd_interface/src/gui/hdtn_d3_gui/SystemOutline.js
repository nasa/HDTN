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
        SetDiskUsage: function(percentage){
            SetUsage(percentage);
        }
    };
}


