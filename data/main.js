var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
window.addEventListener('load', onLoad);
let timetable_data = [];

function initWebSocket() {
    console.log('opening websocket connnection...');
    websocket = new WebSocket(gateway);
    websocket.onopen    = onOpen;
    websocket.onclose   = onClose;
    websocket.onmessage = onMessage;
    websocket.onerror   = onError;
}

function onOpen(event) {
    console.log('connection opened');
    document.getElementsByTagName("h1")[0].style.background = "aqua";
}

function onClose(event) {
    console.log('connection closed');
    document.getElementsByTagName("h1")[0].style.background = "orange";
    setTimeout(initWebSocket, 2000);
}

function onError(error) {
    console.log('error');
    document.getElementsByTagName("h1")[0].style.background = "red";
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
            <th style="width: 50%;">Name</th>
            <th style="width: 40%;">ISIC</th>
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
        timetable_data = data;

        document.getElementById("timeTable").innerHTML = `
        <tr>
            <th style="width: 10%;">№</th>
            <th style="width: 50%;">Name</th>
            <th style="width: 40%;">Time</th>
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
    document.getElementById("databaseTable").style.display = "table";
    document.getElementById("reloadButton").style.display = "block";
    document.getElementById("loadButton").style.display = "none";
}

function reloadTable() {
    websocket.send("RELOAD_TABLE");
}

function loadTimetable() {
    websocket.send("RELOAD_TIMETABLE");
    document.getElementById("timeTable").style.display = "table";
    document.getElementById("reloadTTButton").style.display = "block";
    document.getElementById("clearTTButton").style.display = "block";
    document.getElementById("saveTTBox").style.display = "flex";
    document.getElementById("loadTTButton").style.display = "none";
}

function reloadTimetable() {
    websocket.send("RELOAD_TIMETABLE");
}

function clearTimetable() {
    websocket.send("CLEAR_TIMETABLE");
    reloadTimetable();
}

function onLoad(event) {
    initWebSocket();
    let trs = document.getElementsByTagName("tr");
    for (let i = 0; i < trs.length; i++) {
        trs[i].addEventListener("click", function(event) {
            if (event.currentTarget.className == "selected") {
                event.currentTarget.className = "";
                return;
            } else {
                event.currentTarget.className = "selected";
            }
        });
    }
}

function saveTimetable() {
    let res = download_as_csv();
    let mesElement = document.getElementById("save-timetable-message");
    mesElement.style.color = res.startsWith("ERR") ? "red" : "green";
    mesElement.innerHTML = res.split(":")[1];
    mesElement.style.opacity = 1;
}

function download_as_csv() {
    if (timetable_data.length == 0) return "ERR:timetable not loaded";
    let text = "num,name,time\n";
    for (let i = 1; i < timetable_data.length; i++)
        text += `${i},${timetable_data[i].split("|")[0]},${timetable_data[i].split("|")[1]}\n`;

    let a = document.createElement('a');
    
    const date = new Date();
    const pad = (num, totalLength) => String(num).padStart(totalLength, '0');
    a.href = 'data:attachment/text,' + encodeURI(text);
    a.target = '_blank';
    a.download = `timetable-${pad(date.getDate(), 2)}${pad(date.getMonth() + 1, 2)}${date.getFullYear()}.csv`;
    a.click();
    return "OK:successfully saved";
}