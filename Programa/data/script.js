// Complete project details: https://randomnerdtutorials.com/esp32-plot-readings-charts-multiple/

// Get current sensor readings when the page loads
window.addEventListener('load', getReadings);
const ip = '192.168.0.190';
const baseUrl = `/get?`;


//Create Temperature Chart

Highcharts.setOptions({
  time: {
    useUTC: false,
    timezoneOffset: new Date().getTimezoneOffset()
  }
});

var chartT = new Highcharts.Chart({
  chart:{
    renderTo:'chart-temperature'
	
  },
  series: [
    {
      name: 'Controller',
      type: 'line',
      color: '#101D42',
      marker: {
        symbol: 'circle',
        radius: 3,
        fillColor: '#101D42',
      }
    },
    {
      name: 'Position',
      type: 'line',
      color: '#00A6A6',
      marker: {
        symbol: 'square',
        radius: 3,
        fillColor: '#00A6A6',
      }
    },
    {
      name: 'Flow',
      type: 'line',
      color: '#8B2635',
      marker: {
        symbol: 'triangle',
        radius: 3,
        fillColor: '#8B2635',
      }
    },
    {
      name: 'SetPoint',
      type: 'line',
      color: '#71B48D',
      marker: {
        symbol: 'triangle-down',
        radius: 3,
        fillColor: '#71B48D',
      }
    },
  ],
  title: {
    text: undefined
  },
  xAxis: {
    type: 'datetime',
    dateTimeLabelFormats: { second: '%H:%M:%S' }
  },
  yAxis: {
    title: {
      text: 'Processo'
    }
  },
  credits: {
    enabled: false
  }
});


//Plot temperature in the temperature chart
function plotTemperature(jsonValue) {

  var keys = Object.keys(jsonValue);
  //console.log(keys);
  //console.log(keys.length);

  for (var i = 0; i < keys.length; i++){
    var x = (new Date()).getTime();
    //console.log(x);
    const key = keys[i];
    var y = Number(jsonValue[key]);
    //console.log(y);

    if(chartT.series[i].data.length > 40) {
      chartT.series[i].addPoint([x, y], true, true, true);
    } else {
      chartT.series[i].addPoint([x, y], true, false, true);
    }

  }
}

// Function to get current readings on the webpage when it loads for the first time
function getReadings(){
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      var myObj = JSON.parse(this.responseText);
      //console.log(myObj);
	  loadvalues (myObj);
      plotTemperature(myObj);
    }
  };
  xhr.open("GET", "/readings", true);
  xhr.send();
}

function loadvalues (obj){
	//var setpoint = obj["sensor4"];
	document.getElementById('SetPoint').innerHTML= `SetPoint (L/h) ${obj["sensor4"].toFixed(2)}`;
	document.getElementById('AbsPos').innerHTML= `Pos Absoluta (%) ${obj["sensor2"].toFixed(2)}`;
	document.getElementById('RelativePos').innerHTML= `Pos Relativa (%) ${obj["sensor2"].toFixed(2)}`;
	document.getElementById('KpValue').innerHTML= `KP ${obj["sensor5"].toFixed(5)}`;
	document.getElementById('KiValue').innerHTML= `KI ${obj["sensor6"].toFixed(5)}`;
	document.getElementById('KdValue').innerHTML= `KD ${obj["sensor7"].toFixed(5)}`;
	//console.log(obj["sensor1"]);
}

function Send(name){
	let value = document.getElementsByName(name)[0].value;
	let urlFinal = `${baseUrl}${name}=${value}`;
	const response = fetch(urlFinal);
	console.log(response);
}

if (!!window.EventSource) {
  var source = new EventSource('/events');

  source.addEventListener('open', function(e) {
    //console.log("Events Connected");
  }, false);

  source.addEventListener('error', function(e) {
    if (e.target.readyState != EventSource.OPEN) {
      //console.log("Events Disconnected");
    }
  }, false);

  source.addEventListener('message', function(e) {
    //console.log("message", e.data);
  }, false);

  source.addEventListener('new_readings', function(e) {
    //console.log("new_readings", e.data);
    var myObj = JSON.parse(e.data);
    //console.log(myObj);
    loadvalues (myObj);
	plotTemperature(myObj);
  }, false);
}
