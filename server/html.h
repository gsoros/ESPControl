const char *html = R"====(<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">

</head>
<body>

<div style="width: 100%; height: 100vh; text-align: center">
    <h1>Stepper Control</h1>
    <input type="range" min="-1000" max="1000" value="0" class="slider" id="range" style="width: 100%; height: 20%">
    <p><span id="output"></span></p>
    <button type="button" id="start" style="width: 45%; height: 20%">Start</button>
    <button type="button" id="stop" style="width: 45%; height: 20%">Stop</button>
</div>

<script>
var slider = document.getElementById("range");
var output = document.getElementById("output");
var start = document.getElementById("start");
var stop = document.getElementById("stop");
var vector = 0;
var run = false;
output.innerHTML = slider.value;

function sendRequest() {
    var req = new XMLHttpRequest();
    req.open("GET", "/command?vector=" + (run ? vector : 0), true);
    req.send();
}

slider.oninput = function() {
    vector = this.value;
    output.innerHTML = vector;
    if (run) {
        sendRequest();
    }
}

start.onclick = function() {
    run = true;
    sendRequest();
}

stop.onclick = function() {
    run = false;
    sendRequest();
}

</script>

</body>
</html>)====";
