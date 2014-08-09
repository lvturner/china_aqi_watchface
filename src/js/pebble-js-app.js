function fetchWeather(latitude, longitude) {
  var response;
  var req = new XMLHttpRequest();
  req.open('GET', "http://api.openweathermap.org/data/2.1/find/city?" +
           "lat=" + latitude + "&lon=" + longitude + "&cnt=1", true);
           req.onload = function(e) {
             if (req.readyState == 4) {
               if(req.status == 200) {
                 console.log(req.responseText);
                 response = JSON.parse(req.responseText);
                 var temperature, city;
                 if (response && response.list && response.list.length > 0) {
                   var weatherResult = response.list[0];
                   temperature = Math.round(weatherResult.main.temp - 273.15);
                   city = weatherResult.name;
                   localStorage.setItem("city", city);
                   localStorage.setItem("temperature", temperature);
                   fetch_pollution_data(city, temperature);
                 }
               } else {
                 fetch_pollution_data(localStorage.getItem("city"), localStorage.getItem("temperature"));
               }
             }
           };
           req.send(null);
}

function locationSuccess(pos) {
  var coordinates = pos.coords;
  localStorage.setItem("coords", pos.coords);
  fetchWeather(coordinates.latitude, coordinates.longitude);
}

function locationError(err) {
  var coords = localStorage.getItem("coords");
  if(coords === null || coords === undefined) {
    console.warn('location error (' + err.code + '): ' + err.message);
    Pebble.sendAppMessage({
      "city":"Loc Unavailable",
      "temperature":"N/A"
    });
  } else {
    fetchWeather(coords.latitude, coords.longitude);
  }
}

var locationOptions = { "timeout": 15000, "maximumAge": 60000 };

function fetch_pollution_data(city, temperature) {
  console.log("Looking up for city: "+ city.toLowerCase());
  var req = new XMLHttpRequest(),
  token = 'XAs1txvhW9Y3rmxqK4zA';
  req.open('GET', 'http://www.pm25.in/api/querys/pm2_5.json?city='+city.toLowerCase()+'&token='+token+'&stations=no', true);

  req.onload = function(e) {
    if (req.readyState == 4 && req.status == 200) {
      if (req.status == 200) {
        var response = JSON.parse(req.responseText);
        console.log('response received:');
        console.log(JSON.stringify(response));
        var aqi = response[0].aqi;
        console.log('aqi:' + aqi);
        localStorage.setItem("aqi", aqi);
        // bit silly hacking these to strings actually...
        Pebble.sendAppMessage({
          "temperature":+temperature,
          "city":city,
          "aqi":+aqi
        });
      } else {
        Pebble.sendAppMessage({
          "temperature":""+localStorage.getItem("temperature"),
          "city":localStorage.getItem("city"),
          "aqi":""+localStorage.getItem("aqi")
        });
      }
    }
  };
  req.send(null);
}

Pebble.addEventListener("ready", function(e) {
  locationWatcher = window.navigator.geolocation.watchPosition(locationSuccess, locationError, locationOptions);
});

Pebble.addEventListener("appmessage", function(e) {
  window.navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
});

Pebble.addEventListener("webviewclosed", function(e) {
  console.log("Response: " + e);
});
