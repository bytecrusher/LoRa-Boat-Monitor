const char wificonfig_html[] PROGMEM = R"rawliteral(
  <h3 style="color:Red;">Error in Filesystem or missing Files. Please Format Filesystem and get Files from server.</h3>
  <h3>Please enter the Wifi Network informations.</h3><br>
  <form name="wifiform" action="/savesettings" method="POST">
    <input type='hidden' name='csrf' value='%csrfToken%'>
    WLAN #1 Client SSID: <input type='text' required name='cssid1' size='20' value='%cssid1%' maxlength='30' onchange='check_ssid("cssid1")'>
    <br><br>
    WLAN #1 Client Password: <input type='password' name='cpasswd1' size='20' value='' placeholder='%cpassword1Masked%' maxlength='30' onchange='check_passwd("cpasswd1")'>
    <br>
    WLAN #2 Client SSID: <input type='text' required name='cssid2' size='20' value='%cssid2%' maxlength='30' onchange='check_ssid("cssid2")'>
    <br><br>
    WLAN #2 Client Password: <input type='password' name='cpasswd2' size='20' value='' placeholder='%cpassword2Masked%' maxlength='30' onchange='check_passwd("cpasswd2")'>
    <br>
    WLAN #3 Client SSID: <input type='text' required name='cssid3' size='20' value='%cssid3%' maxlength='30' onchange='check_ssid("cssid3")'>
    <br><br>
    WLAN #3 Client Password: <input type='password' name='cpasswd3' size='20' value='' placeholder='%cpassword3Masked%' maxlength='30' onchange='check_passwd("cpasswd3")'>
    <br>
    <input id="sub" type="submit" value="Submit">
  </form>
  <br><hr>
)rawliteral";
