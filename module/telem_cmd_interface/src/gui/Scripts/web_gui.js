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
    width: 500,
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
    width: 500,
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

// Supported convergence layers
const StcpConvergenceLayer = 1;
const LtpConvergenceLayer = 2;

// Given a data view and a byte index, updates an HTML element with common outduct data
updateElementWithCommonOutductData = (htmlElement, outductTelem) => {
    const totalBundlesAcked = outductTelem.totalBundlesAcked;
    htmlElement.querySelector("#totalBundlesAcked").innerHTML = totalBundlesAcked.toFixed();
    const totalBytesAcked = outductTelem.totalBundleBytesAcked;
    htmlElement.querySelector("#totalBytesAcked").innerHTML = totalBytesAcked.toFixed();
    const totalBundlesSent = outductTelem.totalBundlesSent;
    htmlElement.querySelector("#totalBundlesSent").innerHTML = totalBundlesSent.toFixed();
    const totalBytesSent = outductTelem.totalBundleBytesSent;
    htmlElement.querySelector("#totalBytesSent").innerHTML = totalBytesSent.toFixed();
    const totalBundlesFailedToSend = outductTelem.totalBundlesFailedToSend;
    htmlElement.querySelector("#totalBundlesFailedToSend").innerHTML = totalBundlesFailedToSend.toFixed();
}


function createOutductCard(outductTelem, outductPos) {
    switch(outductTelem.convergenceLayer){
        case "ltp_over_udp":{
            var convergenceLayer = "LTP";
            break;}
        case "stcp":{
            var convergenceLayer = "STCP";
            break;}
        case "tcpcl_v4":{
            var convergenceLayer = "TCPCLv4";
            break;}
        case "tcpcl_v3":{
            var convergenceLayer = "TCPCLv3";
            break;}
        case "udp":{
            var convergenceLayer = "UDP";
            break;}
    }
    const uniqueId = convergenceLayer + outductPos;
    var card = document.getElementById(uniqueId);
    if (!card) {                                    
        const template = document.getElementById("outductTemplate"); 
        card = template.cloneNode(true);
        card.id = uniqueId;
        card.classList.remove("hidden");
        template.parentNode.append(card);
        card.querySelector("#convergenceLayer").innerHTML = convergenceLayer;
        var displayName = "Outduct " + (outductPos + 1).toFixed();
        card.querySelector("#cardName").innerHTML = displayName;
    }

    var newTableData="";

    for (var key in outductTelem) {
        var newRow;
        if (isNaN(outductTelem[key])) {
            newRow ="<tr>" + "<td>" + key + "</td>" + "<td>" + (outductTelem[key]).toLocaleString('en') + "</td>" + "</tr>";
        }
        else {
            newRow ="<tr>" + "<td>" + key + "</td>" + "<td>" + (outductTelem[key]).toLocaleString('en') + "</td>" + "</tr>";
        }
        newTableData += newRow;
    }
    card.querySelector("#cardTableBody").innerHTML = newTableData;
}

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

var ingressRateCalculator = new RateCalculator();
var egressRateCalculator = new RateCalculator();
var hdtnConfig = null;

