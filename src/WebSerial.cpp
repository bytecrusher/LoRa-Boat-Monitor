#include <WebSerial.h>
#include <Configuration.h>

extern configData actconf;

namespace {
constexpr size_t MAX_LOG_BUFFER = 12000;

const char webserial_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>WebSerial</title>
  <style>
    body { font-family: monospace; background: #111; color: #f4f4f4; margin: 0; padding: 1rem; }
    h1 { font-size: 1.1rem; margin: 0 0 1rem; }
    #log { width: 100%; height: 70vh; box-sizing: border-box; background: #000; color: #9df59d; border: 1px solid #444; padding: .75rem; overflow: auto; white-space: pre-wrap; }
    form { display: flex; gap: .5rem; margin-top: 1rem; }
    input { flex: 1; padding: .6rem; box-sizing: border-box; }
    button { padding: .6rem 1rem; }
  </style>
</head>
<body>
  <h1>WebSerial Debugger</h1>
  <div id="status" style="margin-bottom: .75rem; color: #ffd27a;"></div>
  <pre id="log"></pre>
  <form id="sendForm">
    <input id="msg" type="text" autocomplete="off" placeholder="Nachricht senden">
    <button type="submit">Senden</button>
  </form>
  <script>
    const log = document.getElementById('log');
    const status = document.getElementById('status');
    const append = (text) => {
      log.textContent += text;
      log.scrollTop = log.scrollHeight;
    };

    const events = new EventSource('/webserial/events');
    events.addEventListener('history', (event) => {
      log.textContent = event.data;
      log.scrollTop = log.scrollHeight;
    });
    events.addEventListener('log', (event) => append(event.data));
    events.addEventListener('status', (event) => {
      status.textContent = event.data;
    });

    document.getElementById('sendForm').addEventListener('submit', async (event) => {
      event.preventDefault();
      const msg = document.getElementById('msg');
      const value = msg.value.trim();
      if (!value) return;
      await fetch('/webserial/send?msg=' + encodeURIComponent(value));
      msg.value = '';
    });
  </script>
</body>
</html>
)rawliteral";
}  // namespace

void WebSerialClass::begin(AsyncWebServer *server) {
  if (initialized_ || server == nullptr) {
    return;
  }

  initialized_ = true;
  events_ = new AsyncEventSource("/webserial/events");
  events_->onConnect([this](AsyncEventSourceClient *client) {
    client->send(logBuffer_.c_str(), "history", millis());
    client->send((actconf.WebSerialDebug == 1) ? "WebSerial Debug ist aktiv." : "WebSerial Debug ist deaktiviert. Bitte in den Einstellungen aktivieren.", "status", millis());
  });

  server->addHandler(events_);

  server->on("/webserial", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1 && !request->authenticate(actconf.username, actconf.password)) {
      return request->requestAuthentication();
    }
    request->send_P(200, "text/html", webserial_html);
  });

  server->on("/webserial/send", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (actconf.crypt == 1 && !request->authenticate(actconf.username, actconf.password)) {
      return request->requestAuthentication();
    }

    if (request->hasParam("msg")) {
      String msg = request->getParam("msg")->value();
      if (callback_) {
        callback_(reinterpret_cast<uint8_t *>(msg.begin()), msg.length());
      }
      append("> " + msg + "\n");
    }
    request->send(200, "text/plain", "OK");
  });
}

void WebSerialClass::onMessage(MessageCallback callback) {
  callback_ = callback;
}

void WebSerialClass::loop() {
}

void WebSerialClass::append(const String &value) {
  logBuffer_ += value;
  if (logBuffer_.length() > MAX_LOG_BUFFER) {
    logBuffer_.remove(0, logBuffer_.length() - MAX_LOG_BUFFER);
  }

  if (events_ != nullptr) {
    events_->send(value.c_str(), "log", millis());
  }
}

size_t WebSerialClass::write(const uint8_t *buffer, size_t size) {
  if (buffer == nullptr || size == 0) {
    return 0;
  }

  String out;
  out.reserve(size);
  for (size_t i = 0; i < size; ++i) {
    out += char(buffer[i]);
  }
  append(out);
  return size;
}

size_t WebSerialClass::print(const char *value) {
  if (!value) {
    return 0;
  }
  append(String(value));
  return strlen(value);
}

size_t WebSerialClass::print(char *value) {
  return print(static_cast<const char *>(value));
}

size_t WebSerialClass::print(char value) {
  append(String(value));
  return 1;
}

size_t WebSerialClass::print(int value) {
  String out = String(value);
  append(out);
  return out.length();
}

size_t WebSerialClass::print(unsigned int value, int base) {
  String out = String(value, base);
  append(out);
  return out.length();
}

size_t WebSerialClass::print(unsigned long value) {
  String out = String(value);
  append(out);
  return out.length();
}

size_t WebSerialClass::print(float value) {
  String out = String(value);
  append(out);
  return out.length();
}

size_t WebSerialClass::print(const String &value) {
  append(value);
  return value.length();
}

size_t WebSerialClass::print(const IPAddress &value) {
  String out = value.toString();
  append(out);
  return out.length();
}

size_t WebSerialClass::println(const char *value) {
  return print(value) + print("\r\n");
}

size_t WebSerialClass::println(char *value) {
  return println(static_cast<const char *>(value));
}

size_t WebSerialClass::println(char value) {
  return print(value) + print("\r\n");
}

size_t WebSerialClass::println(int value) {
  return print(value) + print("\r\n");
}

size_t WebSerialClass::println(unsigned int value) {
  return print(value) + print("\r\n");
}

size_t WebSerialClass::println(unsigned long value) {
  return print(value) + print("\r\n");
}

size_t WebSerialClass::println(float value) {
  return print(value) + print("\r\n");
}

size_t WebSerialClass::println(const String &value) {
  return print(value) + print("\r\n");
}

size_t WebSerialClass::println(const IPAddress &value) {
  return print(value) + print("\r\n");
}

WebSerialClass WebSerial;
