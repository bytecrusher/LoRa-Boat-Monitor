const char wificonfig_html[] PROGMEM = R"rawliteral(
    <h3 style="color:Red;">Error in Filesystem or missing Files. Please Format Filesystem and get Files from server.</h3>
    <h3>Please enter the Wifi Network informations.</h3><br>
  <form name="wifiform" action="/savesettings" method="GET">
    WLAN Client SSID: <input type='text' required name='cssid' size='20' value='%cssid%' maxlength='20' onchange='check_ssid("cssid")'>
    <br>
    <br>
    WLAN Client Password: <input type='text' required name='cpasswd' size='20' value='%cpassword%' maxlength='20' onchange='check_passwd("cpasswd")'>
    <br>
    <input id="sub" type="submit" value="Submit">
  </form>
  <br><hr>
  )rawliteral";
