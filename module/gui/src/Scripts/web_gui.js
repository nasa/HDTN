//struct for HDTN Data Rate graph
var rate_data_ingress = [{
    x: [],
    y: [],
    name: 'ingress',
    type: 'scatter',
    line: {
        color: "aqua"
    }
}];

var rate_data_egress = [{
    x: [],
    y: [],
    name: 'egress',
    type: 'scatter',
    line: {
        color: "magenta"
    }
}];

//Style for HDTN Data Rate Graph
var ingressLayout = {
    title: 'Ingress Data Rate',
    paper_bgcolor: "#404040",
    plot_bgcolor: "#404040",
    width: 700,
    height: 450,
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

var egressLayout = {
    title: 'Egress Data Rate',
    paper_bgcolor: "#404040",
    plot_bgcolor: "#404040",
    width: 700,
    height: 450,
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

//Launch Data Graphs
Plotly.newPlot("ingress_rate_graph", rate_data_ingress, ingressLayout, { displaylogo: false });
Plotly.newPlot("egress_rate_graph", rate_data_egress, egressLayout, { displaylogo: false });


//Struct for Pie Chart of bundle destinations
var pie_data = [{
    //add variables for storage: egress ratio
    values: [0,0],
    labels: ['Storage', 'Egress'], 
    type: 'pie'
}];

//Style for Pie Chart
var pie_layout = {
    title: 'Bundle Destinations',
    height: 450,
    width:450,
    paper_bgcolor: "#404040",
    font:{
        family: "Arial",
        size: 18,
        color: "white"
    }
};

//Launch Pie Chart
Plotly.newPlot('storage_egress_chart', pie_data, pie_layout, { displaylogo: false });

var ingressRateCount = 0;
var egressRateCount = 0;
var egressBundleCount = 0;
var egressRate = 0;
var startTime = new Date().getTime()*1000;
var lastEgressData = 0;
var lastTime = startTime;
var lastEgressData = 0;

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
        
                rate_data_ingress[0]['x'].push(ingressRateCount++);
                rate_data_ingress[0]['y'].push(rate);
                Plotly.update("ingress_rate_graph", rate_data_ingress, ingressLayout);
        	pie_data[0]['values'] = [bundleCountStorage, bundleCountEgress];
		Plotly.update('storage_egress_chart', pie_data, pie_layout);
                document.getElementById("rate_data").innerHTML = rate.toFixed(3);
                document.getElementById("max_data").innerHTML = Math.max.apply(Math, rate_data_ingress[0].y).toFixed(3);
                document.getElementById("avg_data").innerHTML = averageRate.toFixed(3);
                document.getElementById("ingressBundleData").innerHTML = totalData.toFixed(2);              
	        document.getElementById("ingressBundleCountStorage").innerHTML = bundleCountStorage;
        	document.getElementById("ingressBundleCountEgress").innerHTML = bundleCountEgress;
        	document.getElementById("ingressBundleCount").innerHTML = bundleCountStorage + bundleCountEgress;

	    }
            else if(type == 2){
                //Egress
		egressBundleCount = dv.getUint64(byteIndex, littleEndian);
                byteIndex += 8;
                var egressBundleData = dv.getFloat64(byteIndex, littleEndian);
                byteIndex += 8;
                var egressMessageCount = dv.getUint64(byteIndex, littleEndian);

		var newTime = new Date().getTime()*1000;
		var duration = newTime - lastTime;
		lastTime = newTime;
		egressRate = (8.0 * (egressBundleData - lastEgressData) * 1000)/duration;
		lastEgressData = egressBundleData;

		rate_data_egress[0]['x'].push(egressRateCount++);
                rate_data_egress[0]['y'].push(egressRate);
                Plotly.update("egress_rate_graph", rate_data_egress, egressLayout);

		document.getElementById("egressDataRate").innerHTML = egressRate.toFixed(3);
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



