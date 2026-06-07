let headerOfflineTimer = 0;

function setHeaderStatus(isOnline) {
    var led = document.getElementById("myping");
    var text = document.getElementById("mypingtxt");
    if (!led || !text) {
        return;
    }

    led.classList.toggle("led-green", isOnline);
    led.classList.toggle("led-red", !isOnline);
    text.textContent = isOnline ? "Online" : "Offline";
}

function scheduleHeaderOfflineState() {
    if (headerOfflineTimer !== 0) {
        return;
    }

    headerOfflineTimer = window.setTimeout(function () {
        headerOfflineTimer = 0;
        setHeaderStatus(false);
    }, 1100);
}

function pingHeaderStatus() {
    scheduleHeaderOfflineState();

    var request = new XMLHttpRequest();
    request.onreadystatechange = function () {
        if (request.readyState !== XMLHttpRequest.DONE) {
            return;
        }

        if (request.status < 200 || request.status >= 300) {
            return;
        }

        if (headerOfflineTimer !== 0) {
          clearTimeout(headerOfflineTimer);
          headerOfflineTimer = 0;
        }
        setHeaderStatus(true);
        window.setTimeout(function () {
            var led = document.getElementById("myping");
            if (led) {
                led.classList.remove("led-green");
            }
        }, 400);
    };

    request.open("GET", "/getdata?ts=" + Date.now(), true);
    request.send();
}

window.setInterval(pingHeaderStatus, 1000);
pingHeaderStatus();
