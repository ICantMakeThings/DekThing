how to do:

download repo

put ssid and password in code.

flash esp.

wire it:

SCLK   13

MOSI   23

CS     15

DC     2

RST    4

VCC    3v3

GND    GND

download node.js

https://nodejs.org/en/download

cd to server folder

run `npm install`

run `node esp_proxy.mjs`

you shud see `ESP proxy running at http://localhost:5000`

if so, cool.

now install spicetify if you dont alr have it.

https://spicetify.app/docs/getting-started

once fully installed, open  `spicetify-extension.js` and change the ip to the one your esp has

(make sure to change your esp to static ip on your router)

`spicetify config-dir`

coppy the `spicetify-extension.js` file to the extentions folder that popped up.

now run
`spicetify config extensions spicetify-extension.js`
next run
`spicetify apply`

boom.


want the server to run on startup? this is the command for you:
(linux)
`@reboot /usr/bin/node /home/user/place/to/server/esp_proxy.mjs`

on windows it should be:

`schtasks /create /sc onlogon /tn "safeproxything" /tr "C:\Program Files\nodejs\node.exe C:\Users\name\place\to\scripts\esp_proxy.mjs" /rl highest`