function InternalUpdateWithData(objJson) {
    //console.log(objJson);
    if("hdtnConfigName" in objJson) {
        hdtnConfig = objJson;
    }
    else if("allInducts" in objJson) {
        //Ingress
        const timestampMilliseconds = objJson.timestampMilliseconds;
        const totalDataBytes = objJson.bundleByteCountEgress + objJson.bundleByteCountStorage;
        const bundleCountEgress = objJson.bundleCountEgress;
        const bundleCountStorage = objJson.bundleCountStorage;

        ingressRateCalculator.appendVal(totalDataBytes, timestampMilliseconds);
        if (ingressRateCalculator.count == 1) {
            // Don't plot anything the first time through, since we don't have
            // sufficient data
            return;
        }

        rate_data_ingress[0]['x'].push(ingressRateCalculator.count);
        rate_data_ingress[0]['y'].push(ingressRateCalculator.currentMbpsRate);
        Plotly.update("ingress_rate_graph", rate_data_ingress, ingressLayout);
        pie_data[0]['values'] = [bundleCountStorage, bundleCountEgress];
        Plotly.update('storage_egress_chart', pie_data, pie_layout);
        document.getElementById("rate_data").innerHTML = ingressRateCalculator.currentMbpsRate.toFixed(3);
        document.getElementById("max_data").innerHTML = Math.max.apply(Math, rate_data_ingress[0].y).toFixed(3);
        document.getElementById("avg_data").innerHTML = ingressRateCalculator.averageMbpsRate.toFixed(3);
        document.getElementById("ingressBundleData").innerHTML = totalDataBytes.toFixed(2);
        document.getElementById("ingressBundleCountStorage").innerHTML = bundleCountStorage;
        document.getElementById("ingressBundleCountEgress").innerHTML = bundleCountEgress;
        document.getElementById("ingressBundleCount").innerHTML = bundleCountStorage + bundleCountEgress;
    }
    else if("allOutducts" in objJson) {
        //Egress
        const timestampMilliseconds = objJson.timestampMilliseconds;
        const egressBundleCount = objJson.totalBundlesSuccessfullySent;
        const egressTotalDataBytes = objJson.totalBundleBytesSuccessfullySent;
        const egressMessageCount = objJson.totalBundlesGivenToOutducts;

        egressRateCalculator.appendVal(egressTotalDataBytes, timestampMilliseconds);
        if (egressRateCalculator.count == 1) {
            // Don't plot anything the first time through, since we don't have
            // sufficient data
            return;
        }

        rate_data_egress[0]['x'].push(egressRateCalculator.count);
        rate_data_egress[0]['y'].push(egressRateCalculator.currentMbpsRate);
        Plotly.update("egress_rate_graph", rate_data_egress, egressLayout);

        document.getElementById("egressDataRate").innerHTML = egressRateCalculator.currentMbpsRate.toFixed(3);
        document.getElementById("egressBundleCount").innerHTML = egressBundleCount;
        document.getElementById("egressBundleData").innerHTML = egressTotalDataBytes.toFixed(2);
        document.getElementById("egressMessageCount").innerHTML = egressMessageCount;

        // Handle any outduct data
        objJson.allOutducts.forEach(function(outductTelem, i) {
            createOutductCard(outductTelem, i);
        });
    }
    else if("outductCapabilityTelemetryList" in objJson) {

    }
    else if("usedSpaceBytes" in objJson) {
        //Storage
        const timestampMilliseconds = objJson.timestampMilliseconds;
        const totalBundlesErasedFromStorage = objJson.totalBundlesErasedFromStorageNoCustodyTransfer + objJson.totalBundlesErasedFromStorageWithCustodyTransfer;
        const totalBundlesSentToEgressFromStorage = objJson.totalBundlesSentToEgressFromStorageReadFromDisk;
        const usedSpaceBytes = objJson.usedSpaceBytes;
        const freeSpaceBytes = objJson.freeSpaceBytes;
        document.getElementById("totalBundlesErasedFromStorage").innerHTML = totalBundlesErasedFromStorage;
        document.getElementById("totalBundlesSentToEgressFromStorage").innerHTML = totalBundlesSentToEgressFromStorage;
        document.getElementById("usedSpaceBytes").innerHTML = usedSpaceBytes;
        document.getElementById("freeSpaceBytes").innerHTML = freeSpaceBytes;
    }
    else {
        //HandleReceivedNewHdtnConfig(objJson);
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
        if(e.data instanceof ArrayBuffer) { //this is binary data
            console.log("binary data received.. ignoring");
        }
        else { //this is text data such as json
            if(e.data === "Hello websocket") {
                console.log(e.data);
            }
            else { //json data
                var obj = JSON.parse(e.data); //this could error based on encodings
                //console.log(obj);
                InternalUpdateWithData(obj);

            }
            //console.log(e.data);
        }
    }
});
