//struct for HDTN Data Rate graph
var rate_data = [{
    x: [],
    y: [],
    type: 'scatter',
    line: {
        color: "aqua"
    }
}];

var rateCount = 0;

//Style for HDTN Data Rate Graph
var layout = {
    title: 'HDTN Data Rate',
    paper_bgcolor: "#404040",
    plot_bgcolor: "#404040",
    xaxis: {
        title: "Timestamp (s)",
    },
    yaxis: {
        title: "Data Rate (Mbps)",
    },
    font:{
        family: "Arial",
        size: 18,
        color: "white"
    }
};

//Launch Data Graph
GRAPH = document.getElementById("data_rate_graph");
Plotly.newPlot(GRAPH, rate_data, layout);

//Struct for Pie Chart of bundle destinations
var pie_data = [{
    values: [29,71],
    labels: ['Storage', 'Egress'], 
    type: 'pie'
}];

//Style for Pie Chart
var pie_layout = {
    title: 'Bundle Destinations',
    height: 500,
    width:500,
    paper_bgcolor: "#404040",
    font:{
        family: "Arial",
        size: 18,
        color: "white"
    }
};

//Launch Pie Chart
Plotly.newPlot('storage_egress_chart', pie_data, pie_layout);

//Runs when JSON Message is received
//Updates Graphs and Tables based on the new data
function UpdateData(objJson){
    if("bundleDataRate" in objJson){
        rate_data[0]['x'].push(rateCount++);
        rate_data[0]['y'].push(parseFloat(objJson.bundleDataRate));
        console.log(rate_data[0]['y']);
        Plotly.update(GRAPH, rate_data, layout);
        document.getElementById("rate_data").innerHTML = parseFloat(objJson.bundleDataRate).toFixed(3);
        document.getElementById("max_data").innerHTML = Math.max.apply(Math, rate_data[0].y).toFixed(3);
    }
    if("averageRate" in objJson){
        document.getElementById("avg_data").innerHTML = parseFloat(objJson.averageRate).toFixed(3);
    }
    if("totalData" in objJson){
        document.getElementById("ingressBundleData").innerHTML = parseFloat(objJson.totalData).toFixed(2);
    }
    if("bundleCountStorage" in objJson && "bundleCountEgress" in objJson){
        pie_data[0]['values'] = [parseInt(objJson.bundleCountStorage), parseInt(objJson.bundleCountEgress)];
        Plotly.update('storage_egress_chart', pie_data, pie_layout);

        document.getElementById("ingressBundleCountStorage").innerHTML = objJson.bundleCountStorage;
        document.getElementById("ingressBundleCountEgress").innerHTML = objJson.bundleCountEgress;
        document.getElementById("ingressBundleCount").innerHTML = parseInt(objJson.bundleCountStorage) + parseInt(objJson.bundleCountEgress);

    }
    if("egressBundleCount" in objJson){
        document.getElementById("egressBundleCount").innerHTML = objJson.egressBundleCount;
    }
    if("egressBundleData" in objJson){
        document.getElementById("egressBundleData").innerHTML = parseFloat(objJson.egressBundleData).toFixed(2);
    }
    if("egressMessageCount" in objJson){
        document.getElementById("egressMessageCount").innerHTML = objJson.egressMessageCount;
    }
    if("totalBundlesErasedFromStorage" in objJson){
        document.getElementById("totalBundlesErasedFromStorage").innerHTML = objJson.totalBundlesErasedFromStorage;
    }
    if("totalBundlesSentToEgressFromStorage" in objJson){
        document.getElementById("totalBundlesSentToEgressFromStorage").innerHTML = objJson.totalBundlesSentToEgressFromStorage;
    }

}

window.addEventListener("load", function(event){
    if(!("WebSocket" in window)){
        alert("WebSocket is not supported by your Browser!");
    }

var wsproto = (location.protocol === 'http:') ? 'ws:' : 'ws:';
connection = new WebSocket(wsproto + '//' + window.location.host + '/websocket');
connection.binaryType = "arraybuffer";

//When connection is established
connection.onopen = function(){
    console.log("ws opened");
}

//When connection is closed
connection.onclose = function(){
    console.log("ws closed");
}

//When client receives data from server
connection.onmessage = function(e){
    console.log("rcvd");
    //if binary data
    if(e.data instanceof ArrayBuffer){
        var dv = new DataView(e.data);
    }
    //else this is text data
    else{
        if(e.data === "Hello websocket"){
            console.log(e.data);
        }
        else{
            try{
                console.log(e.data);
                var obj = JSON.parse(e.data); //could error based on encodings
                console.log("JSON data parsed");
                UpdateData(obj);
            }
            catch(err){
                console.log(err.message);
            }
        }
    }
}
});

