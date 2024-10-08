#include "html_src.h"

const char* html_form = R"=====(
<!DOCTYPE html>
<html>
<head>
  <title>NODE Setup</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      background-color: #ffffff;
      margin: 0;
      display: flex;
      justify-content: center;
      align-items: center;
      height: 100vh;
      flex-direction: column;
      overflow: hidden;
    }
    h2, h3, h4 {
      color: #000000;
      text-align: center;
      margin-bottom: 20px;
    }
    form {
      width: 350px;
      padding: 20px;
      background: #ffffff;
      border-radius: 10px;
      box-shadow: 0 4px 8px rgba(0,0,0,0.1);
      margin-bottom: 20px;
      opacity: 0;
      transform: translateY(20px);
      transition: all 0.5s ease-in-out;

    }
    form.active {
      opacity: 1;
      transform: translateY(0);
    }
    label {
      display: block;
      margin-bottom: 8px;
      font-weight: bold;
      color: #555;
    }
    input[type="text"],
    input[type="password"],
    select {
      width: 100%;
      padding: 10px;
      margin-bottom: 15px;
      border: 1px solid #ddd;
      border-radius: 4px;
      box-sizing: border-box;
      font-size: 14px;
      transition: all 0.3s;
    }
    input[type="text"]:focus,
    input[type="password"]:focus,
    select:focus {
      border-color: #4caf50;
      box-shadow: 0 0 5px rgba(76, 175, 80, 0.5);
    }
    input[type="button"] {
      background-color: #20c997;
      color: white;
      padding: 12px 20px;
      border: none;
      border-radius: 4px;
      cursor: pointer;
      font-size: 16px;
      width: 100%;
      margin-bottom: 10px;
      transition: background-color 0.3s, transform 0.2s;
    }
    input[type="button"]:hover {
      background-color: #20c997;
      transform: translateY(-2px);
    }
    #wifiList {
      list-style-type: none;
      padding: 0;
      margin: 0;
      flex: 1;
      overflow-y: auto;
    }
    #wifiList li {
      padding: 10px;
      background-color: #f9f9f9;
      margin-bottom: 5px;
      cursor: pointer;
      border-radius: 4px;
      transition: background-color 0.3s, transform 0.2s;
    }
    #wifiList li:hover {
      background-color: #e0e0e0;
      transform: translateY(-2px);
    }
    .form-step {
      display: none;
    }
    .form-step.active {
      display: block;
    }
    .fade {
      animation: fadeIn 0.5s;
    }
    @keyframes fadeIn {
      from {
        opacity: 0;
        transform: translateY(20px);
      }
      to {
        opacity: 1;
        transform: translateY(0);
      }
    }
    #selectedSSID {
      display: flex;
      font-size: medium;
      margin-bottom: 15px;
      justify-content: center;
      align-items: center;
    }
  </style>
  <script>
    function selectNetwork(ssid) {
      document.getElementById('selectedSSID').textContent = ssid;
      showStep(2);
    }

    function showStep(step) {
      document.querySelectorAll('.form-step').forEach((formStep) => {
        formStep.classList.remove('active');
        formStep.classList.remove('fade');
      });
      document.getElementById('step-' + step).classList.add('active');
      document.getElementById('step-' + step).classList.add('fade');
    }

    function connectWiFi() {
      var ssid = document.getElementById('selectedSSID').textContent;
      var password = document.getElementById('password').value;
      var api_post_ep = document.getElementById('api_post_ep').value;
      var api_key = document.getElementById('api_key').value;
      var server_type = document.getElementById('server_type').value;

      fetch('/connect', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ssid: ssid, password: password, api_post_ep: api_post_ep, api_key: api_key, server_type: server_type })
      })
      .then(response => response.text())
      .then(data => alert(data))
      .catch(error => alert('Error: ' + error.message));
    }
    function updateWiFiList() {
      fetch('/scan')
      .then(response => response.json())
      .then(data => {
        var wifiList = document.getElementById('wifiList');
        wifiList.innerHTML = '';
        data.forEach(item => {
          var listItem = document.createElement('li');
          listItem.textContent = item.ssid;
          listItem.onclick = function() { 
            selectNetwork(item.ssid);
              };
          wifiList.appendChild(listItem);
        });
      });
    }
    document.addEventListener('DOMContentLoaded', function() {
      updateWiFiList();
      setInterval(updateWiFiList, 10000);
      showStep(1);
    });
  </script>
</head>
<body>
  <form id="step-1" class="form-step active">
    <h3>Select your Wi-Fi <br>Network</h3>
    <ul id='wifiList'></ul>
  </form>
  <form id="step-2" class="form-step">
    <h3>Selected Wi-Fi <br>Network:</h3>
    <div id="selectedSSID"></div>
    <label for='password'>Password:</label>
    <input type='password' id='password' name='password' required>
    <input type='button' value='Back' onclick='showStep(1)'>
    <input type='button' value='Next' onclick='showStep(3)'> 
  </form>
  <form id="step-3" class="form-step">
    <h3>Configure Server API</h3>
    <label for='server_type'>Server Type:</label>
    <select id='server_type' name='server_type' >
      <option value='0'>Generic REST Server</option>
      <option value='1'>Firebase API</option>
    </select>
    <label for='api_post_ep'>API Post Endpoint:</label>
    <input type='text' id='api_post_ep' name='api_post_ep' required>
    <label for='api_key'>API Key:</label>
    <input type='text' id='api_key' name='api_key' required>
    <input type='button' value='Back' onclick='showStep(2)'>
    <input type='button' value='Connect' onclick='connectWiFi()'>
  </form>
</body>
</html>
)=====";