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

    var wsproto = (location.protocol === 'https:') ? 'wss:' : 'ws:';
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
            console.log("Binary Data Received");

            littleEndian = true;
        //https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/DataView
        DataView.prototype.getUint64 = function(byteOffset, littleEndian) {
            // split 64-bit number into two 32-bit (4-byte) parts
            const left =  this.getUint32(byteOffset, littleEndian);
            const right = this.getUint32(byteOffset+4, littleEndian);

            // combine the two 32-bit values
            const combined = littleEndian? left + 2**32*right : 2**32*left + right;

            if (!Number.isSafeInteger(combined))
                console.warn(combined, 'exceeds MAX_SAFE_INTEGER. Precision may be lost');

            return combined;
        }
    
            var dv = new DataView(e.data);
            var byteIndex = 0;
            var type = dv.getUint64(byteIndex, littleEndian);
            byteIndex += 8;
        
            if(type == 1){
                //Ingress
                var rate = dv.getFloat64(byteIndex, littleEndian);
                byteIndex += 8;
                var averageRate = dv.getFloat64(byteIndex, littleEndian);
                byteIndex += 8;
                var totalData = dv.getFloat64(byteIndex, littleEndian);
                byteIndex += 8;
                var bundleCountEgress = dv.getUint64(byteIndex, littleEndian);
                byteIndex += 8;
                var bundleCountStorage = dv.getUint64(byteIndex, littleEndian);
        
        
                rate_data[0]['x'].push(rateCount++);
                rate_data[0]['y'].push(rate);
                console.log(rate_data[0]['y']);
                Plotly.update(GRAPH, rate_data, layout);
                document.getElementById("rate_data").innerHTML = rate.toFixed(3);
                document.getElementById("max_data").innerHTML = Math.max.apply(Math, rate_data[0].y).toFixed(3);
                document.getElementById("avg_data").innerHTML = averageRate.toFixed(3);
                document.getElementById("ingressBundleData").innerHTML = totalData.toFixed(2);
                pie_data[0]['values'] = [bundleCountStorage, bundleCountEgress];
                Plotly.update('storage_egress_chart', pie_data, pie_layout);
                document.getElementById("ingressBundleCountStorage").innerHTML = bundleCountStorage;
                document.getElementById("ingressBundleCountEgress").innerHTML = bundleCountEgress;
                document.getElementById("ingressBundleCount").innerHTML = bundleCountStorage + bundleCountEgress;
            }
            else if(type == 2){
                //Egress
                var egressBundleCount = dv.getUint64(byteIndex, littleEndian);
                byteIndex += 8;
                var egressBundleData = dv.getFloat64(byteIndex, littleEndian);
                byteIndex += 8;
                var egressMessageCount = dv.getUint64(byteIndex, littleEndian);
        
                document.getElementById("egressBundleCount").innerHTML = egressBundleCount;
                document.getElementById("egressBundleData").innerHTML = egressBundleData.toFixed(2);
                document.getElementById("egressMessageCount").innerHTML = egressMessageCount;
            }
            else if(type == 3){
                //Storage
                var totalBundlesErasedFromStorage = dv.getUint64(byteIndex, littleEndian);
                byteIndex += 8;
                var totalBundlesSentToEgressFromStorage = dv.getUint64(byteIndex, littleEndian);
                document.getElementById("totalBundlesErasedFromStorage").innerHTML = totalBundlesErasedFromStorage;
                document.getElementById("totalBundlesSentToEgressFromStorage").innerHTML = totalBundlesSentToEgressFromStorage;
            }


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

