var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
window.addEventListener('load', onLoad);

function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen    = onOpen;
    websocket.onclose   = onClose;
    websocket.onmessage = onMessage;
}

function onOpen(event) {
    console.log('Connection opened');
    document.getElementsByTagName("h1")[0].style.background = "aqua";
}

function onClose(event) {
    console.log('Connection closed');
    document.getElementsByTagName("h1")[0].style.background = "orange";
    setTimeout(initWebSocket, 2000);
}

function onMessage(event) {
    if (event.data == "REQUEST_NAME") {
        document.getElementById("nameInput").style.display = "block";
        document.getElementById("lastName").style.display = "none";
        document.getElementById("nameInput").addEventListener("keyup", function(event) {
            if (event.key === "Enter") {
                websocket.send(document.getElementById("nameInput").value);
                document.getElementById("nameInput").style.display = "none";
                document.getElementById("lastName").innerHTML = "name submitted";
                document.getElementById("lastName").style.color = "green";
                document.getElementById("lastName").style.display = "block";
            }
        });
    } else if (event.data.startsWith("TABLE_DATA")) {
        // TABLE_DATA::name1|code1::name2|code2::name3|code3

        let data = event.data.split("::");

        document.getElementById("databaseTable").innerHTML = `
        <tr>
            <th style="width: 10%;">№</th>
            <th style="width: 60%;">Name</th>
            <th style="width: 30%;">ISIC</th>
        </tr>`;
        
        for (let i = 1; i < data.length; i++) {
            document.getElementById("databaseTable").innerHTML += `
            <tr>
                <td class="colNumber">${i}</td>
                <td class="colName">${data[i].split("|")[0]}</td>
                <td class="colISIC">${data[i].split("|")[1]}</td>
            </tr>
            `;
        }
    } else if (event.data.startsWith("TIMETABLE_DATA")) {
        // TIMETABLE_DATA::name1|time1::name2|time2::name3|time3

        let data = event.data.split("::");

        document.getElementById("timeTable").innerHTML = `
        <tr>
            <th style="width: 40px;">№</th>
            <th style="width: 240px;">Name</th>
            <th style="width: 180px;">Time</th>
        </tr>`;
        
        for (let i = 1; i < data.length; i++) {
            document.getElementById("timeTable").innerHTML += `
            <tr>
                <td class="colNumber">${i}</td>
                <td class="colName">${data[i].split("|")[0]}</td>
                <td class="colTime">${data[i].split("|")[1]}</td>
            </tr>
            `;
        }
    } else {
        var name = event.data.split(" | ")[0];
        var time = event.data.split(" | ")[1];
        document.getElementById("lastName").innerHTML = name;
        document.getElementById("lastName").style.color = "royalblue";
        document.getElementById("lastTime").innerHTML = time;
    }
}

function loadTable() {
    websocket.send("RELOAD_TABLE");
    document.getElementById("databaseTable").style.display = "block";
    document.getElementById("reloadButton").style.display = "block";
    document.getElementById("loadButton").style.display = "none";
}

function reloadTable() {
    websocket.send("RELOAD_TABLE");
}

function loadTimetable() {
    websocket.send("RELOAD_TIMETABLE");
    document.getElementById("timeTable").style.display = "block";
    document.getElementById("reloadTTButton").style.display = "block";
    document.getElementById("loadTTButton").style.display = "none";
}

function reloadTimetable() {
    websocket.send("RELOAD_TIMETABLE");
}

function onLoad(event) {
    initWebSocket();
}