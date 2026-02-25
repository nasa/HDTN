/**
 * @file d3_context_menu.js
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
 * The d3_context_menu library is a closure that supports a right click context menu
 * that appears when a right click occurs on a supported d3 svg object.
 */

function contextMenu() {
    var height,
        width,
        margin = 0.1, // fraction of width
        items = [],
        rescale = false,
        style = {
            'rect': {
                'mouseout': {
                    'fill': 'rgb(244,244,244)',
                    'stroke': 'white',
                    'stroke-width': '1px'
                },
                'mouseover': {
                    'fill': 'rgb(200,200,200)'
                }
            },
            'text': {
                'fill': 'steelblue'
            }
        };

    function menu(x, y) {
        d3.select('.context-menu').remove();
        scaleItems();

        // Draw the menu
        d3.select('svg')
            .append('svg:g').attr('class', 'context-menu')
            .selectAll('.context_text_tmp')
            .data(items, function(d) {
                return d.text;
            })
            .enter()
            .append('svg:g').attr('class', 'menu-entry')
            .on('mouseover', function(){
                d3.select(this).select('rect').style('fill', 'rgb(200,200,200)');
            })
            .on('mouseout', function(){
                d3.select(this).select('rect').style('fill', 'rgb(244,244,244)');
            })
            .style('cursor', 'pointer');

        d3.selectAll('.menu-entry')
            .append('svg:rect')
            .attr('x', x)
            .attr('y', function(d, i){ return y + (i * height); })
            .attr('width', width)
            .attr('height', height)
            .style('fill', 'rgb(244,244,244)')
            .style('stroke', 'white')
            .style('stroke-width', '1px')
            .on('click', function(d) {
                d.func();
            });
            //.style(style.rect.mouseout);

        d3.selectAll('.menu-entry')
            .append('svg:text')
            .text(function(d){ return d.text; })
            .attr('x', x)
            .attr('y', function(d, i){ return y + (i * height); })
            .attr('dy', height - margin / 2)
            .attr('dx', margin)
            .style('fill', 'steelblue');
            //.style(style.text);

        // Other interactions
        d3.select('body')
            .on('click', function() {
                d3.select('.context-menu').remove();
            });

    }

    menu.items = function(e) {
        if (!arguments.length) return items;
        for (i in arguments) items.push(arguments[i]);
        rescale = true;
        return menu;
    }

    // Automatically set width, height, and margin;
    function scaleItems() {
        if (rescale) {
            //draw this in a separate svg outside the viewbox so getBoundingClientRect() won't return scaled results
            d3.select("#hiddenTextMeasurementDiv").append('svg').selectAll('tmp')
                .data(items, function(d) {
                    return d.text;
                })
                .enter()
                .append('svg:text')
                .attr('x', -1000)
                .attr('y', -1000)
                .attr('class', 'tmp')
                .text(function(d){ return d.text; });


            var allWidth = [], allHeight = [];
            d3.selectAll('text.tmp')
                .each(function(d) {
                    var boundingRect = d3.select(this).node().getBoundingClientRect();
                    allWidth.push(boundingRect.width);
                    allHeight.push(boundingRect.height);
                })


            width = d3.max(allWidth);
            margin = margin * width;
            width =  width + 2 * margin;
            height = d3.max(allHeight) + margin / 2;

            // cleanup
            d3.select("#hiddenTextMeasurementDiv").selectAll('svg').remove();
            rescale = false;
        }
    }

    return menu;
}
