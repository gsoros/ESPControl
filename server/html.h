const char *html = R"====(<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">

</head>
<body>

<div style="width: 100%; height: 100vh; text-align: center">
    <input type="range" min="-100" max="100" value="0" class="slider" id="range" style="width: 100%; height: 20%">
    <p id="output"></p>
    <button type="button" id="start" style="width: 45%; height: 20%">Start</button>
    <button type="button" id="stop" style="width: 45%; height: 20%">Stop</button>
    <p style:"margin-top: 5%" id="message"></p>
</div>

<script>
var slider = document.getElementById("range");
var output = document.getElementById("output");
var message = document.getElementById("message");
var start = document.getElementById("start");
var stop = document.getElementById("stop");
var vector = 0;
var run = false;
var rate = 500; // 2 commands/sec
var lastCommand = 0;
var nextCommand = 0;
output.innerHTML = slider.value;

var req = new XMLHttpRequest();
req.addEventListener("load", function() {
    console.log(this.responseText);
    var myObj = JSON.parse(this.responseText);
});
req.open("GET", "/config");
req.send();

var req = new XMLHttpRequest();
req.onreadystatechange = function() {
    if (this.readyState == 4) {
        if (this.status == 200) {
            var response = JSON.parse(this.responseText);
            if (typeof response.name !== 'undefined') {
                message.innerHTML = "Connected to " + response.name;
            }
            if (typeof response.min !== 'undefined') {
                slider.min = response.min;
            }            
            if (typeof response.max !== 'undefined') {
                slider.max = response.max;
            }
            if ((typeof response.rate !== 'undefined') && (response.rate != 0)) {
                rate = response.rate;
            }
        }
        else {
            message.innerHTML = "Failed to get config, server replied: " + this.responseText;
        }
    }
    else {
        message.innerHTML = "Getting config...";
    }
};
req.open("GET", "/config");
req.send();

function now() {
    
}

function sendCommand() {
    var d = new Date();
    var now = d.getTime();
    if ((now - lastCommand) >= rate) {
        var req = new XMLHttpRequest();
        req.open("GET", "/command?vector=" + (run ? vector : 0), true);
        req.send();
        lastCommand = now;
    }
    else if (nextCommand <= now) {
        nextCommand = now + rate;
        setTimeout(sendCommand, rate);
    }
}

slider.oninput = function() {
    vector = this.value;
    output.innerHTML = vector;
    if (run) {
        sendCommand();
    }
}

start.onclick = function() {
    run = true;
    sendCommand();
}

stop.onclick = function() {
    run = false;
    sendCommand();
}

</script>

</body>
</html>)====";
